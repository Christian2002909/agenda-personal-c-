#pragma once
// Sincronizacion con Apple Reminders / iCloud vía CalDAV (equivale a
// electron/icloud-sync.js, que usa la libreria tsdav). Es la integracion mas
// fragil: iCloud descubre el servidor por PROPFIND y redirige a un host por
// usuario. Se hace un descubrimiento best-effort y luego un PUT del VTODO.

#include "app/Model.h"
#include <string>

namespace agenda {

namespace IcloudSync {

// Crea o actualiza el recordatorio (VTODO) de la tarea. Devuelve "" si todo bien
// o un mensaje de error en español.
std::string sincronizarTarea(const IcloudCfg& cfg, const Tarea& tarea);

} // namespace IcloudSync
} // namespace agenda
