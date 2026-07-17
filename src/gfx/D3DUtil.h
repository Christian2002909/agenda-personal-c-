#pragma once
// Utilidades comunes de Direct3D 11: rutas de recursos junto al .exe, compilacion
// de shaders desde archivo, y un pequeño "render target" reutilizable.

#include <d3d11.h>
#include <wrl/client.h>
#include <string>

namespace agenda {

using Microsoft::WRL::ComPtr;

// Carpeta donde esta el ejecutable (para localizar assets/ y shaders/).
std::wstring exeDir();
// Ruta absoluta de un recurso relativo al .exe (ej. L"shaders\\glass.hlsl").
std::wstring resourcePath(const std::wstring& rel);

// Compila un shader HLSL desde archivo. Devuelve nullptr y llena 'error' si falla.
ComPtr<ID3DBlob> compilarShaderArchivo(const std::wstring& archivo,
                                       const char* entry,
                                       const char* target,
                                       std::string& error);

// Textura que sirve a la vez como destino de render (RTV) y como entrada de
// muestreo (SRV). Base de los pases de fondo y desenfoque.
struct RenderTexture {
    ComPtr<ID3D11Texture2D>          tex;
    ComPtr<ID3D11RenderTargetView>   rtv;
    ComPtr<ID3D11ShaderResourceView> srv;
    UINT w = 0, h = 0;

    bool crear(ID3D11Device* dev, UINT width, UINT height,
               DXGI_FORMAT fmt = DXGI_FORMAT_R8G8B8A8_UNORM);
    void liberar();
};

} // namespace agenda
