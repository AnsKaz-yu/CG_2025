#ifndef RENDER_CLASS_H
#define RENDER_CLASS_H

#include <dxgi.h>
#include <d3d11.h>

class RenderClass
{
public:
    RenderClass(): m_pDevice(nullptr), 
                   m_pDeviceContext(nullptr), 
                   m_pSwapChain(nullptr), 
                   m_pRenderTargetView(nullptr)
    {}

    HRESULT Init(HWND hWnd);
    void Terminate();

    void Render();
    void Resize(HWND hWnd);

private:
    HRESULT ConfigureBackBuffer();

    ID3D11Device* m_pDevice;
    ID3D11DeviceContext* m_pDeviceContext;

    IDXGISwapChain* m_pSwapChain;
    ID3D11RenderTargetView* m_pRenderTargetView;
};
#endif
