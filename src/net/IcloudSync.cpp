#include "net/IcloudSync.h"
#include "net/Http.h"

#include <algorithm>
#include <cctype>

namespace agenda {
namespace IcloudSync {

static const char* BASE = "https://caldav.icloud.com";

// scheme://host de una URL absoluta.
static std::string hostRoot(const std::string& url) {
    size_t p = url.find("://");
    if (p == std::string::npos) return BASE;
    size_t slash = url.find('/', p + 3);
    return (slash == std::string::npos) ? url : url.substr(0, slash);
}

// Resuelve 'href' (absoluto o relativo) contra una URL base absoluta.
static std::string resolver(const std::string& base, const std::string& href) {
    if (href.rfind("http", 0) == 0) return href;
    if (!href.empty() && href[0] == '/') return hostRoot(base) + href;
    // relativo a la carpeta de base
    size_t slash = base.find_last_of('/');
    std::string dir = (slash == std::string::npos) ? base : base.substr(0, slash + 1);
    return dir + href;
}

static std::string minus(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    return s;
}

// Primer <...href>VALOR</...href> que aparezca despues de 'tagLocal' (ignora prefijos/ns).
static std::string primerHrefTras(const std::string& xml, const std::string& tagLocal) {
    std::string low = minus(xml);
    size_t start = low.find(minus(tagLocal));
    if (start == std::string::npos) start = 0;
    size_t h = low.find("href", start);
    if (h == std::string::npos) return "";
    size_t gt = low.find('>', h);
    if (gt == std::string::npos) return "";
    size_t lt = low.find('<', gt);
    if (lt == std::string::npos) return "";
    std::string val = xml.substr(gt + 1, lt - gt - 1);
    // trim
    size_t a = val.find_first_not_of(" \r\n\t");
    size_t b = val.find_last_not_of(" \r\n\t");
    return (a == std::string::npos) ? "" : val.substr(a, b - a + 1);
}

static HttpResponse propfind(const std::string& url, const IcloudCfg& cfg,
                             const std::string& depth, const std::string& body) {
    HttpRequest req;
    req.method = "PROPFIND";
    req.url = url;
    req.depth = depth;
    req.headers = { "Content-Type: application/xml; charset=utf-8" };
    req.basicUser = cfg.appleId;
    req.basicPass = cfg.appPassword;
    req.body = body;
    return httpRequest(req);
}

// Genera el VTODO (igual que tareaAVTodo del original).
static std::string construirVTodo(const Tarea& tarea) {
    std::string fecha = tarea.fechaLimite;
    fecha.erase(std::remove(fecha.begin(), fecha.end(), '-'), fecha.end());  // YYYYMMDD
    std::string notas = tarea.notas;
    // Escapar saltos de linea para iCalendar.
    std::string desc;
    for (char c : notas) { if (c == '\n') desc += "\\n"; else if (c == '\r') {} else desc += c; }

    std::string estado = tarea.completada ? "COMPLETED" : "NEEDS-ACTION";
    std::string s;
    s += "BEGIN:VCALENDAR\r\n";
    s += "VERSION:2.0\r\n";
    s += "PRODID:-//Agenda Personal//ES\r\n";
    s += "BEGIN:VTODO\r\n";
    s += "UID:" + tarea.id + "@agenda-personal\r\n";
    s += "SUMMARY:" + tarea.titulo + "\r\n";
    s += "DUE;VALUE=DATE:" + fecha + "\r\n";
    s += "DESCRIPTION:" + desc + "\r\n";
    s += "STATUS:" + estado + "\r\n";
    s += "END:VTODO\r\n";
    s += "END:VCALENDAR\r\n";
    return s;
}

// Descubre la URL de la lista de recordatorios (calendario que soporta VTODO).
static std::string descubrirCalendario(const IcloudCfg& cfg, std::string& err) {
    // 1) current-user-principal
    HttpResponse r = propfind(std::string(BASE) + "/", cfg, "0",
        "<d:propfind xmlns:d='DAV:'><d:prop><d:current-user-principal/></d:prop></d:propfind>");
    if (!r.error.empty()) { err = "iCloud: error de red: " + r.error; return ""; }
    if (r.status == 401) { err = "iCloud: Apple ID o contraseña de aplicación incorrectos."; return ""; }
    std::string principalHref = primerHrefTras(r.body, "current-user-principal");
    if (principalHref.empty()) { err = "iCloud: no se encontro el principal del usuario."; return ""; }
    std::string principalUrl = resolver(std::string(BASE) + "/", principalHref);

    // 2) calendar-home-set
    r = propfind(principalUrl, cfg, "0",
        "<d:propfind xmlns:d='DAV:' xmlns:c='urn:ietf:params:xml:ns:caldav'>"
        "<d:prop><c:calendar-home-set/></d:prop></d:propfind>");
    if (!r.error.empty()) { err = "iCloud: error de red: " + r.error; return ""; }
    std::string homeHref = primerHrefTras(r.body, "calendar-home-set");
    if (homeHref.empty()) { err = "iCloud: no se encontro la carpeta de calendarios."; return ""; }
    std::string homeUrl = resolver(principalUrl, homeHref);

    // 3) listar calendarios y elegir el que soporta VTODO
    r = propfind(homeUrl, cfg, "1",
        "<d:propfind xmlns:d='DAV:' xmlns:c='urn:ietf:params:xml:ns:caldav'>"
        "<d:prop><d:resourcetype/><c:supported-calendar-component-set/></d:prop></d:propfind>");
    if (!r.error.empty()) { err = "iCloud: error de red: " + r.error; return ""; }

    // Partir en bloques <response> y buscar uno con VTODO.
    std::string low = minus(r.body);
    std::string elegido, primero;
    size_t pos = 0;
    while (true) {
        size_t ini = low.find("response", pos);
        if (ini == std::string::npos) break;
        size_t fin = low.find("response", ini + 8);
        size_t blockEnd = (fin == std::string::npos) ? r.body.size() : fin;
        std::string bloque = r.body.substr(ini, blockEnd - ini);
        std::string href = primerHrefTras(bloque, "href");
        std::string bloqueLow = minus(bloque);
        if (!href.empty()) {
            if (primero.empty() && bloqueLow.find("calendar") != std::string::npos) primero = href;
            if (bloqueLow.find("vtodo") != std::string::npos) { elegido = href; break; }
        }
        pos = blockEnd;
        if (fin == std::string::npos) break;
    }
    std::string calHref = !elegido.empty() ? elegido : primero;
    if (calHref.empty()) { err = "iCloud: no se encontro una lista de Recordatorios."; return ""; }
    return resolver(homeUrl, calHref);
}

std::string sincronizarTarea(const IcloudCfg& cfg, const Tarea& tarea) {
    if (!cfg.activo || cfg.appleId.empty()) return "";
    if (cfg.appPassword.empty())
        return "Falta la contraseña de aplicación de iCloud en Configuración.";

    std::string err;
    std::string calUrl = descubrirCalendario(cfg, err);
    if (!err.empty()) return err;

    std::string objetoUrl = calUrl;
    if (!objetoUrl.empty() && objetoUrl.back() != '/') objetoUrl += '/';
    objetoUrl += tarea.id + ".ics";

    HttpRequest put;
    put.method = "PUT";
    put.url = objetoUrl;
    put.headers = { "Content-Type: text/calendar; charset=utf-8" };
    put.basicUser = cfg.appleId;
    put.basicPass = cfg.appPassword;
    put.body = construirVTodo(tarea);
    HttpResponse r = httpRequest(put);
    if (!r.error.empty()) return "iCloud: error de red: " + r.error;
    if (!r.ok()) return "iCloud: el servidor respondio " + std::to_string(r.status) + " al guardar el recordatorio.";
    return "";
}

} // namespace IcloudSync
} // namespace agenda
