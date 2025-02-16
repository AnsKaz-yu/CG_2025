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
                   m_pRenderTargetView(nullptr),
                   m_pVertexBuffer(nullptr),
                   m_pIndexBuffer(nullptr),
                   m_pPixelShader(nullptr),
                   m_pVertexShader(nullptr),
                   m_pLayout(nullptr)
    {}

    HRESULT Init(HWND hWnd);
    void Terminate();

    HRESULT InitBufferShader();
    void TerminateBufferShader();

    HRESULT CompileShader(const std::wstring& path, ID3DBlob** pCodeShader=nullptr);

    void Render();
    void Resize(HWND hWnd);

private:
    HRESULT ConfigureBackBuffer();

    ID3D11Device* m_pDevice;
    ID3D11DeviceContext* m_pDeviceContext;

    IDXGISwapChain* m_pSwapChain;
    ID3D11RenderTargetView* m_pRenderTargetView;

    ID3D11Buffer* m_pVertexBuffer;
    ID3D11Buffer* m_pIndexBuffer;

    ID3D11PixelShader* m_pPixelShader;
    ID3D11VertexShader* m_pVertexShader;
    ID3D11InputLayout* m_pLayout;
};
#endif
