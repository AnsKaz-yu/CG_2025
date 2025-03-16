#ifndef RENDER_CLASS_H
#define RENDER_CLASS_H

#include <dxgi.h>
#include <d3d11.h>
#include <DirectXMath.h>

using namespace DirectX;

class RenderClass
{
public:
    RenderClass() :
        m_pDevice(nullptr),
        m_pDeviceContext(nullptr),
        m_pSwapChain(nullptr),
        m_pRenderTargetView(nullptr),
        m_pVertexBuffer(nullptr),
        m_pIndexBuffer(nullptr),
        m_pPixelShader(nullptr),
        m_pVertexShader(nullptr),
        m_pLayout(nullptr),
        m_pModelBuffer(nullptr),
        m_pVPBuffer(nullptr),
        m_szTitle(nullptr),
        m_szWindowClass(nullptr),
        m_pTextureView(nullptr),
        m_pSamplerState(nullptr),
        m_pSkyboxSRV(nullptr),
        m_pSkyboxVB(nullptr),
        m_pSkyboxVPBuffer(nullptr),
        m_pSkyboxVS(nullptr),
        m_pSkyboxPS(nullptr),
        m_pSkyboxLayout(nullptr),
        m_pDepthView(nullptr),
        m_CameraPosition(0.0f, 0.0f, -5.0f),
        m_CameraSpeed(0.1f),
        m_LRAngle(0.0f),
        m_UDAngle(0.0f)
    {
    }

    HRESULT Init(HWND hWnd, WCHAR szTitle[], WCHAR szWindowClass[]);
    void Terminate();

    HRESULT InitBufferShader();
    void TerminateBufferShader();

    HRESULT InitSkybox();
    void TerminateSkybox();

    HRESULT CompileShader(const std::wstring& path, ID3D11VertexShader** ppVertexShader, ID3D11PixelShader** ppPixelShader, ID3DBlob** pCodeShader = nullptr);

    void Render();
    void Resize(HWND hWnd);
    void MoveCamera(float dx, float dy, float dz);
    void RotateCamera(float yaw, float pitch);

private:
    HRESULT ConfigureBackBuffer(UINT width, UINT height);
    void SetMVPBuffer();

    HRESULT LoadCubemapFropCrossImage(ID3D11Device* device, ID3D11DeviceContext* context, const wchar_t* filename, ID3D11ShaderResourceView** cubeSVR);

    ID3D11Device* m_pDevice;
    ID3D11DeviceContext* m_pDeviceContext;

    IDXGISwapChain* m_pSwapChain;
    ID3D11RenderTargetView* m_pRenderTargetView;

    ID3D11Buffer* m_pModelBuffer;
    ID3D11Buffer* m_pVPBuffer;

    ID3D11Buffer* m_pVertexBuffer;
    ID3D11Buffer* m_pIndexBuffer;

    ID3D11PixelShader* m_pPixelShader;
    ID3D11VertexShader* m_pVertexShader;
    ID3D11InputLayout* m_pLayout;

    ID3D11ShaderResourceView* m_pTextureView;
    ID3D11SamplerState* m_pSamplerState;

    ID3D11ShaderResourceView* m_pSkyboxSRV;
    ID3D11Buffer* m_pSkyboxVB;
    ID3D11Buffer* m_pSkyboxVPBuffer;
    ID3D11VertexShader* m_pSkyboxVS;
    ID3D11PixelShader* m_pSkyboxPS;
    ID3D11InputLayout* m_pSkyboxLayout;
    ID3D11DepthStencilView* m_pDepthView;

    float m_CubeAngle = 0.0f;
    WCHAR* m_szTitle;
    WCHAR* m_szWindowClass;

    XMFLOAT3 m_CameraPosition;
    float m_CameraSpeed;
    float m_LRAngle;  
    float m_UDAngle;   

};
#endif