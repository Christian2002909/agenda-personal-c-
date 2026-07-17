#pragma once
// Carga de imagenes (JPG/PNG) a texturas de Direct3D 11 mediante stb_image.

#include <d3d11.h>
#include <wrl/client.h>
#include <string>

namespace agenda {

using Microsoft::WRL::ComPtr;

struct LoadedTexture {
    ComPtr<ID3D11ShaderResourceView> srv;
    int w = 0, h = 0;
    bool valido() const { return srv != nullptr; }
};

// Carga una imagen desde disco (ruta wide, admite acentos). Devuelve textura vacia
// si falla. Genera mipmaps para muestreo suave al escalar.
LoadedTexture cargarTexturaArchivo(ID3D11Device* dev, ID3D11DeviceContext* ctx,
                                   const std::wstring& archivo);

} // namespace agenda
