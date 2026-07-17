#pragma once
// Icono en la bandeja del sistema con globo de notificacion (equivale a Tray +
// Notification de Electron).

#include <windows.h>
#include <shellapi.h>
#include <string>

namespace agenda {

class Tray {
public:
    // callbackMsg: mensaje que Windows envia a la ventana ante eventos del icono.
    bool init(HWND hwnd, UINT callbackMsg, HICON icono);
    void mostrarGlobo(const std::wstring& titulo, const std::wstring& cuerpo);
    void quitar();
    ~Tray();
private:
    NOTIFYICONDATAW nid_{};
    bool activo_ = false;
};

} // namespace agenda
