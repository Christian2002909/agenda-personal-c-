#include "sys/SingleInstance.h"

namespace agenda {

const wchar_t* kWindowClass = L"AgendaPersonalWindowClass";
static const wchar_t* kMutexName = L"Global\\AgendaPersonalSingleInstanceMutex";

bool SingleInstance::adquirir() {
    mutex_ = CreateMutexW(nullptr, TRUE, kMutexName);
    if (!mutex_) return true; // si no se pudo crear, no bloqueamos la app
    return GetLastError() != ERROR_ALREADY_EXISTS;
}

SingleInstance::~SingleInstance() {
    if (mutex_) { ReleaseMutex(mutex_); CloseHandle(mutex_); }
}

void activarVentanaExistente() {
    HWND h = FindWindowW(kWindowClass, nullptr);
    if (h) {
        if (IsIconic(h)) ShowWindow(h, SW_RESTORE);
        ShowWindow(h, SW_SHOW);
        SetForegroundWindow(h);
    }
}

} // namespace agenda
