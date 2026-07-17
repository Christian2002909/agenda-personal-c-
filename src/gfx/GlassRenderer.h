#pragma once
// Renderizador del efecto Liquid Glass y del fondo de la app.
// - Dibuja el fondo (degradado / foto / color) a pantalla completa.
// - Calcula su version desenfocada (Dual Kawase) para el "frosted glass".
// - Dibuja paneles de vidrio (refraccion + tinte + rim + sheen + sombra).

#include "gfx/D3DUtil.h"
#include "app/Theme.h"
#include "app/Model.h"

#include <vector>

namespace agenda {

struct GlassPanel {
    float x = 0, y = 0, w = 0, h = 0;  // rect del panel en px
    float radius = 22.0f;              // radio de esquina
    Col   tint;                        // color/translucidez del vidrio
    Col   rim;                         // brillo de borde
    float refractPx = 10.0f;           // fuerza de refraccion
    float shadowStrength = 0.18f;      // opacidad de la sombra flotante
};

class GlassRenderer {
public:
    bool init(ID3D11Device* dev, ID3D11DeviceContext* ctx);
    bool recompilarShaders(std::string& error);   // usado en init; recompila desde archivo
    void ensureSize(UINT w, UINT h);

    // Dibuja el fondo a 'backRTV' (nitido) y prepara la version desenfocada.
    // 'photo' puede ser null (modos degradado/color).
    void renderBackground(ID3D11RenderTargetView* backRTV, UINT w, UINT h,
                          const ThemePalette& pal, const Config& cfg,
                          ID3D11ShaderResourceView* photo, int photoW, int photoH);

    // Dibuja un panel de vidrio sobre 'backRTV' (debe estar vinculado como destino).
    void drawPanel(ID3D11RenderTargetView* backRTV, UINT w, UINT h,
                   const GlassPanel& panel, float timeSec);

private:
    void buildBlur();
    void fullscreenPass(ID3D11RenderTargetView* rtv, UINT w, UINT h,
                        ID3D11PixelShader* ps, ID3D11ShaderResourceView* src);

    ID3D11Device*        dev_ = nullptr;
    ID3D11DeviceContext* ctx_ = nullptr;

    // Shaders.
    ComPtr<ID3D11VertexShader> vsFull_;
    ComPtr<ID3D11VertexShader> vsGlass_;
    ComPtr<ID3D11PixelShader>  psCopy_, psGradient_, psPhoto_, psSolid_, psDown_, psUp_, psGlass_;

    // Constant buffers.
    ComPtr<ID3D11Buffer> cbBg_, cbBlur_, cbGlass_;

    // Estados.
    ComPtr<ID3D11SamplerState>      sampler_;
    ComPtr<ID3D11BlendState>        blendOpaque_, blendAlpha_;
    ComPtr<ID3D11RasterizerState>   raster_;

    // Texturas de trabajo.
    RenderTexture bgTex_;                 // fondo nitido (fuente del blur)
    RenderTexture blurTex_;               // resultado desenfocado (full res)
    std::vector<RenderTexture> downs_;    // niveles del Dual Kawase
    UINT w_ = 0, h_ = 0;
};

} // namespace agenda
