#include "gfx/Texture.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include <stb_image.h>

#include <fstream>
#include <vector>

namespace agenda {

LoadedTexture cargarTexturaArchivo(ID3D11Device* dev, ID3D11DeviceContext* ctx,
                                   const std::wstring& archivo) {
    LoadedTexture out;

    // Leer el archivo a memoria con ruta wide (evita problemas de acentos en stb_fopen).
    std::ifstream in(archivo.c_str(), std::ios::binary | std::ios::ate);
    if (!in) return out;
    std::streamsize sz = in.tellg();
    if (sz <= 0) return out;
    in.seekg(0, std::ios::beg);
    std::vector<unsigned char> bytes(static_cast<size_t>(sz));
    if (!in.read(reinterpret_cast<char*>(bytes.data()), sz)) return out;

    int w = 0, h = 0, canales = 0;
    unsigned char* pix = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()),
                                               &w, &h, &canales, 4);
    if (!pix) return out;

    D3D11_TEXTURE2D_DESC td{};
    td.Width = static_cast<UINT>(w);
    td.Height = static_cast<UINT>(h);
    td.MipLevels = 0;                 // 0 => cadena completa de mips
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET; // RT: para GenerateMips
    td.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    ComPtr<ID3D11Texture2D> tex;
    if (FAILED(dev->CreateTexture2D(&td, nullptr, &tex))) {
        stbi_image_free(pix);
        return out;
    }
    // Subir el nivel 0 y generar mips.
    ctx->UpdateSubresource(tex.Get(), 0, nullptr, pix, static_cast<UINT>(w) * 4, 0);
    stbi_image_free(pix);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
    srvd.Format = td.Format;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MipLevels = static_cast<UINT>(-1);
    if (FAILED(dev->CreateShaderResourceView(tex.Get(), &srvd, &out.srv))) {
        out.srv.Reset();
        return out;
    }
    ctx->GenerateMips(out.srv.Get());

    out.w = w;
    out.h = h;
    return out;
}

} // namespace agenda
