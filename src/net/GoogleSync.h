#pragma once
// Sincronizacion con Google Calendar (equivale a electron/google-sync.js).
// Flujo OAuth de escritorio + crear/actualizar/eliminar eventos.

#include "app/Model.h"
#include <string>

namespace agenda {

namespace GoogleSync {

// Abre el navegador, espera el consentimiento y obtiene tokens. Modifica cfg
// (tokens + activo). Devuelve "" si todo bien, o un mensaje de error.
std::string autenticar(GoogleCfg& cfg);

// Crea o actualiza el evento de la tarea. Devuelve el eventId en 'eventIdOut'.
// Devuelve "" si todo bien, o un mensaje de error.
std::string crearOActualizarEvento(GoogleCfg& cfg, const Tarea& tarea, std::string& eventIdOut);

// Elimina el evento de la tarea (al completarla/eliminarla).
std::string eliminarEvento(GoogleCfg& cfg, const Tarea& tarea);

} // namespace GoogleSync
} // namespace agenda
