#include "sys/Autostart.h"

#include <windows.h>
#include <string>

namespace agenda {

static const wchar_t* kRunKey  = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t* kValueNm = L"Agenda Personal";

static std::wstring exePathQuoted() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return L"\"" + std::wstring(buf, n) + L"\"";
}

void establecerAutostart(bool activar) {
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
        return;
    if (activar) {
        std::wstring cmd = exePathQuoted();
        RegSetValueExW(key, kValueNm, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(cmd.c_str()),
                       static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(key, kValueNm);
    }
    RegCloseKey(key);
}

bool obtenerAutostart() {
    wchar_t buf[MAX_PATH * 2];
    DWORD tam = sizeof(buf), tipo = REG_SZ;
    LONG r = RegGetValueW(HKEY_CURRENT_USER, kRunKey, kValueNm,
                          RRF_RT_REG_SZ, &tipo, buf, &tam);
    return r == ERROR_SUCCESS;
}

} // namespace agenda
