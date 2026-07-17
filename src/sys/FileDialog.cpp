#include "sys/FileDialog.h"

#include <commdlg.h>

namespace agenda {

std::wstring elegirImagen(HWND owner) {
    wchar_t archivo[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"Imagenes\0*.png;*.jpg;*.jpeg;*.webp\0Todos\0*.*\0";
    ofn.lpstrFile = archivo;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&ofn)) return std::wstring(archivo);
    return L"";
}

} // namespace agenda
