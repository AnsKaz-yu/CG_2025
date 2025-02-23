#include "framework.h"
#include "RenderClass.h"

#include <filesystem>
#include <dxgi.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

struct Vertex
{
    float x, y, z;
    COLORREF color;
};

struct MatrixBuffer
{
    DirectX::XMMATRIX m;
};

struct CameraBuffer
{
    DirectX::XMMATRIX vp;
};

HRESULT RenderClass::Init(HWND hWnd, WCHAR szTitle[], WCHAR szWindowClass[])
{
    m_szTitle = szTitle;
    m_szWindowClass = szWindowClass;

    HRESULT hr = S_OK;

    IDXGIFactory* dxgiFactory = nullptr;
    hr = CreateDXGIFactory(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&dxgiFactory));

    IDXGIAdapter* chosenAdapter = nullptr;
    if (SUCCEEDED(hr))
    {
        IDXGIAdapter* currentAdapter = nullptr;
        UINT adapterIndex = 0;
        while (SUCCEEDED(dxgiFactory->EnumAdapters(adapterIndex, &currentAdapter)))
        {
            DXGI_ADAPTER_DESC adapterDesc;
            currentAdapter->GetDesc(&adapterDesc);

            if (wcscmp(adapterDesc.Description, L"Microsoft Basic Render Driver") != 0)
            {
                chosenAdapter = currentAdapter;
                break;
            }
            currentAdapter->Release();
            ++adapterIndex;
        }
    }

    D3D_FEATURE_LEVEL featureLevel;
    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
    if (SUCCEEDED(hr))
    {
        UINT createFlags = 0;
#ifdef _DEBUG
        createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        hr = D3D11CreateDevice(chosenAdapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
            createFlags, featureLevels, 1, D3D11_SDK_VERSION, &m_pDevice, &featureLevel, &m_pDeviceContext);
    }

    if (SUCCEEDED(hr))
    {
        DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
        swapChainDesc.BufferCount = 2;
        swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.OutputWindow = hWnd;
        swapChainDesc.SampleDesc.Count = 4;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.Windowed = TRUE;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        swapChainDesc.Flags = 0;

        hr = dxgiFactory->CreateSwapChain(m_pDevice, &swapChainDesc, &m_pSwapChain);
    }

    if (SUCCEEDED(hr))
    {
        hr = ConfigureBackBuffer();
    }

    if (SUCCEEDED(hr))
    {
        hr = InitBufferShader();
    }

    if (chosenAdapter)
        chosenAdapter->Release();
    if (dxgiFactory)
        dxgiFactory->Release();

    if (FAILED(hr))
    {
        Terminate();
    }

    return hr;
}

void RenderClass::Terminate()
{
    TerminateBufferShader();

    if (m_pDeviceContext)
    {
        m_pDeviceContext->ClearState();
        m_pDeviceContext->Release();
    }
    if (m_pRenderTargetView)
        m_pRenderTargetView->Release();
    if (m_pSwapChain)
        m_pSwapChain->Release();
    if (m_pDevice)
        m_pDevice->Release();
}

HRESULT RenderClass::InitBufferShader()
{
    static const Vertex vertices[] =
    {
        { -1.0f,  1.0f, -1.0f, RGB(255, 20, 147) },
        {  1.0f,  1.0f, -1.0f, RGB(34, 34, 34) },
        {  1.0f,  1.0f,  1.0f, RGB(255, 0, 255) },
        { -1.0f,  1.0f,  1.0f, RGB(34, 0, 0) },
        { -1.0f, -1.0f, -1.0f, RGB(255, 139, 0) },
        {  1.0f, -1.0f, -1.0f, RGB(255, 0, 0) },
        {  1.0f, -1.0f,  1.0f, RGB(0, 139, 139) },
        { -1.0f, -1.0f,  1.0f, RGB(255, 255, 0) }
    };

    WORD indices[] =
    {
        3,1,0,
        2,1,3,
        0,5,4,
        1,5,0,
        3,4,7,
        0,4,3,
        1,6,5,
        2,6,1,
        2,7,6,
        3,7,2,
        6,4,5,
        7,4,6,
    };

    static const D3D11_INPUT_ELEMENT_DESC inputElements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    HRESULT hr = S_OK;

    if (SUCCEEDED(hr))
    {
        D3D11_BUFFER_DESC vbDesc = {};
        vbDesc.ByteWidth = sizeof(vertices);
        vbDesc.Usage = D3D11_USAGE_DEFAULT;
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vbDesc.CPUAccessFlags = 0;
        vbDesc.MiscFlags = 0;
        vbDesc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA vbData = {};
        vbData.pSysMem = vertices;
        vbData.SysMemPitch = sizeof(vertices);
        vbData.SysMemSlicePitch = 0;

        hr = m_pDevice->CreateBuffer(&vbDesc, &vbData, &m_pVertexBuffer);
        if (FAILED(hr))
            return hr;
    }

    if (SUCCEEDED(hr))
    {
        D3D11_BUFFER_DESC ibDesc = {};
        ibDesc.ByteWidth = sizeof(indices);
        ibDesc.Usage = D3D11_USAGE_DEFAULT;
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        ibDesc.CPUAccessFlags = 0;
        ibDesc.MiscFlags = 0;
        ibDesc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA ibData = {};
        ibData.pSysMem = indices;
        ibData.SysMemPitch = sizeof(indices);
        ibData.SysMemSlicePitch = 0;

        hr = m_pDevice->CreateBuffer(&ibDesc, &ibData, &m_pIndexBuffer);
        if (FAILED(hr))
            return hr;
    }

    if (SUCCEEDED(hr))
    {
        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.ByteWidth = sizeof(MatrixBuffer);
        cbDesc.Usage = D3D11_USAGE_DEFAULT;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = 0;
        cbDesc.MiscFlags = 0;
        cbDesc.StructureByteStride = 0;

        hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pModelBuffer);
        if (FAILED(hr))
            return hr;
    }

    if (SUCCEEDED(hr))
    {
        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.ByteWidth = sizeof(CameraBuffer);
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        cbDesc.MiscFlags = 0;
        cbDesc.StructureByteStride = 0;

        hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pVPBuffer);
        if (FAILED(hr))
            return hr;
    }

    ID3DBlob* vertexShaderCode = nullptr;
    if (SUCCEEDED(hr))
    {
        hr = CompileShader(L"ColorVertex.vs", &vertexShaderCode);
    }
    if (SUCCEEDED(hr))
    {
        hr = CompileShader(L"ColorPixel.ps");
    }

    if (SUCCEEDED(hr))
    {
        hr = m_pDevice->CreateInputLayout(inputElements, 2,
            vertexShaderCode->GetBufferPointer(), vertexShaderCode->GetBufferSize(), &m_pLayout);
    }

    if (vertexShaderCode)
        vertexShaderCode->Release();

    return hr;
}

void RenderClass::TerminateBufferShader()
{
    if (m_pLayout)
        m_pLayout->Release();
    if (m_pPixelShader)
        m_pPixelShader->Release();
    if (m_pVertexShader)
        m_pVertexShader->Release();
    if (m_pIndexBuffer)
        m_pIndexBuffer->Release();
    if (m_pVertexBuffer)
        m_pVertexBuffer->Release();
    if (m_pModelBuffer)
        m_pModelBuffer->Release();
    if (m_pVPBuffer)
        m_pVPBuffer->Release();
}

std::wstring RenderClass::Extension(const std::wstring& filePath)
{
    size_t pos = filePath.find_last_of(L".");
    if (pos == std::wstring::npos || pos == 0)
        return L"";
    return filePath.substr(pos + 1);
}

HRESULT RenderClass::CompileShader(const std::wstring& filePath, ID3DBlob** pCodeShader)
{
    std::wstring ext = Extension(filePath);
    std::string shaderModel;

    if (ext == L"vs")
        shaderModel = "vs_5_0";
    else if (ext == L"ps")
        shaderModel = "ps_5_0";

    UINT compileFlags = 0;
#ifdef _DEBUG
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob* compiledCode = nullptr;
    ID3DBlob* errorMsg = nullptr;

    HRESULT hr = D3DCompileFromFile(filePath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "main", shaderModel.c_str(), 0, 0, &compiledCode, &errorMsg);
    if (!SUCCEEDED(hr) && errorMsg)
    {
        OutputDebugStringA((const char*)errorMsg->GetBufferPointer());
    }
    if (errorMsg)
        errorMsg->Release();

    if (SUCCEEDED(hr))
    {
        if (ext == L"vs")
        {
            hr = m_pDevice->CreateVertexShader(compiledCode->GetBufferPointer(), compiledCode->GetBufferSize(), nullptr, &m_pVertexShader);
            if (FAILED(hr))
            {
                compiledCode->Release();
                return hr;
            }
        }
        else if (ext == L"ps")
        {
            hr = m_pDevice->CreatePixelShader(compiledCode->GetBufferPointer(), compiledCode->GetBufferSize(), nullptr, &m_pPixelShader);
            if (FAILED(hr))
            {
                compiledCode->Release();
                return hr;
            }
        }
    }

    if (pCodeShader)
        *pCodeShader = compiledCode;
    else
        compiledCode->Release();

    return hr;
}

void RenderClass::MoveCamera(float dx, float dy, float dz)
{
    m_CameraPosition.x += dx * m_CameraSpeed;
    m_CameraPosition.y += dy * m_CameraSpeed;
    m_CameraPosition.z += dz * m_CameraSpeed;
}

void RenderClass::RotateCamera(float lrAngle, float udAngle)
{
    m_LRAngle += lrAngle;
    m_UDAngle += udAngle;

    if (m_LRAngle > XM_2PI) m_LRAngle -= XM_2PI;
    if (m_LRAngle < -XM_2PI) m_LRAngle += XM_2PI;

    if (m_UDAngle > XM_PIDIV2) m_UDAngle = XM_PIDIV2;
    if (m_UDAngle < -XM_PIDIV2) m_UDAngle = -XM_PIDIV2;
}

void RenderClass::Render()
{
    float clearColor[4] = { 0.1f, 0.2f, 0.8f, 1.0f };
    m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, clearColor);

    SetMVPBuffer();

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);
    m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->IASetInputLayout(m_pLayout);

    m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

    m_pDeviceContext->DrawIndexed(36, 0, 0);
    m_pSwapChain->Present(1, 0);
}

void RenderClass::SetMVPBuffer()
{
    m_CubeAngle += 0.01f;
    if (m_CubeAngle > XM_2PI) m_CubeAngle -= XM_2PI;
    XMMATRIX model = XMMatrixRotationY(m_CubeAngle);

    XMVECTOR direction = XMVectorSet(
        cosf(m_UDAngle) * sinf(m_LRAngle),
        sinf(m_UDAngle),
        cosf(m_UDAngle) * cosf(m_LRAngle),
        0.0f
    );

    XMVECTOR eyePos = XMVectorSet(m_CameraPosition.x, m_CameraPosition.y, m_CameraPosition.z, 0.0f);
    XMVECTOR focusPoint = XMVectorAdd(eyePos, direction);
    XMVECTOR upDir = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX view = XMMatrixLookAtLH(eyePos, focusPoint, upDir);

    RECT rc;
    GetClientRect(FindWindow(m_szWindowClass, m_szTitle), &rc);
    float aspect = static_cast<float>(rc.right - rc.left) / (rc.bottom - rc.top);
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspect, 0.1f, 100.0f);

    XMMATRIX vp = view * proj;
    XMMATRIX mT = XMMatrixTranspose(model);
    XMMATRIX vpT = XMMatrixTranspose(vp);

    m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, &mT, 0, 0);

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = m_pDeviceContext->Map(m_pVPBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (SUCCEEDED(hr))
    {
        memcpy(mappedResource.pData, &vpT, sizeof(XMMATRIX));
        m_pDeviceContext->Unmap(m_pVPBuffer, 0);
    }
    m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pModelBuffer);
    m_pDeviceContext->VSSetConstantBuffers(1, 1, &m_pVPBuffer);
}

HRESULT RenderClass::ConfigureBackBuffer()
{
    ID3D11Texture2D* pBackBuffer = nullptr;
    HRESULT hr = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    if (FAILED(hr))
        return hr;

    hr = m_pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_pRenderTargetView);
    pBackBuffer->Release();
    if (FAILED(hr))
        return hr;

    return hr;
}

void RenderClass::Resize(HWND hWnd)
{
    if (m_pRenderTargetView)
    {
        m_pRenderTargetView->Release();
        m_pRenderTargetView = nullptr;
    }

    if (m_pSwapChain)
    {
        HRESULT hr;

        RECT rc;
        GetClientRect(hWnd, &rc);
        UINT width = rc.right - rc.left;
        UINT height = rc.bottom - rc.top;

        hr = m_pSwapChain->ResizeBuffers(1, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
        if (FAILED(hr))
        {
            MessageBox(nullptr, L"ResizeBuffers failed.", L"Error", MB_OK);
            return;
        }

        HRESULT resultBack = ConfigureBackBuffer();
        if (FAILED(resultBack))
        {
            MessageBox(nullptr, L"Configure back buffer failed.", L"Error", MB_OK);
            return;
        }

        m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, nullptr);

        D3D11_VIEWPORT vp;
        vp.Width = (FLOAT)width;
        vp.Height = (FLOAT)height;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftX = 0;
        vp.TopLeftY = 0;
        m_pDeviceContext->RSSetViewports(1, &vp);
    }
}