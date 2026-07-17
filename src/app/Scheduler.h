#pragma once
// Motor de avisos: replica scheduler.js + la re-insistencia de main.js del
// proyecto original. Corre en un hilo de fondo revisando cada 30 s.

#include "app/Model.h"
#include "app/Store.h"

#include <functional>
#include <thread>
#include <atomic>
#include <string>
#include <vector>

namespace agenda {

struct AvisoPendiente {
    Tarea       tarea;
    int         dias = 0;
    std::string hora;
    std::string clave;
};

// Calcula los avisos que deben dispararse en 'ahoraMs' (epoch ms local corregido).
// Expuesto para poder validarlo de forma aislada.
std::vector<AvisoPendiente> calcularAvisosPendientes(
    const std::vector<Tarea>& tareas,
    const std::unordered_map<std::string, int64_t>& ultimosAvisos,
    int64_t ahoraMs);

bool tareaEstaVencida(const Tarea& tarea, int64_t ahoraMs);

class Scheduler {
public:
    // Callbacks (se invocan desde el hilo del scheduler).
    std::function<void(const std::wstring& titulo, const std::wstring& cuerpo)> onNotificar;
    std::function<void(const Tarea&, int dias)> onCorreo;
    std::function<void(const Tarea&)> onGoogleResync;

    void start(Store* store);
    void stop();
    ~Scheduler();

private:
    void loop();
    void revisar();

    Store* store_ = nullptr;
    std::thread th_;
    std::atomic<bool> corriendo_{false};
};

} // namespace agenda
