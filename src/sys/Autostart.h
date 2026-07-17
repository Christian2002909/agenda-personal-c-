#pragma once
// Iniciar (o no) con Windows: valor en HKCU\...\Run apuntando al .exe actual.

namespace agenda {

void establecerAutostart(bool activar);
bool obtenerAutostart();

} // namespace agenda
