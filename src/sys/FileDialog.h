#pragma once
// Dialogo para elegir una imagen de fondo (equivale a dialog.showOpenDialog).

#include <windows.h>
#include <string>

namespace agenda {

// Devuelve la ruta elegida o cadena vacia si se cancela.
std::wstring elegirImagen(HWND owner);

} // namespace agenda
