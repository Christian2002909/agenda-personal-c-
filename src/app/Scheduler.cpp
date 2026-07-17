#include "app/Scheduler.h"
#include "app/Util.h"

#include <ctime>
#include <cstdio>
#include <chrono>

namespace agenda {

static const int64_t DIA_MS           = 24LL * 60 * 60 * 1000;
static const int64_t NAG_VENTANA_MS   = 5LL  * 60 * 1000;
static const int64_t NAG_CALENDARIO_MS= 10LL * 60 * 1000;

// "YYYY-MM-DD" + "HH:MM" -> epoch ms en horario local.
static int64_t combinarFechaHoraMs(const std::string& ymd, const std::string& hm) {
    int Y=0,M=0,D=0,h=0,m=0;
    std::sscanf(ymd.c_str(), "%d-%d-%d", &Y, &M, &D);
    std::sscanf(hm.c_str(),  "%d:%d", &h, &m);
    std::tm t{};
    t.tm_year = Y - 1900; t.tm_mon = M - 1; t.tm_mday = D;
    t.tm_hour = h; t.tm_min = m; t.tm_sec = 0; t.tm_isdst = -1;
    std::time_t tt = std::mktime(&t);
    return static_cast<int64_t>(tt) * 1000;
}

// "YYYY-MM-DD" menos 'dias' -> "YYYY-MM-DD".
static std::string fechaConDiasRestados(const std::string& ymd, int dias) {
    int Y=0,M=0,D=0;
    std::sscanf(ymd.c_str(), "%d-%d-%d", &Y, &M, &D);
    std::tm t{};
    t.tm_year = Y - 1900; t.tm_mon = M - 1; t.tm_mday = D;
    t.tm_hour = 12; t.tm_sec = 0; t.tm_isdst = -1;  // mediodia: evita saltos por DST
    std::time_t tt = std::mktime(&t);
    tt -= static_cast<std::time_t>(dias) * 24 * 60 * 60;
    std::tm out{};
#if defined(_WIN32)
    localtime_s(&out, &tt);
#else
    localtime_r(&tt, &out);
#endif
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", &out);
    return std::string(buf);
}

std::vector<AvisoPendiente> calcularAvisosPendientes(
    const std::vector<Tarea>& tareas,
    const std::unordered_map<std::string, int64_t>& ultimosAvisos,
    int64_t ahoraMs) {

    std::vector<AvisoPendiente> pendientes;
    for (const auto& tarea : tareas) {
        if (tarea.completada || tarea.eliminada) continue;
        std::vector<int> dias = tarea.avisosPrevios.empty() ? std::vector<int>{0} : tarea.avisosPrevios;
        std::vector<std::string> horas = tarea.horarios.empty() ? std::vector<std::string>{"09:00"} : tarea.horarios;

        for (int d : dias) {
            std::string fechaBase = fechaConDiasRestados(tarea.fechaLimite, d);
            for (const auto& hora : horas) {
                int64_t objetivoMs = combinarFechaHoraMs(fechaBase, hora);
                std::string clave = tarea.id + "|" + std::to_string(d) + "|" + hora + "|" + fechaBase;
                bool yaDisparado = ultimosAvisos.count(clave) > 0;
                int64_t diff = ahoraMs - objetivoMs;
                if (!yaDisparado && diff >= 0 && diff < DIA_MS) {
                    pendientes.push_back({ tarea, d, hora, clave });
                }
            }
        }
    }
    return pendientes;
}

bool tareaEstaVencida(const Tarea& tarea, int64_t ahoraMs) {
    std::vector<int> dias = tarea.avisosPrevios.empty() ? std::vector<int>{0} : tarea.avisosPrevios;
    std::vector<std::string> horas = tarea.horarios.empty() ? std::vector<std::string>{"09:00"} : tarea.horarios;
    for (int d : dias) {
        std::string fechaBase = fechaConDiasRestados(tarea.fechaLimite, d);
        for (const auto& hora : horas) {
            if (ahoraMs - combinarFechaHoraMs(fechaBase, hora) >= 0) return true;
        }
    }
    return false;
}

void Scheduler::start(Store* store) {
    store_ = store;
    corriendo_ = true;
    th_ = std::thread([this] { loop(); });
}

void Scheduler::stop() {
    corriendo_ = false;
    if (th_.joinable()) th_.join();
}

Scheduler::~Scheduler() { stop(); }

void Scheduler::loop() {
    // Primera revision inmediata, luego cada 30 s (en tramos cortos para poder salir rapido).
    revisar();
    int acumulado = 0;
    while (corriendo_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        acumulado += 500;
        if (acumulado >= 30000) { acumulado = 0; if (corriendo_) revisar(); }
    }
}

void Scheduler::revisar() {
    if (!store_) return;
    Config cfg = store_->getConfig();
    std::vector<Tarea> tareas = store_->getTareas();
    auto ultimos = store_->getUltimosAvisos();

    int64_t correccionMs = static_cast<int64_t>(cfg.correccionHorariaMin) * 60 * 1000;
    int64_t ahoraMs = nowEpochMs() + correccionMs;

    // Avisos puntuales.
    auto pendientes = calcularAvisosPendientes(tareas, ultimos, ahoraMs);
    for (const auto& p : pendientes) {
        NotifCfg canales = cfg.notificaciones;
        std::string texto = p.dias > 0
            ? "Vence en " + std::to_string(p.dias) + " dia(s): " + p.tarea.fechaLimite
            : "Vence hoy (" + p.tarea.fechaLimite + ")";
        if (canales.ventana && onNotificar) onNotificar(widen(p.tarea.titulo), widen(texto));
        if (canales.correo  && onCorreo)    onCorreo(p.tarea, p.dias);
        store_->marcarAvisoDisparado(p.clave);
        ultimos[p.clave] = nowEpochMs();
    }

    // Re-insistencia: notificacion cada 5 min y re-sync de Google cada 10 min.
    NotifCfg canales = cfg.notificaciones;
    for (const auto& tarea : tareas) {
        if (tarea.completada || tarea.eliminada) continue;
        if (!tareaEstaVencida(tarea, ahoraMs)) continue;

        auto debeRenagar = [&](const std::string& clave, int64_t intervalo) -> bool {
            auto it = ultimos.find(clave);
            if (it == ultimos.end() || ahoraMs - it->second >= intervalo) {
                store_->marcarAvisoDisparado(clave);
                ultimos[clave] = nowEpochMs();
                return true;
            }
            return false;
        };

        if (canales.ventana && debeRenagar("nagVentana:" + tarea.id, NAG_VENTANA_MS)) {
            if (onNotificar) onNotificar(widen(tarea.titulo),
                                         widen("Vence el " + tarea.fechaLimite + " y sigue pendiente"));
        }
        if (cfg.googleCalendar.activo && debeRenagar("nagCalendario:" + tarea.id, NAG_CALENDARIO_MS)) {
            if (onGoogleResync) onGoogleResync(tarea);
        }
    }
}

} // namespace agenda
