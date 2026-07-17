#include "net/GoogleSync.h"
#include "net/Http.h"
#include "net/OAuthLoopback.h"
#include "app/Util.h"

#include <windows.h>
#include <shellapi.h>
#include <nlohmann/json.hpp>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <algorithm>

using nlohmann::json;

namespace agenda {
namespace GoogleSync {

static const char* REDIRECT_URI = "http://127.0.0.1:53682/oauth2callback";
static const char* TOKEN_URL    = "https://oauth2.googleapis.com/token";
static const char* EVENTS_URL   = "https://www.googleapis.com/calendar/v3/calendars/primary/events";

// Desfase horario local respecto a UTC, formateado "+HH:MM" / "-HH:MM".
static std::string offsetLocal() {
    std::time_t t = std::time(nullptr);
    std::tm g{}, l{};
#if defined(_WIN32)
    gmtime_s(&g, &t); localtime_s(&l, &t);
#else
    gmtime_r(&t, &g); localtime_r(&t, &l);
#endif
    // offset (local - UTC) en segundos, con el idioma estandar mktime/difftime.
    double diff = std::difftime(std::mktime(&l), std::mktime(&g));
    int mins = static_cast<int>(diff / 60.0);
    char sign = mins >= 0 ? '+' : '-';
    int a = std::abs(mins);
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%c%02d:%02d", sign, a / 60, a % 60);
    return buf;
}

static std::string sumarMinutos(const std::string& hm, int mins) {
    int h = 0, m = 0; std::sscanf(hm.c_str(), "%d:%d", &h, &m);
    int total = h * 60 + m + mins;
    total = ((total % 1440) + 1440) % 1440;
    char buf[8]; std::snprintf(buf, sizeof(buf), "%02d:%02d", total / 60, total % 60);
    return buf;
}

// Refresca el access token si vencio (o si nunca se obtuvo). Devuelve "" o error.
static std::string asegurarToken(GoogleCfg& cfg) {
    if (!cfg.tokens.valido || cfg.tokens.refreshToken.empty())
        return "No estas conectado con Google. Pulsa \"Conectar con Google\".";
    // Margen de 60 s.
    if (cfg.tokens.expiryEpochMs > nowEpochMs() + 60000 && !cfg.tokens.accessToken.empty())
        return "";

    HttpRequest req;
    req.method = "POST";
    req.url = TOKEN_URL;
    req.headers = { "Content-Type: application/x-www-form-urlencoded" };
    req.body =
        "client_id=" + urlEncode(cfg.clientId) +
        "&client_secret=" + urlEncode(cfg.clientSecret) +
        "&refresh_token=" + urlEncode(cfg.tokens.refreshToken) +
        "&grant_type=refresh_token";
    HttpResponse r = httpRequest(req);
    if (!r.error.empty()) return "Google: error de red: " + r.error;
    if (!r.ok()) return "Google: no se pudo refrescar el acceso (" + std::to_string(r.status) + ").";
    try {
        json j = json::parse(r.body);
        cfg.tokens.accessToken = j.value("access_token", "");
        int expiresIn = j.value("expires_in", 3600);
        cfg.tokens.expiryEpochMs = nowEpochMs() + (int64_t)expiresIn * 1000;
        cfg.tokens.valido = !cfg.tokens.accessToken.empty();
    } catch (...) { return "Google: respuesta de token invalida."; }
    return "";
}

std::string autenticar(GoogleCfg& cfg) {
    if (cfg.clientId.empty() || cfg.clientSecret.empty())
        return "Falta el Client ID / Client Secret de Google en Configuración.";

    std::string authUrl =
        "https://accounts.google.com/o/oauth2/v2/auth"
        "?client_id=" + urlEncode(cfg.clientId) +
        "&redirect_uri=" + urlEncode(REDIRECT_URI) +
        "&response_type=code"
        "&access_type=offline&prompt=consent"
        "&scope=" + urlEncode("https://www.googleapis.com/auth/calendar.events");

    ShellExecuteW(nullptr, L"open", widen(authUrl).c_str(), nullptr, nullptr, SW_SHOWNORMAL);

    std::string code = oauthEsperarCodigo(180);
    if (code.empty())
        return "No se recibio la autorizacion de Google (se agoto el tiempo o se cancelo).";

    HttpRequest req;
    req.method = "POST";
    req.url = TOKEN_URL;
    req.headers = { "Content-Type: application/x-www-form-urlencoded" };
    req.body =
        "code=" + urlEncode(code) +
        "&client_id=" + urlEncode(cfg.clientId) +
        "&client_secret=" + urlEncode(cfg.clientSecret) +
        "&redirect_uri=" + urlEncode(REDIRECT_URI) +
        "&grant_type=authorization_code";
    HttpResponse r = httpRequest(req);
    if (!r.error.empty()) return "Google: error de red: " + r.error;
    if (!r.ok()) return "Google: no se pudieron obtener los tokens (" + std::to_string(r.status) + ").";
    try {
        json j = json::parse(r.body);
        cfg.tokens.accessToken  = j.value("access_token", "");
        cfg.tokens.refreshToken = j.value("refresh_token", cfg.tokens.refreshToken);
        int expiresIn = j.value("expires_in", 3600);
        cfg.tokens.expiryEpochMs = nowEpochMs() + (int64_t)expiresIn * 1000;
        cfg.tokens.valido = !cfg.tokens.accessToken.empty();
        cfg.activo = true;
    } catch (...) { return "Google: respuesta de autenticacion invalida."; }
    return "";
}

static std::string fechaMasUnDia(const std::string& ymd) {
    int Y=0,M=0,D=0; std::sscanf(ymd.c_str(), "%d-%d-%d", &Y,&M,&D);
    std::tm t{}; t.tm_year=Y-1900; t.tm_mon=M-1; t.tm_mday=D; t.tm_hour=12; t.tm_isdst=-1;
    std::time_t tt = std::mktime(&t) + 24*60*60;
    std::tm o{};
#if defined(_WIN32)
    localtime_s(&o,&tt);
#else
    localtime_r(&tt,&o);
#endif
    char buf[16]; std::strftime(buf,sizeof(buf),"%Y-%m-%d",&o); return buf;
}

static json construirEvento(const Tarea& tarea) {
    std::vector<std::string> horarios = tarea.horarios;
    std::sort(horarios.begin(), horarios.end());
    bool usaHora = !horarios.empty();

    json ev;
    ev["summary"] = tarea.titulo;
    std::string desc = tarea.notas;
    if (horarios.size() > 1) {
        desc += "\n\nOtros horarios de aviso en Agenda Personal: ";
        for (size_t i = 1; i < horarios.size(); ++i) { if (i>1) desc += ", "; desc += horarios[i]; }
    }
    ev["description"] = desc;

    if (usaHora) {
        std::string off = offsetLocal();
        std::string ini = tarea.fechaLimite + "T" + horarios[0] + ":00" + off;
        std::string fin = tarea.fechaLimite + "T" + sumarMinutos(horarios[0], 30) + ":00" + off;
        ev["start"] = { {"dateTime", ini} };
        ev["end"]   = { {"dateTime", fin} };
    } else {
        ev["start"] = { {"date", tarea.fechaLimite} };
        ev["end"]   = { {"date", fechaMasUnDia(tarea.fechaLimite)} };  // fin exclusivo (requisito de Google)
    }

    json overrides = json::array();
    std::vector<int> dias = tarea.avisosPrevios.empty() ? std::vector<int>{0} : tarea.avisosPrevios;
    for (int d : dias) overrides.push_back({ {"method","popup"}, {"minutes", d*24*60} });
    ev["reminders"] = { {"useDefault", false}, {"overrides", overrides} };
    return ev;
}

std::string crearOActualizarEvento(GoogleCfg& cfg, const Tarea& tarea, std::string& eventIdOut) {
    eventIdOut = tarea.googleEventId;
    if (!cfg.activo || !cfg.tokens.valido) return "";
    std::string err = asegurarToken(cfg);
    if (!err.empty()) return err;

    json ev = construirEvento(tarea);
    HttpRequest req;
    req.headers = {
        "Authorization: Bearer " + cfg.tokens.accessToken,
        "Content-Type: application/json"
    };
    req.body = ev.dump();
    if (!tarea.googleEventId.empty()) {
        req.method = "PUT";
        req.url = std::string(EVENTS_URL) + "/" + urlEncode(tarea.googleEventId);
    } else {
        req.method = "POST";
        req.url = EVENTS_URL;
    }
    HttpResponse r = httpRequest(req);
    if (!r.error.empty()) return "Google Calendar: error de red: " + r.error;
    if (!r.ok()) return "Google Calendar: el servidor respondio " + std::to_string(r.status) + ".";
    try {
        json j = json::parse(r.body);
        eventIdOut = j.value("id", tarea.googleEventId);
    } catch (...) {}
    return "";
}

std::string eliminarEvento(GoogleCfg& cfg, const Tarea& tarea) {
    if (!cfg.activo || !cfg.tokens.valido || tarea.googleEventId.empty()) return "";
    std::string err = asegurarToken(cfg);
    if (!err.empty()) return err;

    HttpRequest req;
    req.method = "DELETE";
    req.url = std::string(EVENTS_URL) + "/" + urlEncode(tarea.googleEventId);
    req.headers = { "Authorization: Bearer " + cfg.tokens.accessToken };
    HttpResponse r = httpRequest(req);
    if (!r.error.empty()) return "Google Calendar: error de red: " + r.error;
    // 404/410 = el evento ya no existe; no es error real.
    if (!r.ok() && r.status != 404 && r.status != 410)
        return "Google Calendar: no se pudo eliminar (" + std::to_string(r.status) + ").";
    return "";
}

} // namespace GoogleSync
} // namespace agenda
