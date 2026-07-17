#pragma once
// Nucleo Direct3D 11: dispositivo, contexto, swap chain y back buffer.

#include <d3d11.h>
#include <wrl/client.h>

namespace agenda {

using Microsoft::WRL::ComPtr;

class Renderer {
public:
    bool init(HWND hwnd);
    void shutdown();
    void resize(UINT w, UINT h);
    void present(bool vsync);

    // Vincula el back buffer como destino de render con viewport a pantalla completa.
    void bindBackBuffer();
    void clearBackBuffer(float r, float g, float b, float a);

    ID3D11Device*           device()  const { return device_.Get(); }
    ID3D11DeviceContext*    context() const { return ctx_.Get(); }
    ID3D11RenderTargetView* backRTV() const { return rtv_.Get(); }
    UINT width()  const { return w_; }
    UINT height() const { return h_; }

private:
    void createRTV();

    ComPtr<ID3D11Device>           device_;
    ComPtr<ID3D11DeviceContext>    ctx_;
    ComPtr<IDXGISwapChain>         swap_;
    ComPtr<ID3D11RenderTargetView> rtv_;
    HWND hwnd_ = nullptr;
    UINT w_ = 0, h_ = 0;
};

} // namespace agenda
