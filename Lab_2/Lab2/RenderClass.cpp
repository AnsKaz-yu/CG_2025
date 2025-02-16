#include "framework.h"
#include "RenderClass.h"

#include <dxgi.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "dxgi.lib")
#pragma comment (lib, "d3dcompiler.lib")

// Определение структуры вершины
struct Vertex
{
    float x, y, z;
    COLORREF color; // Цвет в формате 0x00BBGGRR (DWORD)
};

//
// Методы класса RenderClass
//

HRESULT RenderClass::Init(HWND hWnd)
{
    HRESULT result;

    IDXGIFactory* pFactory = nullptr;
    result = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory);

    IDXGIAdapter* pSelectedAdapter = nullptr;
    if (SUCCEEDED(result))
    {
        IDXGIAdapter* pAdapter = nullptr;
        UINT adapterIdx = 0;
        while (SUCCEEDED(pFactory->EnumAdapters(adapterIdx, &pAdapter)))
        {
            DXGI_ADAPTER_DESC desc;
            pAdapter->GetDesc(&desc);

            if (wcscmp(desc.Description, L"Microsoft Basic Render Driver") != 0)
            {
                pSelectedAdapter = pAdapter;
                break;
            }
            pAdapter->Release();
            adapterIdx++;
        }
    }

    // Создание устройства DirectX 11
    D3D_FEATURE_LEVEL level;
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
    if (SUCCEEDED(result))
    {
        UINT flags = 0;
#ifdef _DEBUG
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        result = D3D11CreateDevice(pSelectedAdapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
            flags, levels, 1, D3D11_SDK_VERSION, &m_pDevice, &level, &m_pDeviceContext);
    }

    if (SUCCEEDED(result))
    {
        DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
        swapChainDesc.BufferCount = 2;
        swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.OutputWindow = hWnd;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.Windowed = true;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        swapChainDesc.Flags = 0;

        result = pFactory->CreateSwapChain(m_pDevice, &swapChainDesc, &m_pSwapChain);
    }

    if (SUCCEEDED(result))
    {
        result = ConfigureBackBuffer();
    }

    if (SUCCEEDED(result))
    {
        result = InitBufferShader();
    }

    pSelectedAdapter->Release();
    pFactory->Release();

    return result;
}

void RenderClass::Terminate()
{
    TerminateBufferShader();

    if (m_pDeviceContext)
    {
        m_pDeviceContext->ClearState();
        m_pDeviceContext->Release();
        m_pDeviceContext = nullptr;
    }
    if (m_pRenderTargetView)
    {
        m_pRenderTargetView->Release();
        m_pRenderTargetView = nullptr;
    }
    if (m_pSwapChain)
    {
        m_pSwapChain->Release();
        m_pSwapChain = nullptr;
    }
    if (m_pDevice)
    {
        m_pDevice->Release();
        m_pDevice = nullptr;
    }
}

// Отрисовка кадра (отрисовка треугольника)
void RenderClass::Render()
{
    // Заливка фона
    float BackColor[4] = { 0.66f, 0.47f, 0.78f, 1.0f };
    m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, BackColor);

    // Установка вершинного буфера
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);
    // Установка индексного буфера
    m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    // Установка топологии примитивов
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    // Установка входного слоя (input layout)
    m_pDeviceContext->IASetInputLayout(m_pLayout);

    // Установка шейдеров
    m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

    // Отрисовка треугольника
    m_pDeviceContext->DrawIndexed(3, 0, 0);

    // Отображение кадра
    m_pSwapChain->Present(0, 0);
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

        // Изменение размера swap chain
        hr = m_pSwapChain->ResizeBuffers(1, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
        if (FAILED(hr))
        {
            MessageBox(nullptr, L"ResizeBuffers failed.", L"Error", MB_OK);
            return;
        }

        hr = ConfigureBackBuffer();
        if (FAILED(hr))
        {
            MessageBox(nullptr, L"Configure back buffer failed.", L"Error", MB_OK);
            return;
        }

        m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, nullptr);

        // Настройка вьюпорта
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

// Инициализация буферов и шейдеров (отрисовка треугольника)
HRESULT RenderClass::InitBufferShader()
{
    // Определение вершин и индексов для треугольника
    static const Vertex vertices[] = {
        {  0.0f,  0.5f, 0.0f, RGB(255, 122, 255) },
        {  0.5f, -0.5f, 0.0f, RGB(0, 255, 0) },
        { -0.5f, -0.5f, 0.0f, RGB(0, 255, 255) }
    };
    static const USHORT indices[] = { 0, 1, 2 };

    // Создание вершинного буфера
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = sizeof(vertices);
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices;
    HRESULT hr = m_pDevice->CreateBuffer(&vbDesc, &vbData, &m_pVertexBuffer);
    if (FAILED(hr))
        return hr;

    // Создание индексного буфера
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = sizeof(indices);
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices;
    hr = m_pDevice->CreateBuffer(&ibDesc, &ibData, &m_pIndexBuffer);
    if (FAILED(hr))
        return hr;

    // Описание входного слоя
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    // Компиляция вершинного шейдера
    ID3DBlob* pVSBlob = nullptr;
    hr = CompileShader(L"ColorVertex.vs", &pVSBlob);
    if (FAILED(hr))
        return hr;

    // Создание вершинного шейдера
    hr = m_pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &m_pVertexShader);
    if (FAILED(hr))
    {
        pVSBlob->Release();
        return hr;
    }

    // Компиляция пиксельного шейдера
    ID3DBlob* pPSBlob = nullptr;
    hr = CompileShader(L"ColorPixel.ps", &pPSBlob);
    if (FAILED(hr))
    {
        pVSBlob->Release();
        return hr;
    }
    // Создание пиксельного шейдера
    hr = m_pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &m_pPixelShader);
    pPSBlob->Release();
    if (FAILED(hr))
    {
        pVSBlob->Release();
        return hr;
    }

    // Создание входного слоя (input layout) на основе скомпилированного вершинного шейдера
    hr = m_pDevice->CreateInputLayout(layoutDesc, 2, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &m_pLayout);
    pVSBlob->Release();
    if (FAILED(hr))
        return hr;

    return S_OK;
}

// Освобождение ресурсов, связанных с буферами и шейдерами
void RenderClass::TerminateBufferShader()
{
    if (m_pLayout)
    {
        m_pLayout->Release();
        m_pLayout = nullptr;
    }
    if (m_pVertexShader)
    {
        m_pVertexShader->Release();
        m_pVertexShader = nullptr;
    }
    if (m_pPixelShader)
    {
        m_pPixelShader->Release();
        m_pPixelShader = nullptr;
    }
    if (m_pIndexBuffer)
    {
        m_pIndexBuffer->Release();
        m_pIndexBuffer = nullptr;
    }
    if (m_pVertexBuffer)
    {
        m_pVertexBuffer->Release();
        m_pVertexBuffer = nullptr;
    }
}

// Функция компиляции шейдера из файла
HRESULT RenderClass::CompileShader(const std::wstring& path, ID3DBlob** ppCodeBlob)
{
    // Определение профиля шейдера по расширению файла
    std::wstring extension = path.substr(path.find_last_of(L".") + 1);
    std::string profile;
    if (extension == L"vs")
    {
        profile = "vs_5_0";
    }
    else if (extension == L"ps")
    {
        profile = "ps_5_0";
    }
    else
    {
        return E_INVALIDARG;
    }

    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    ID3DBlob* pShaderBlob = nullptr;
    ID3DBlob* pErrorBlob = nullptr;
    HRESULT hr = D3DCompileFromFile(path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "main", profile.c_str(), flags, 0, &pShaderBlob, &pErrorBlob);
    if (FAILED(hr))
    {
        if (pErrorBlob)
        {
            OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
            pErrorBlob->Release();
        }
        if (pShaderBlob)
            pShaderBlob->Release();
        return hr;
    }
    if (pErrorBlob)
        pErrorBlob->Release();

    if (ppCodeBlob)
    {
        *ppCodeBlob = pShaderBlob;
    }
    else
    {
        pShaderBlob->Release();
    }
    return S_OK;
}
