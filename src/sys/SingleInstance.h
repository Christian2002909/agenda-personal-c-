#pragma once
// Garantiza una sola instancia de la app (como app.requestSingleInstanceLock del
// original). Si ya hay una corriendo, se puede pedir que muestre su ventana.

#include <windows.h>

namespace agenda {

// Nombre de la clase de ventana usado para localizar la instancia existente.
extern const wchar_t* kWindowClass;

class SingleInstance {
public:
    // Devuelve true si esta es la primera instancia. false si ya habia otra.
    bool adquirir();
    ~SingleInstance();
private:
    HANDLE mutex_ = nullptr;
};

// Trae al frente la ventana de la instancia que ya estaba corriendo.
void activarVentanaExistente();

} // namespace agenda
