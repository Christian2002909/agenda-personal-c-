#include "gfx/Renderer.h"

#include <iterator>

namespace agenda {

bool Renderer::init(HWND hwnd) {
    hwnd_ = hwnd;

    RECT rc{};
    GetClientRect(hwnd, &rc);
    w_ = static_cast<UINT>(rc.right - rc.left);
    h_ = static_cast<UINT>(rc.bottom - rc.top);
    if (w_ == 0) w_ = 1;
    if (h_ == 0) h_ = 1;

    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = w_;
    sd.BufferDesc.Height = h_;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    const D3D_FEATURE_LEVEL niveles[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL nivelObtenido{};

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        niveles, static_cast<UINT>(std::size(niveles)), D3D11_SDK_VERSION,
        &sd, &swap_, &device_, &nivelObtenido, &ctx_);

    if (FAILED(hr)) {
        // Reintento con SwapEffect clasico por si el flip model no esta soportado.
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        sd.BufferCount = 1;
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
            niveles, static_cast<UINT>(std::size(niveles)), D3D11_SDK_VERSION,
            &sd, &swap_, &device_, &nivelObtenido, &ctx_);
    }
    if (FAILED(hr)) return false;

    createRTV();
    return true;
}

void Renderer::createRTV() {
    ComPtr<ID3D11Texture2D> back;
    if (SUCCEEDED(swap_->GetBuffer(0, IID_PPV_ARGS(&back)))) {
        device_->CreateRenderTargetView(back.Get(), nullptr, &rtv_);
    }
}

void Renderer::resize(UINT w, UINT h) {
    if (!swap_ || (w == w_ && h == h_)) return;
    if (w == 0) w = 1;
    if (h == 0) h = 1;
    w_ = w; h_ = h;

    rtv_.Reset();
    swap_->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
    createRTV();
}

void Renderer::bindBackBuffer() {
    ID3D11RenderTargetView* rtvs[] = { rtv_.Get() };
    ctx_->OMSetRenderTargets(1, rtvs, nullptr);
    D3D11_VIEWPORT vp{};
    vp.Width = static_cast<float>(w_);
    vp.Height = static_cast<float>(h_);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    ctx_->RSSetViewports(1, &vp);
}

void Renderer::clearBackBuffer(float r, float g, float b, float a) {
    const float c[4] = { r, g, b, a };
    if (rtv_) ctx_->ClearRenderTargetView(rtv_.Get(), c);
}

void Renderer::present(bool vsync) {
    if (swap_) swap_->Present(vsync ? 1 : 0, 0);
}

void Renderer::shutdown() {
    rtv_.Reset();
    swap_.Reset();
    ctx_.Reset();
    device_.Reset();
}

} // namespace agenda
