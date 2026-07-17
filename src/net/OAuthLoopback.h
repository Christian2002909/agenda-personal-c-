#pragma once
// Servidor HTTP minimo en 127.0.0.1:53682 que captura el ?code= del redirect de
// OAuth de Google (equivale al http.createServer de google-sync.js).

#include <string>

namespace agenda {

// Espera el redirect y devuelve el "code" de autorizacion, o cadena vacia si
// falla o expira el tiempo. Bloqueante (llamar desde un hilo de trabajo).
std::string oauthEsperarCodigo(int timeoutSeg = 180);

} // namespace agenda
