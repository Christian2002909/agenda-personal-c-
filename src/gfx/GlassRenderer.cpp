#include "gfx/GlassRenderer.h"

#include <algorithm>
#include <cstring>
#include <cstdlib>

namespace agenda {

// --- Estructuras de constant buffer (deben coincidir con los .hlsl) ---------
struct BgCB {
    float bgColor[4];
    float blobAColor[4];
    float blobBColor[4];
    float blobACenter[2]; float blobBCenter[2];
    float blobARadius[2]; float blobBRadius[2];
    float overlay[4];
    float resolution[2];  float photoSize[2];
};
struct BlurCB {
    float texelSize[2]; float pad[2];
};
struct GlassCB {
    float resolution[2];
    float panelMin[2];
    float panelSize[2];
    float quadMin[2];
    float quadSize[2];
    float pad0[2];
    float tint[4];
    float rim[4];
    float radius;
    float time;
    float refractPx;
    float shadowStrength;
};

static void setCol(float* d, const Col& c) { d[0]=c.r; d[1]=c.g; d[2]=c.b; d[3]=c.a; }

template <typename T>
static ComPtr<ID3D11Buffer> crearCB(ID3D11Device* dev) {
    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = (sizeof(T) + 15) & ~15u;   // multiplo de 16
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    ComPtr<ID3D11Buffer> b;
    dev->CreateBuffer(&bd, nullptr, &b);
    return b;
}
template <typename T>
static void subirCB(ID3D11DeviceContext* ctx, ID3D11Buffer* buf, const T& data) {
    D3D11_MAPPED_SUBRESOURCE ms{};
    if (SUCCEEDED(ctx->Map(buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
        std::memcpy(ms.pData, &data, sizeof(T));
        ctx->Unmap(buf, 0);
    }
}

bool GlassRenderer::recompilarShaders(std::string& error) {
    auto fs    = resourcePath(L"shaders\\fullscreen.hlsl");
    auto bg    = resourcePath(L"shaders\\background.hlsl");
    auto blur  = resourcePath(L"shaders\\blur_kawase.hlsl");
    auto glass = resourcePath(L"shaders\\glass.hlsl");

    auto vsFull = compilarShaderArchivo(fs, "VSFull", "vs_5_0", error);
    if (!vsFull) return false;
    dev_->CreateVertexShader(vsFull->GetBufferPointer(), vsFull->GetBufferSize(), nullptr, &vsFull_);

    auto vsGlass = compilarShaderArchivo(glass, "VSGlass", "vs_5_0", error);
    if (!vsGlass) return false;
    dev_->CreateVertexShader(vsGlass->GetBufferPointer(), vsGlass->GetBufferSize(), nullptr, &vsGlass_);

    struct { const std::wstring* file; const char* entry; ComPtr<ID3D11PixelShader>* out; } ps[] = {
        { &fs,    "PSCopy",     &psCopy_ },
        { &bg,    "PSGradient", &psGradient_ },
        { &bg,    "PSPhoto",    &psPhoto_ },
        { &bg,    "PSSolid",    &psSolid_ },
        { &blur,  "PSDown",     &psDown_ },
        { &blur,  "PSUp",       &psUp_ },
        { &glass, "PSGlass",    &psGlass_ },
    };
    for (auto& e : ps) {
        auto blob = compilarShaderArchivo(*e.file, e.entry, "ps_5_0", error);
        if (!blob) return false;
        e.out->Reset();
        dev_->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, e.out->GetAddressOf());
    }
    return true;
}

bool GlassRenderer::init(ID3D11Device* dev, ID3D11DeviceContext* ctx) {
    dev_ = dev; ctx_ = ctx;

    std::string error;
    if (!recompilarShaders(error)) {
        // Deja los shaders en null; main mostrara el error de compilacion.
        OutputDebugStringA(("Error compilando shaders: " + error + "\n").c_str());
        return false;
    }

    cbBg_    = crearCB<BgCB>(dev_);
    cbBlur_  = crearCB<BlurCB>(dev_);
    cbGlass_ = crearCB<GlassCB>(dev_);

    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    dev_->CreateSamplerState(&sd, &sampler_);

    D3D11_BLEND_DESC bo{};
    bo.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    dev_->CreateBlendState(&bo, &blendOpaque_);

    D3D11_BLEND_DESC ba{};
    ba.RenderTarget[0].BlendEnable = TRUE;
    ba.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    ba.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    ba.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    ba.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    ba.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    ba.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    ba.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    dev_->CreateBlendState(&ba, &blendAlpha_);

    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    dev_->CreateRasterizerState(&rd, &raster_);

    return true;
}

void GlassRenderer::ensureSize(UINT w, UINT h) {
    if (w == 0) w = 1;
    if (h == 0) h = 1;
    if (w == w_ && h == h_ && bgTex_.tex) return;
    w_ = w; h_ = h;

    bgTex_.crear(dev_, w, h);
    blurTex_.crear(dev_, w, h);

    downs_.clear();
    UINT lw = w, lh = h;
    // Mas niveles = desenfoque mas suave/difuso ("frosted" tipo Apple).
    for (int i = 0; i < 5; ++i) {
        lw = std::max<UINT>(1, lw / 2);
        lh = std::max<UINT>(1, lh / 2);
        RenderTexture rt;
        rt.crear(dev_, lw, lh);
        downs_.push_back(std::move(rt));
    }
}

void GlassRenderer::fullscreenPass(ID3D11RenderTargetView* rtv, UINT w, UINT h,
                                   ID3D11PixelShader* ps, ID3D11ShaderResourceView* src) {
    ID3D11RenderTargetView* rtvs[] = { rtv };
    ctx_->OMSetRenderTargets(1, rtvs, nullptr);
    D3D11_VIEWPORT vp{}; vp.Width=(float)w; vp.Height=(float)h; vp.MaxDepth=1.0f;
    ctx_->RSSetViewports(1, &vp);

    ctx_->IASetInputLayout(nullptr);
    ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx_->VSSetShader(vsFull_.Get(), nullptr, 0);
    ctx_->PSSetShader(ps, nullptr, 0);
    ctx_->PSSetSamplers(0, 1, sampler_.GetAddressOf());
    if (src) ctx_->PSSetShaderResources(0, 1, &src);
    ctx_->Draw(3, 0);

    ID3D11ShaderResourceView* nulo[1] = { nullptr };
    ctx_->PSSetShaderResources(0, 1, nulo);   // desvincula para poder usarla como RTV luego
}

void GlassRenderer::buildBlur() {
    // Constant buffer del blur se actualiza por pase (texelSize de la fuente).
    auto pase = [&](ID3D11RenderTargetView* rtv, UINT w, UINT h,
                    ID3D11PixelShader* ps, ID3D11ShaderResourceView* src,
                    UINT srcW, UINT srcH) {
        BlurCB cb{}; cb.texelSize[0] = 1.0f / (float)srcW; cb.texelSize[1] = 1.0f / (float)srcH;
        subirCB(ctx_, cbBlur_.Get(), cb);
        ctx_->PSSetConstantBuffers(0, 1, cbBlur_.GetAddressOf());
        fullscreenPass(rtv, w, h, ps, src);
    };

    const int n = (int)downs_.size();
    // Downsample: bg -> down0 -> down1 -> ... -> down[n-1]
    pase(downs_[0].rtv.Get(), downs_[0].w, downs_[0].h, psDown_.Get(), bgTex_.srv.Get(), bgTex_.w, bgTex_.h);
    for (int i = 1; i < n; ++i)
        pase(downs_[i].rtv.Get(), downs_[i].w, downs_[i].h, psDown_.Get(),
             downs_[i-1].srv.Get(), downs_[i-1].w, downs_[i-1].h);

    // Upsample: down[n-1] -> ... -> down0 -> blurTex (full res)
    for (int i = n - 2; i >= 0; --i)
        pase(downs_[i].rtv.Get(), downs_[i].w, downs_[i].h, psUp_.Get(),
             downs_[i+1].srv.Get(), downs_[i+1].w, downs_[i+1].h);
    pase(blurTex_.rtv.Get(), blurTex_.w, blurTex_.h, psUp_.Get(),
         downs_[0].srv.Get(), downs_[0].w, downs_[0].h);
}

void GlassRenderer::renderBackground(ID3D11RenderTargetView* backRTV, UINT w, UINT h,
                                     const ThemePalette& pal, const Config& cfg,
                                     ID3D11ShaderResourceView* photo, int photoW, int photoH) {
    ensureSize(w, h);
    ctx_->RSSetState(raster_.Get());
    float bf[4] = {0,0,0,0};
    ctx_->OMSetBlendState(blendOpaque_.Get(), bf, 0xffffffff);

    // Preparar el constant buffer del fondo.
    BgCB cb{};
    setCol(cb.bgColor,    pal.bg);
    setCol(cb.blobAColor, pal.blobA);
    setCol(cb.blobBColor, pal.blobB);
    cb.blobACenter[0]=0.12f; cb.blobACenter[1]=0.08f;
    cb.blobBCenter[0]=0.92f; cb.blobBCenter[1]=1.00f;
    cb.blobARadius[0]=1.0f;  cb.blobARadius[1]=0.93f;
    cb.blobBRadius[0]=0.9f;  cb.blobBRadius[1]=1.07f;
    cb.overlay[0]=cb.overlay[1]=cb.overlay[2]=0; cb.overlay[3]=0;
    cb.resolution[0]=(float)w; cb.resolution[1]=(float)h;
    cb.photoSize[0]=(float)std::max(photoW,1); cb.photoSize[1]=(float)std::max(photoH,1);

    // Elegir el pixel shader segun el tipo de fondo.
    ID3D11PixelShader* ps = psGradient_.Get();
    const std::string& tipo = cfg.fondo.tipo;
    if ((tipo == "lavanda" || tipo == "tulipanes" || tipo == "imagen") && photo) {
        ps = psPhoto_.Get();
        if (pal.isDark) { // capa oscura sobre la foto (themes.css tema oscuro)
            cb.overlay[0]=0.06f; cb.overlay[1]=0.04f; cb.overlay[2]=0.10f; cb.overlay[3]=0.5f;
        }
    } else if (tipo == "color") {
        ps = psSolid_.Get();
        // valor es un color hex "#rrggbb"
        unsigned int rgb = 0xffffff;
        if (cfg.fondo.valor.size() >= 7 && cfg.fondo.valor[0]=='#')
            rgb = (unsigned int)strtoul(cfg.fondo.valor.c_str()+1, nullptr, 16);
        cb.bgColor[0]=((rgb>>16)&0xFF)/255.0f; cb.bgColor[1]=((rgb>>8)&0xFF)/255.0f; cb.bgColor[2]=(rgb&0xFF)/255.0f; cb.bgColor[3]=1.0f;
    }

    subirCB(ctx_, cbBg_.Get(), cb);
    ctx_->PSSetConstantBuffers(0, 1, cbBg_.GetAddressOf());

    // 1) Fondo nitido -> bgTex_
    if (photo && ps == psPhoto_.Get()) ctx_->PSSetShaderResources(0, 1, &photo);
    fullscreenPass(bgTex_.rtv.Get(), w, h, ps, (ps==psPhoto_.Get()) ? photo : nullptr);

    // 2) Copiar bgTex_ -> back buffer (queda nitido donde no hay vidrio)
    fullscreenPass(backRTV, w, h, psCopy_.Get(), bgTex_.srv.Get());

    // 3) Construir la version desenfocada para el vidrio
    buildBlur();
}

void GlassRenderer::drawPanel(ID3D11RenderTargetView* backRTV, UINT w, UINT h,
                              const GlassPanel& p, float timeSec) {
    ID3D11RenderTargetView* rtvs[] = { backRTV };
    ctx_->OMSetRenderTargets(1, rtvs, nullptr);
    D3D11_VIEWPORT vp{}; vp.Width=(float)w; vp.Height=(float)h; vp.MaxDepth=1.0f;
    ctx_->RSSetViewports(1, &vp);

    float bf[4] = {0,0,0,0};
    ctx_->OMSetBlendState(blendAlpha_.Get(), bf, 0xffffffff);
    ctx_->RSSetState(raster_.Get());

    const float margin = 44.0f;   // espacio para la sombra flotante
    GlassCB cb{};
    cb.resolution[0]=(float)w; cb.resolution[1]=(float)h;
    cb.panelMin[0]=p.x; cb.panelMin[1]=p.y;
    cb.panelSize[0]=p.w; cb.panelSize[1]=p.h;
    cb.quadMin[0]=p.x - margin; cb.quadMin[1]=p.y - margin*0.5f;
    cb.quadSize[0]=p.w + margin*2.0f; cb.quadSize[1]=p.h + margin*1.5f;
    setCol(cb.tint, p.tint);
    setCol(cb.rim, p.rim);
    cb.radius = p.radius;
    cb.time = timeSec;
    cb.refractPx = p.refractPx;
    cb.shadowStrength = p.shadowStrength;
    subirCB(ctx_, cbGlass_.Get(), cb);

    ctx_->IASetInputLayout(nullptr);
    ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ctx_->VSSetShader(vsGlass_.Get(), nullptr, 0);
    ctx_->VSSetConstantBuffers(0, 1, cbGlass_.GetAddressOf());
    ctx_->PSSetShader(psGlass_.Get(), nullptr, 0);
    ctx_->PSSetConstantBuffers(0, 1, cbGlass_.GetAddressOf());
    ctx_->PSSetSamplers(0, 1, sampler_.GetAddressOf());
    ctx_->PSSetShaderResources(0, 1, blurTex_.srv.GetAddressOf());
    ctx_->Draw(4, 0);

    ID3D11ShaderResourceView* nulo[1] = { nullptr };
    ctx_->PSSetShaderResources(0, 1, nulo);
}

} // namespace agenda
