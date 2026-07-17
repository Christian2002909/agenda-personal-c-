#include "sys/Tray.h"

#include <shellapi.h>
#include <cstring>

namespace agenda {

bool Tray::init(HWND hwnd, UINT callbackMsg, HICON icono) {
    ZeroMemory(&nid_, sizeof(nid_));
    nid_.cbSize = sizeof(nid_);
    nid_.hWnd = hwnd;
    nid_.uID = 1;
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid_.uCallbackMessage = callbackMsg;
    nid_.hIcon = icono ? icono : LoadIcon(nullptr, IDI_APPLICATION);
    wcscpy_s(nid_.szTip, L"Agenda Personal");
    activo_ = Shell_NotifyIconW(NIM_ADD, &nid_) == TRUE;
    return activo_;
}

void Tray::mostrarGlobo(const std::wstring& titulo, const std::wstring& cuerpo) {
    if (!activo_) return;
    nid_.uFlags = NIF_INFO;
    nid_.dwInfoFlags = NIIF_INFO;
    wcsncpy_s(nid_.szInfoTitle, titulo.c_str(), _TRUNCATE);
    wcsncpy_s(nid_.szInfo, cuerpo.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
    // Restaurar flags base para siguientes modificaciones.
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
}

void Tray::quitar() {
    if (activo_) {
        Shell_NotifyIconW(NIM_DELETE, &nid_);
        activo_ = false;
    }
}

Tray::~Tray() { quitar(); }

} // namespace agenda
