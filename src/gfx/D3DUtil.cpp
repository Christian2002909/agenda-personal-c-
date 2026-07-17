#include "gfx/D3DUtil.h"

#include <windows.h>
#include <d3dcompiler.h>
#include <fstream>
#include <sstream>

namespace agenda {

std::wstring exeDir() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring path(buf, n);
    size_t slash = path.find_last_of(L"\\/");
    return (slash == std::wstring::npos) ? L"." : path.substr(0, slash);
}

std::wstring resourcePath(const std::wstring& rel) {
    return exeDir() + L"\\" + rel;
}

ComPtr<ID3DBlob> compilarShaderArchivo(const std::wstring& archivo,
                                       const char* entry,
                                       const char* target,
                                       std::string& error) {
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
    ComPtr<ID3DBlob> code, err;
    HRESULT hr = D3DCompileFromFile(archivo.c_str(), nullptr,
                                    D3D_COMPILE_STANDARD_FILE_INCLUDE,
                                    entry, target, flags, 0, &code, &err);
    if (FAILED(hr)) {
        error.clear();
        if (err) error.assign(static_cast<const char*>(err->GetBufferPointer()),
                              err->GetBufferSize());
        else     error = "No se pudo abrir/compilar el shader";
        return nullptr;
    }
    return code;
}

bool RenderTexture::crear(ID3D11Device* dev, UINT width, UINT height, DXGI_FORMAT fmt) {
    liberar();
    if (width == 0)  width = 1;
    if (height == 0) height = 1;
    w = width; h = height;

    D3D11_TEXTURE2D_DESC td{};
    td.Width = width;
    td.Height = height;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = fmt;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(dev->CreateTexture2D(&td, nullptr, &tex))) return false;
    if (FAILED(dev->CreateRenderTargetView(tex.Get(), nullptr, &rtv))) return false;
    if (FAILED(dev->CreateShaderResourceView(tex.Get(), nullptr, &srv))) return false;
    return true;
}

void RenderTexture::liberar() {
    srv.Reset(); rtv.Reset(); tex.Reset();
    w = h = 0;
}

} // namespace agenda
