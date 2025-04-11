#include "framework.h"
#include "RenderClass.h"
#include "WICTextureLoader.h"
#include "DDSTextureLoader11.h"
#include <filesystem>
#include <wrl/client.h>
#include <dxgi.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#pragma comment (lib, "d3dcompiler.lib")
#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "dxgi.lib")

using namespace Microsoft::WRL;

std::wstring Extension(const std::wstring& path) {
    size_t dotPos = path.find_last_of(L".");
    if (dotPos == std::wstring::npos || dotPos == 0) {
        return L"";
    }
    return path.substr(dotPos + 1);
}

HRESULT RenderClass::Init(HWND hWnd, WCHAR szTitle[], WCHAR szWindowClass[]) {
    m_szTitle = szTitle;
    m_szWindowClass = szWindowClass;

    HRESULT hr;

    IDXGIFactory* pFactory = nullptr;
    hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory);

    IDXGIAdapter* pSelectedAdapter = NULL;
    if (SUCCEEDED(hr)) {
        IDXGIAdapter* pAdapter = NULL;
        UINT adapterIdx = 0;
        while (SUCCEEDED(pFactory->EnumAdapters(adapterIdx, &pAdapter))) {
            DXGI_ADAPTER_DESC desc;
            pAdapter->GetDesc(&desc);

            if (wcscmp(desc.Description, L"Microsoft Basic Render Driver") != 0) {
                pSelectedAdapter = pAdapter;
                break;
            }

            pAdapter->Release();
            adapterIdx++;
        }
    }

    D3D_FEATURE_LEVEL level;
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
    if (SUCCEEDED(hr)) {
        UINT flags = 0;
#ifdef _DEBUG
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        hr = D3D11CreateDevice(pSelectedAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL,
            flags, levels, 1, D3D11_SDK_VERSION, &m_pDevice, &level, &m_pDeviceContext);
    }

    if (SUCCEEDED(hr)) {
        DXGI_SWAP_CHAIN_DESC swapChainDesc = { 0 };
        swapChainDesc.BufferCount = 2;
        swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.OutputWindow = hWnd;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.Windowed = true;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        swapChainDesc.Flags = 0;

        hr = pFactory->CreateSwapChain(m_pDevice, &swapChainDesc, &m_pSwapChain);
    }

    if (SUCCEEDED(hr)) {
        RECT rc;
        GetClientRect(hWnd, &rc);
        UINT width = rc.right - rc.left;
        UINT height = rc.bottom - rc.top;
        hr = ConfigureBackBuffer(width, height);
    }

    if (SUCCEEDED(hr)) {
        hr = InitBufferShader();
    }

    if (SUCCEEDED(hr))
    {
        hr = Init2DArray();
    }

    if (SUCCEEDED(hr))
    {
        InitImGui(hWnd);
    }

    if (SUCCEEDED(hr)) {
        hr = InitSkybox();
    }

    if (SUCCEEDED(hr)) {
        hr = InitParallelogram();
    }

    if (SUCCEEDED(hr))
    {
        hr = InitFullScreenTriangle();
    }

    if (SUCCEEDED(hr))
    {
        hr = InitComputeShader();
    }

    pSelectedAdapter->Release();
    pFactory->Release();

    if (FAILED(hr)) {
        Terminate();
    }

    return hr;
}

HRESULT RenderClass::InitBufferShader() {
    static const D3D11_INPUT_ELEMENT_DESC InputDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(CubeVertex, xyz), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(CubeVertex, normal), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(CubeVertex, uv), D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        OutputDebugString(L"COM initialization failed.\n");
        return hr;
    }

    ID3DBlob* pVertexCode = nullptr;
    if (SUCCEEDED(hr)) {
        hr = CompileShader(L"ColorVertex.vs", &m_pVertexShader, nullptr, &pVertexCode);
    }
    if (SUCCEEDED(hr)) {
        hr = CompileShader(L"ColorPixel.ps", nullptr, &m_pPixelShader);
    }

    if (SUCCEEDED(hr)) {
        hr = m_pDevice->CreateInputLayout(InputDesc, 3, pVertexCode->GetBufferPointer(), pVertexCode->GetBufferSize(), &m_pLayout);
    }

    if (pVertexCode)
        pVertexCode->Release();

    if (SUCCEEDED(hr)) {
        hr = CompileShader(L"LightPixel.ps", nullptr, &m_pLightPixelShader);
    }

    static const CubeVertex vertices[] =
    {
        { {-1.0f, -1.0f,  1.0f}, { 0.0f,  -1.0f,  0.0f}, {0.0f, 1.0f} },
        { { 1.0f, -1.0f,  1.0f}, { 0.0f,  -1.0f,  0.0f}, {1.0f, 1.0f} },
        { { 1.0f, -1.0f, -1.0f}, { 0.0f,  -1.0f,  0.0f}, {1.0f, 0.0f} },
        { {-1.0f, -1.0f, -1.0f}, { 0.0f,  -1.0f,  0.0f}, {0.0f, 0.0f} },

        { {-1.0f,  1.0f, -1.0f}, { 0.0f,  1.0f, 0.0f}, {0.0f, 1.0f} },
        { { 1.0f,  1.0f, -1.0f}, { 0.0f,  1.0f, 0.0f}, {1.0f, 1.0f} },
        { { 1.0f,  1.0f,  1.0f}, { 0.0f,  1.0f, 0.0f}, {1.0f, 0.0f} },
        { {-1.0f,  1.0f,  1.0f}, { 0.0f,  1.0f, 0.0f}, {0.0f, 0.0f} },

        { { 1.0f, -1.0f, -1.0f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 1.0f} },
        { { 1.0f, -1.0f,  1.0f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 1.0f} },
        { { 1.0f,  1.0f,  1.0f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 0.0f} },
        { { 1.0f,  1.0f, -1.0f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 0.0f} },

        { {-1.0f, -1.0f,  1.0f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f} },
        { {-1.0f, -1.0f, -1.0f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f} },
        { {-1.0f,  1.0f, -1.0f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.0f} },
        { {-1.0f,  1.0f,  1.0f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 0.0f} },

        { { 1.0f, -1.0f,  1.0f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 1.0f} },
        { {-1.0f, -1.0f,  1.0f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 1.0f} },
        { {-1.0f,  1.0f,  1.0f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 0.0f} },
        { { 1.0f,  1.0f,  1.0f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 0.0f} },

        { {-1.0f, -1.0f, -1.0f}, { 0.0f,  0.0f,  -1.0f}, {0.0f, 1.0f} },
        { { 1.0f, -1.0f, -1.0f}, { 0.0f,  0.0f,  -1.0f}, {1.0f, 1.0f} },
        { { 1.0f,  1.0f, -1.0f}, { 0.0f,  0.0f,  -1.0f}, {1.0f, 0.0f} },
        { {-1.0f,  1.0f, -1.0f}, { 0.0f,  0.0f,  -1.0f}, {0.0f, 0.0f} },
    };

    WORD indices[] = {
        0, 2, 1, 0, 3, 2, 4, 6, 5, 4, 7, 6, 8, 10, 9, 8, 11, 10,
        12, 14, 13, 12, 15, 14, 16, 18, 17, 16, 19, 18, 20, 22, 21, 20, 23, 22
    };

    D3D11_BUFFER_DESC lightBufferDesc = {};
    lightBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    lightBufferDesc.ByteWidth = sizeof(PointLight) * 3;
    lightBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    lightBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = m_pDevice->CreateBuffer(&lightBufferDesc, nullptr, &m_pLightBuffer);
    if (FAILED(hr)) return hr;

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(CubeVertex) * ARRAYSIZE(vertices);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;
    hr = m_pDevice->CreateBuffer(&bd, &initData, &m_pVertexBuffer);
    if (FAILED(hr))
        return hr;

    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(WORD) * ARRAYSIZE(indices);
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;
    initData.pSysMem = indices;
    hr = m_pDevice->CreateBuffer(&bd, &initData, &m_pIndexBuffer);
    if (FAILED(hr))
        return hr;

    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(XMMATRIX);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    hr = m_pDevice->CreateBuffer(&bd, nullptr, &m_pModelBuffer);
    if (FAILED(hr))
        return hr;

    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(InstanceData) * MaxInst;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    hr = m_pDevice->CreateBuffer(&bd, nullptr, &m_pModelBufferInst);
    if (FAILED(hr))
        return hr;

    const int innerCount = 10;
    const int outerCount = 12;
    const float innerRadius = 4.0f;
    const float outerRadius = 9.5f;

    InstanceData modelBuf;
    modelBuf.countInstance = MaxInst;
    modelBuf.model = XMMatrixScaling(m_fixedScale, m_fixedScale, m_fixedScale);
    modelBuf.texInd = 0;
    m_modelInstances.push_back(modelBuf);

    for (int i = 0; i < innerCount; i++)
    {
        float angle = XM_2PI * i / innerCount;
        XMFLOAT3 position = { innerRadius * cosf(angle), 0.0f, innerRadius * sinf(angle) };

        InstanceData modelBuf;
        modelBuf.countInstance = MaxInst;
        modelBuf.model = XMMatrixScaling(m_fixedScale, m_fixedScale, m_fixedScale) *
            XMMatrixTranslation(position.x, position.y, position.z);
        modelBuf.texInd = i % 2;
        m_modelInstances.push_back(modelBuf);
    }

    for (int i = 0; i < outerCount; i++)
    {
        float angle = XM_2PI * i / outerCount;
        XMFLOAT3 position = { outerRadius * cosf(angle), 0.0f, outerRadius * sinf(angle) };

        InstanceData modelBuf;
        modelBuf.countInstance = MaxInst;
        modelBuf.model = XMMatrixScaling(m_fixedScale, m_fixedScale, m_fixedScale) *
            XMMatrixTranslation(position.x, position.y, position.z);
        modelBuf.texInd = i % 2;
        m_modelInstances.push_back(modelBuf);
    }

    m_pDeviceContext->UpdateSubresource(m_pModelBufferInst, 0, nullptr, m_modelInstances.data(), 0, 0);


    D3D11_BUFFER_DESC vpBufferDesc = {};
    vpBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    vpBufferDesc.ByteWidth = sizeof(CameraBuffer); 
    vpBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    vpBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = m_pDevice->CreateBuffer(&vpBufferDesc, nullptr, &m_pVPBuffer);
    if (FAILED(hr))
        return hr;

   

    hr = CreateDDSTextureFromFile(m_pDevice, L"cube_normal.dds", nullptr, &m_pNormalMapView);
    if (FAILED(hr))
        return hr;


    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = m_pDevice->CreateSamplerState(&sampDesc, &m_pSamplerState);
   
    return hr;
}

HRESULT RenderClass::InitComputeShader()
{
    // Компиляция вычислительного шейдера
    HRESULT hr = CompileComputeShader(L"ComputeShader.cs", &m_pComputeShader);
    if (FAILED(hr))
        return hr;

    // Буфер фрустума
    D3D11_BUFFER_DESC descFrustum = {};
    descFrustum.ByteWidth = sizeof(XMVECTOR) * 6;
    descFrustum.Usage = D3D11_USAGE_DYNAMIC;
    descFrustum.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    descFrustum.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = m_pDevice->CreateBuffer(&descFrustum, nullptr, &m_pFrustumPlanesBuffer);
    if (FAILED(hr))
        return hr;

    // Буфер для индиректных аргументов
    D3D11_BUFFER_DESC descArgs = {};
    descArgs.ByteWidth = sizeof(UINT) * 5;
    descArgs.Usage = D3D11_USAGE_DEFAULT;
    descArgs.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    descArgs.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS | D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;

    hr = m_pDevice->CreateBuffer(&descArgs, nullptr, &m_pIndirectArgsBuffer);
    if (FAILED(hr))
        return hr;

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavArgs = {};
    uavArgs.Format = DXGI_FORMAT_R32_TYPELESS;
    uavArgs.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavArgs.Buffer.FirstElement = 0;
    uavArgs.Buffer.NumElements = descArgs.ByteWidth / sizeof(UINT);
    uavArgs.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;

    hr = m_pDevice->CreateUnorderedAccessView(m_pIndirectArgsBuffer, &uavArgs, &m_pIndirectArgsUAV);
    if (FAILED(hr))
        return hr;

    // Буфер идентификаторов объектов
    D3D11_BUFFER_DESC descIDs = {};
    descIDs.ByteWidth = sizeof(UINT) * MaxInst;
    descIDs.Usage = D3D11_USAGE_DEFAULT;
    descIDs.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    descIDs.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    descIDs.StructureByteStride = sizeof(UINT);

    hr = m_pDevice->CreateBuffer(&descIDs, nullptr, &m_pObjectsIdsBuffer);
    if (FAILED(hr))
        return hr;

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavIDs = {};
    uavIDs.Format = DXGI_FORMAT_UNKNOWN;
    uavIDs.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavIDs.Buffer.FirstElement = 0;
    uavIDs.Buffer.NumElements = MaxInst;

    hr = m_pDevice->CreateUnorderedAccessView(m_pObjectsIdsBuffer, &uavIDs, &m_pObjectsIdsUAV);
    if (FAILED(hr))
        return hr;

    // Буфер данных экземпляров
    D3D11_BUFFER_DESC descInstances = {};
    descInstances.ByteWidth = sizeof(InstanceData) * MaxInst;
    descInstances.Usage = D3D11_USAGE_DEFAULT;
    descInstances.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    descInstances.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    descInstances.StructureByteStride = sizeof(InstanceData);

    ID3D11Buffer* pTempInstanceBuffer = nullptr;
    hr = m_pDevice->CreateBuffer(&descInstances, nullptr, &pTempInstanceBuffer);
    if (FAILED(hr))
        return hr;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvInstances = {};
    srvInstances.Format = DXGI_FORMAT_UNKNOWN;
    srvInstances.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvInstances.Buffer.FirstElement = 0;
    srvInstances.Buffer.NumElements = MaxInst;

    hr = m_pDevice->CreateShaderResourceView(pTempInstanceBuffer, &srvInstances, &m_pInstanceDataSRV);
    if (FAILED(hr))
    {
        pTempInstanceBuffer->Release();
        return hr;
    }

    // Обновление содержимого буфера
    m_pDeviceContext->UpdateSubresource(pTempInstanceBuffer, 0, nullptr, m_modelInstances.data(), 0, 0);
    pTempInstanceBuffer->Release();

    return S_OK;
}

HRESULT RenderClass::CompileComputeShader(const std::wstring& path, ID3D11ComputeShader** ppComputeShader) {
    std::wstring ext = Extension(path);
    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    ID3DBlob* pCode = nullptr;
    ID3DBlob* pErr = nullptr;
    HRESULT hr = D3DCompileFromFile(path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "cs_5_0", flags, 0, &pCode, &pErr);
    if (FAILED(hr) && pErr)
        OutputDebugStringA(reinterpret_cast<const char*>(pErr->GetBufferPointer()));
    if (pErr)
        pErr->Release();
    if (SUCCEEDED(hr))
        hr = m_pDevice->CreateComputeShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, ppComputeShader);
    if (pCode)
        pCode->Release();
    return hr;
}


void RenderClass::TerminateComputeShader()
{
    if (m_pComputeShader)
    {
        m_pComputeShader->Release();
        m_pComputeShader = nullptr;
    }

    if (m_pFrustumPlanesBuffer)
    {
        m_pFrustumPlanesBuffer->Release();
        m_pFrustumPlanesBuffer = nullptr;
    }

    if (m_pIndirectArgsBuffer)
    {
        m_pIndirectArgsBuffer->Release();
        m_pIndirectArgsBuffer = nullptr;
    }

    if (m_pObjectsIdsBuffer)
    {
        m_pObjectsIdsBuffer->Release();
        m_pObjectsIdsBuffer = nullptr;
    }

    if (m_pIndirectArgsUAV)
    {
        m_pIndirectArgsUAV->Release();
        m_pIndirectArgsUAV = nullptr;
    }

    if (m_pObjectsIdsUAV)
    {
        m_pObjectsIdsUAV->Release();
        m_pObjectsIdsUAV = nullptr;
    }

    if (m_pInstanceDataSRV)
    {
        m_pInstanceDataSRV->Release();
        m_pInstanceDataSRV = nullptr;
    }
}

std::vector<UINT> RenderClass::ReadUintBufferData(ID3D11DeviceContext* pContext, ID3D11Buffer* pBuffer, UINT count) {
    std::vector<UINT> output(count);
    D3D11_BUFFER_DESC origDesc = {};
    pBuffer->GetDesc(&origDesc);
    D3D11_BUFFER_DESC stagingDesc = origDesc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.MiscFlags = 0;
    ID3D11Buffer* pStaging = nullptr;
    if (SUCCEEDED(m_pDevice->CreateBuffer(&stagingDesc, nullptr, &pStaging))) {
        pContext->CopyResource(pStaging, pBuffer);
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (SUCCEEDED(pContext->Map(pStaging, 0, D3D11_MAP_READ, 0, &mapped))) {
            memcpy(output.data(), mapped.pData, count * sizeof(UINT));
            pContext->Unmap(pStaging, 0);
        }
        pStaging->Release();
    }
    return output;
}

HRESULT RenderClass::Init2DArray()
{
    ID3D11Resource* pTextureResources[2] = { nullptr, nullptr };

    HRESULT result = CreateWICTextureFromFileEx(m_pDevice, m_pDeviceContext, L"cat.png", 0, D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, D3D11_CPU_ACCESS_FLAG(0), D3D11_RESOURCE_MISC_GENERATE_MIPS, WIC_LOADER_DEFAULT, &pTextureResources[0], &m_pTextureView);

    if (FAILED(result))
        return result;

    m_pDeviceContext->GenerateMips(m_pTextureView);

    result = CreateWICTextureFromFileEx(m_pDevice, m_pDeviceContext, L"textile.png", 0, D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, D3D11_CPU_ACCESS_FLAG(0), D3D11_RESOURCE_MISC_GENERATE_MIPS, WIC_LOADER_DEFAULT, &pTextureResources[1], &m_pTextureView);

    if (FAILED(result))
        return result;

    m_pDeviceContext->GenerateMips(m_pTextureView);

    ID3D11Texture2D* pTexture = nullptr;
    pTextureResources[0]->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&pTexture);


    D3D11_TEXTURE2D_DESC texDesc;
    pTexture->GetDesc(&texDesc);
    pTexture->Release();

    D3D11_TEXTURE2D_DESC arrayDesc = texDesc;
    arrayDesc.ArraySize = 2;

    ID3D11Texture2D* pTextureArray = nullptr;
    result = m_pDevice->CreateTexture2D(&arrayDesc, nullptr, &pTextureArray);
    if (FAILED(result))
    {
        pTextureResources[0]->Release();
        pTextureResources[1]->Release();
        return result;
    }

    for (UINT texIndex = 0; texIndex < 2; ++texIndex)
    {
        for (UINT mipLevel = 0; mipLevel < texDesc.MipLevels; ++mipLevel)
        {
            if (pTextureResources[texIndex])
            {
                m_pDeviceContext->CopySubresourceRegion(
                    pTextureArray,
                    D3D11CalcSubresource(mipLevel, texIndex, texDesc.MipLevels),
                    0, 0, 0,
                    pTextureResources[texIndex],
                    mipLevel,
                    nullptr
                );
            }
            else {
                OutputDebugString(L"Warning: Attempted to copy from a null texture resource.\n");
            }
        }
    }

    for (int i = 0; i < 2; ++i)
    {
        if (pTextureResources[i])
            pTextureResources[i]->Release();
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Texture2DArray.ArraySize = 2;
    srvDesc.Texture2DArray.FirstArraySlice = 0;
    srvDesc.Texture2DArray.MipLevels = texDesc.MipLevels;
    srvDesc.Texture2DArray.MostDetailedMip = 0;

    result = m_pDevice->CreateShaderResourceView(pTextureArray, &srvDesc, &m_pTextureView);
    pTextureArray->Release();
    return result;
  
}

HRESULT RenderClass::InitFullScreenTriangle()
{
    // Определяем вершины полноэкранного треугольника
    FullScreenVertex vertices[3] =
    {
        { -1.0f, -1.0f, 0, 1,  0.0f,  1.0f },
        { -1.0f,  3.0f, 0, 1,  0.0f, -1.0f },
        {  3.0f, -1.0f, 0, 1,  2.0f,  1.0f }
    };

    // Описание буфера вершин
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.ByteWidth = sizeof(vertices);
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    // Данные для инициализации буфера
    D3D11_SUBRESOURCE_DATA vertexData = {};
    vertexData.pSysMem = vertices;

    // Создаём буфер вершин
    HRESULT hr = m_pDevice->CreateBuffer(&bufferDesc, &vertexData, &m_pFullScreenVB);
    if (FAILED(hr))
        return hr;

    // Описание входного макета для вершинного шейдера
    D3D11_INPUT_ELEMENT_DESC inputLayoutDesc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    ID3DBlob* pVSBlob = nullptr;

    // Компиляция вершинного шейдера
    hr = CompileShader(L"NegativeVertex.vs", &m_pPostProcessVS, nullptr, &pVSBlob);
    if (FAILED(hr))
        return hr;

    // Создание входного макета на основе описания и скомпилированного шейдера
    hr = m_pDevice->CreateInputLayout(
        inputLayoutDesc,
        ARRAYSIZE(inputLayoutDesc),
        pVSBlob->GetBufferPointer(),
        pVSBlob->GetBufferSize(),
        &m_pFullScreenLayout
    );

    if (pVSBlob)
        pVSBlob->Release();

    if (FAILED(hr))
        return hr;

    // Компиляция пиксельного шейдера
    hr = CompileShader(L"NegativePixel.ps", nullptr, &m_pPostProcessPS);
    return hr;
}



HRESULT RenderClass::LoadCubemapFropCrossImage(ID3D11Device* device, ID3D11DeviceContext* context, const wchar_t* filename, ID3D11ShaderResourceView** cubeSRV) {
    ComPtr<ID3D11Resource> originalTexture;
    ComPtr<ID3D11ShaderResourceView> originalSVR;
    HRESULT hr = CreateWICTextureFromFile(device, filename, originalTexture.GetAddressOf(), originalSVR.GetAddressOf());
    if (FAILED(hr)) {
        return hr;
    }

    ComPtr<ID3D11Texture2D> texture2D;
    originalTexture.As(&texture2D);
    D3D11_TEXTURE2D_DESC srcDesc;
    texture2D->GetDesc(&srcDesc);

    int faceSize = srcDesc.Width / 4;

    if (srcDesc.Height != faceSize * 3) {
        return E_FAIL;
    }

    D3D11_TEXTURE2D_DESC cubeDesc = {};
    cubeDesc.Width = faceSize;
    cubeDesc.Height = faceSize;
    cubeDesc.MipLevels = 1;
    cubeDesc.ArraySize = 6;
    cubeDesc.Format = srcDesc.Format;
    cubeDesc.SampleDesc.Count = 1;
    cubeDesc.Usage = D3D11_USAGE_DEFAULT;
    cubeDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    cubeDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    ComPtr<ID3D11Texture2D> cubeTexture;
    hr = device->CreateTexture2D(&cubeDesc, nullptr, cubeTexture.GetAddressOf());
    if (FAILED(hr)) return hr;

    D3D11_BOX box;
    for (int i = 0; i < 6; ++i) {
        int x = 0, y = 0;
        switch (i) {
        case 0: x = 2 * faceSize; y = faceSize; break;
        case 1: x = 0 * faceSize; y = faceSize; break;
        case 2: x = 1 * faceSize; y = 0 * faceSize; break;
        case 3: x = 1 * faceSize; y = 2 * faceSize; break;
        case 4: x = 1 * faceSize; y = faceSize; break;
        case 5: x = 3 * faceSize; y = faceSize; break;
        }

        box.left = x;
        box.right = x + faceSize;
        box.top = y;
        box.bottom = y + faceSize;
        box.front = 0;
        box.back = 1;

        context->CopySubresourceRegion(cubeTexture.Get(), D3D11CalcSubresource(0, i, 1),
            0, 0, 0, texture2D.Get(), 0, &box);
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = cubeDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.MipLevels = 1;

    hr = device->CreateShaderResourceView(cubeTexture.Get(), &srvDesc, cubeSRV);
    return hr;
}

HRESULT RenderClass::InitSkybox() {
    ID3DBlob* pVertexCode = nullptr;
    HRESULT hr = CompileShader(L"SkyboxVertex.vs", &m_pSkyboxVS, nullptr, &pVertexCode);
    if (SUCCEEDED(hr)) {
        hr = CompileShader(L"SkyboxPixel.ps", nullptr, &m_pSkyboxPS);
    }

    D3D11_INPUT_ELEMENT_DESC skyboxLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    if (SUCCEEDED(hr)) {
        hr = m_pDevice->CreateInputLayout(skyboxLayout, 1, pVertexCode->GetBufferPointer(), pVertexCode->GetBufferSize(), &m_pSkyboxLayout);
    }

    SkyboxVertex SkyboxVertices[] = {
        { -1.0f, -1.0f, -1.0f },
        { -1.0f,  1.0f, -1.0f },
        {  1.0f,  1.0f, -1.0f },
        { -1.0f, -1.0f, -1.0f },
        {  1.0f,  1.0f, -1.0f },
        {  1.0f, -1.0f, -1.0f },
        {  1.0f, -1.0f,  1.0f },
        {  1.0f,  1.0f,  1.0f },
        { -1.0f,  1.0f,  1.0f },
        {  1.0f, -1.0f,  1.0f },
        { -1.0f,  1.0f,  1.0f },
        { -1.0f, -1.0f,  1.0f },
        { -1.0f, -1.0f,  1.0f },
        { -1.0f,  1.0f,  1.0f },
        { -1.0f,  1.0f, -1.0f },
        { -1.0f, -1.0f,  1.0f },
        { -1.0f,  1.0f, -1.0f },
        { -1.0f, -1.0f, -1.0f },
        { 1.0f, -1.0f, -1.0f },
        { 1.0f,  1.0f, -1.0f },
        { 1.0f,  1.0f,  1.0f },
        { 1.0f, -1.0f, -1.0f },
        { 1.0f,  1.0f,  1.0f },
        { 1.0f, -1.0f,  1.0f },
        { -1.0f, 1.0f, -1.0f },
        { -1.0f, 1.0f,  1.0f },
        { 1.0f, 1.0f,  1.0f },
        { -1.0f, 1.0f, -1.0f },
        { 1.0f, 1.0f,  1.0f },
        { 1.0f, 1.0f, -1.0f },
        { -1.0f, -1.0f,  1.0f },
        { -1.0f, -1.0f, -1.0f },
        { 1.0f, -1.0f, -1.0f },
        { -1.0f, -1.0f,  1.0f },
        { 1.0f, -1.0f, -1.0f },
        { 1.0f, -1.0f,  1.0f },
    };

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(SkyboxVertex) * ARRAYSIZE(SkyboxVertices);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = SkyboxVertices;
    hr = m_pDevice->CreateBuffer(&bd, &initData, &m_pSkyboxVB);
    if (FAILED(hr))
        return hr;

    D3D11_BUFFER_DESC vpBufferDesc = {};
    vpBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    vpBufferDesc.ByteWidth = sizeof(CameraBuffer);
    vpBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    vpBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = m_pDevice->CreateBuffer(&vpBufferDesc, nullptr, &m_pSkyboxVPBuffer);
    if (FAILED(hr))
        return hr;

    hr = LoadCubemapFropCrossImage(m_pDevice, m_pDeviceContext, L"skybox.png", &m_pSkyboxSRV);
    if (FAILED(hr))
        return hr;

    return S_OK;
}

void RenderClass::Terminate() {
    TerminateBufferShader();
    TerminateSkybox();
    TerminateParallelogram();

    if (m_pDeviceContext) {
        m_pDeviceContext->ClearState();
        m_pDeviceContext->Release();
        m_pDeviceContext = nullptr;
    }

    if (m_pRenderTargetView) {
        m_pRenderTargetView->Release();
        m_pRenderTargetView = nullptr;
    }

    if (m_pDepthView) {
        m_pDepthView->Release();
        m_pDepthView = nullptr;
    }

    if (m_pSwapChain) {
        m_pSwapChain->Release();
        m_pSwapChain = nullptr;
    }

    if (m_pDevice) {
        m_pDevice->Release();
        m_pDevice = nullptr;
    }
    
    CoUninitialize();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void RenderClass::TerminateBufferShader() {
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

    if (m_pTextureView)
        m_pTextureView->Release();

    if (m_pSamplerState)
        m_pSamplerState->Release();

    if (m_pLightBuffer)
        m_pLightBuffer->Release();

    if (m_pLightPixelShader)
        m_pLightPixelShader->Release();

    if (m_pNormalMapView)
        m_pNormalMapView->Release();

    if (m_pModelBufferInst)
        m_pModelBufferInst->Release();

    if (m_pPostProcessTexture)
        m_pPostProcessTexture->Release();

    if (m_pPostProcessRTV)
        m_pPostProcessRTV->Release();

    if (m_pPostProcessSRV)
        m_pPostProcessSRV->Release();

    if (m_pPostProcessVS)
        m_pPostProcessVS->Release();

    if (m_pPostProcessPS)
        m_pPostProcessPS->Release();

    if (m_pFullScreenVB)
        m_pFullScreenVB->Release();

    if (m_pFullScreenLayout)
        m_pFullScreenLayout->Release();

    m_modelInstances.clear();
}

void RenderClass::TerminateSkybox() {
    if (m_pSkyboxVB) {
        m_pSkyboxVB->Release();
    }

    if (m_pSkyboxSRV) {
        m_pSkyboxSRV->Release();
    }

    if (m_pSkyboxVPBuffer) {
        m_pSkyboxVPBuffer->Release();
    }

    if (m_pSkyboxLayout) {
        m_pSkyboxLayout->Release();
    }

    if (m_pSkyboxVS) {
        m_pSkyboxVS->Release();
    }

    if (m_pSkyboxPS) {
        m_pSkyboxPS->Release();
    }
}



HRESULT RenderClass::CompileShader(const std::wstring& path, ID3D11VertexShader** ppVertexShader, ID3D11PixelShader** ppPixelShader, ID3DBlob** pCodeShader) {
    std::wstring extension = Extension(path);

    std::string platform = "";

    if (extension == L"vs") {
        platform = "vs_5_0";
    }
    else if (extension == L"ps") {
        platform = "ps_5_0";
    }

    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob* pCode = nullptr;
    ID3DBlob* pErr = nullptr;

    HRESULT hr = D3DCompileFromFile(path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", platform.c_str(), 0, 0, &pCode, &pErr);
    if (!SUCCEEDED(hr) && pErr != nullptr) {
        OutputDebugStringA((const char*)pErr->GetBufferPointer());
    }
    if (pErr)
        pErr->Release();

    if (SUCCEEDED(hr)) {
        if (extension == L"vs" && ppVertexShader) {
            hr = m_pDevice->CreateVertexShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, ppVertexShader);
            if (FAILED(hr)) {
                pCode->Release();
                return hr;
            }
        }
        else if (extension == L"ps" && ppPixelShader) {
            hr = m_pDevice->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, ppPixelShader);
            if (FAILED(hr)) {
                pCode->Release();
                return hr;
            }
        }
    }

    if (pCodeShader) {
        *pCodeShader = pCode;
    }
    else {
        pCode->Release();
    }
    return hr;
}

void RenderClass::UpdateFrustum(const XMMATRIX& viewProjMatrix)
{
    // Преобразуем матрицу из SIMD в обычный формат для удобства работы
    XMFLOAT4X4 matrix;
    XMStoreFloat4x4(&matrix, viewProjMatrix);

    // Плоскости усечённой пирамиды (лево, право, низ, верх, ближняя, дальняя)
    m_frustumPlanes[0] = XMVectorSet(
        matrix._14 + matrix._11,
        matrix._24 + matrix._21,
        matrix._34 + matrix._31,
        matrix._44 + matrix._41
    ); // Левая грань

    m_frustumPlanes[1] = XMVectorSet(
        matrix._14 - matrix._11,
        matrix._24 - matrix._21,
        matrix._34 - matrix._31,
        matrix._44 - matrix._41
    ); // Правая грань

    m_frustumPlanes[2] = XMVectorSet(
        matrix._14 + matrix._12,
        matrix._24 + matrix._22,
        matrix._34 + matrix._32,
        matrix._44 + matrix._42
    ); // Нижняя грань

    m_frustumPlanes[3] = XMVectorSet(
        matrix._14 - matrix._12,
        matrix._24 - matrix._22,
        matrix._34 - matrix._32,
        matrix._44 - matrix._42
    ); // Верхняя грань

    m_frustumPlanes[4] = XMVectorSet(
        matrix._13,
        matrix._23,
        matrix._33,
        matrix._43
    ); // Ближняя грань

    m_frustumPlanes[5] = XMVectorSet(
        matrix._14 - matrix._13,
        matrix._24 - matrix._23,
        matrix._34 - matrix._33,
        matrix._44 - matrix._43
    ); // Дальняя грань

    // Нормализуем все плоскости
    for (int i = 0; i < 6; ++i)
    {
        m_frustumPlanes[i] = XMPlaneNormalize(m_frustumPlanes[i]);
    }
}

bool RenderClass::IsAABBInFrustum(const XMFLOAT3& center, float size) const
{
    // Проверка пересечения AABB с каждой плоскостью пирамиды
    for (int i = 0; i < 6; ++i)
    {
        // Расстояние от центра AABB до плоскости
        float distance = XMVectorGetX(XMPlaneDotCoord(m_frustumPlanes[i], XMLoadFloat3(&center)));

        // Радиус AABB в направлении нормали плоскости
        float radius = size * (
            fabs(XMVectorGetX(m_frustumPlanes[i])) +
            fabs(XMVectorGetY(m_frustumPlanes[i])) +
            fabs(XMVectorGetZ(m_frustumPlanes[i]))
            );

        // Если полностью за плоскостью — не попадает в усечённую пирамиду
        if (distance + radius < 0.0f)
        {
            return false;
        }
    }
    return true;
}


void RenderClass::MoveCamera(float dx, float dy, float dz) {
    m_CameraPosition.x += dx * m_CameraSpeed;
    m_CameraPosition.y += dy * m_CameraSpeed;
    m_CameraPosition.z += dz * m_CameraSpeed;
}

void RenderClass::RotateCamera(float lrAngle, float udAngle) {
    m_LRAngle += lrAngle;
    m_UDAngle -= udAngle;

    m_LRAngle = fmodf(m_LRAngle, XM_2PI);
    if (m_LRAngle > XM_PI) m_LRAngle -= XM_2PI;
    if (m_LRAngle < -XM_PI) m_LRAngle += XM_2PI;

    if (m_UDAngle > XM_PIDIV2) m_UDAngle = XM_PIDIV2;
    if (m_UDAngle < -XM_PIDIV2) m_UDAngle = -XM_PIDIV2;
}

void RenderClass::Render() {
    ID3D11ShaderResourceView* nullSRVs[1] = { nullptr };
    m_pDeviceContext->PSSetShaderResources(0, 1, nullSRVs);
    m_pDeviceContext->VSSetShaderResources(0, 1, nullSRVs);

    float clearColor[4] = { 0.48f, 0.57f, 0.48f, 1.0f };
    m_pDeviceContext->ClearRenderTargetView(m_pPostProcessRTV, clearColor);
    m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, clearColor);
    m_pDeviceContext->ClearDepthStencilView(m_pDepthView, D3D11_CLEAR_DEPTH, 1.0f, 0);

    m_pDeviceContext->OMSetRenderTargets(1, &m_pPostProcessRTV, m_pDepthView);

    XMMATRIX rotLR = XMMatrixRotationY(m_LRAngle);
    XMMATRIX rotUD = XMMatrixRotationX(m_UDAngle);
    XMMATRIX totalRot;
    if (m_CameraPosition.z <= 0)
    {
        totalRot = rotLR * rotUD;
    }
    else
    {
        totalRot = rotUD * rotLR;
    }

    
    XMVECTOR eyePos = XMVectorSet(m_CameraPosition.x, m_CameraPosition.y, m_CameraPosition.z, 0.0f);
    XMVECTOR focusPoint = XMVectorAdd(eyePos, XMVector3TransformNormal(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), totalRot));
    XMMATRIX view = XMMatrixLookAtLH(eyePos, focusPoint, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

    RECT rc;
    GetClientRect(FindWindow(m_szWindowClass, m_szTitle), &rc);
    float aspect = static_cast<float>(rc.right - rc.left) / (rc.bottom - rc.top);
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspect, 0.1f, 100.0f);

    RenderSkybox(proj);

    RenderCubes(view, proj);

    RenderParallelogram();

    m_pDeviceContext->PSSetShaderResources(0, 1, nullSRVs);
    m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, nullptr);

    if (m_useNegative)
    {
        m_pDeviceContext->VSSetShader(m_pPostProcessVS, nullptr, 0);
        m_pDeviceContext->PSSetShader(m_pPostProcessPS, nullptr, 0);
        m_pDeviceContext->IASetInputLayout(m_pFullScreenLayout);

        m_pDeviceContext->PSSetShaderResources(0, 1, &m_pPostProcessSRV);
        m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);

        UINT stride = sizeof(FullScreenVertex);
        UINT offset = 0;
        m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pFullScreenVB, &stride, &offset);
        m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_pDeviceContext->Draw(3, 0);
    }
    else
    {
        ID3D11Resource* srcResource = nullptr;
        ID3D11Resource* dstResource = nullptr;

        m_pPostProcessSRV->GetResource(&srcResource);
        m_pRenderTargetView->GetResource(&dstResource);

        if (srcResource && dstResource)
        {
            m_pDeviceContext->CopyResource(dstResource, srcResource);
        }

        if (srcResource) srcResource->Release();
        if (dstResource) dstResource->Release();
    }

    RenderImGui();

    m_pSwapChain->Present(1, 0);
    m_pDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    m_pDeviceContext->PSSetShaderResources(0, 1, nullSRVs);
}

void RenderClass::SetMVPBuffer() {
    XMMATRIX rotLR = XMMatrixRotationY(m_LRAngle);
    XMMATRIX rotUD = XMMatrixRotationX(m_UDAngle);
    XMMATRIX totalRot = rotLR * rotUD;

    XMVECTOR defaultForward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    XMVECTOR direction = XMVector3TransformNormal(defaultForward, totalRot);

    XMVECTOR eyePos = XMVectorSet(m_CameraPosition.x, m_CameraPosition.y, m_CameraPosition.z, 0.0f);
    XMVECTOR focusPoint = XMVectorAdd(eyePos, direction);
    XMVECTOR upDir = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX view = XMMatrixLookAtLH(eyePos, focusPoint, upDir);

    RECT rc;
    GetClientRect(FindWindow(m_szWindowClass, m_szTitle), &rc);
    float aspect = static_cast<float>(rc.right - rc.left) / (rc.bottom - rc.top);
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspect, 0.1f, 100.0f);

    XMMATRIX rotLRSky = XMMatrixRotationY(-m_LRAngle);
    XMMATRIX rotUDSky = XMMatrixRotationX(-m_UDAngle);
    XMMATRIX viewSkybox = rotLRSky * rotUDSky;
    XMMATRIX vpSkybox = XMMatrixTranspose(viewSkybox * proj);

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(m_pDeviceContext->Map(m_pSkyboxVPBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        memcpy(mapped.pData, &vpSkybox, sizeof(XMMATRIX));
        m_pDeviceContext->Unmap(m_pSkyboxVPBuffer, 0);
    }

    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = true;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    ID3D11DepthStencilState* pDSStateSkybox = nullptr;
    m_pDevice->CreateDepthStencilState(&dsDesc, &pDSStateSkybox);
    m_pDeviceContext->OMSetDepthStencilState(pDSStateSkybox, 0);

    D3D11_RASTERIZER_DESC rsDesc = {};
    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.CullMode = D3D11_CULL_FRONT;
    rsDesc.FrontCounterClockwise = false;
    ID3D11RasterizerState* pSkyboxRS = nullptr;
    if (SUCCEEDED(m_pDevice->CreateRasterizerState(&rsDesc, &pSkyboxRS))) {
        m_pDeviceContext->RSSetState(pSkyboxRS);
    }

    UINT stride = sizeof(SkyboxVertex);
    UINT offset = 0;
    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pSkyboxVB, &stride, &offset);
    m_pDeviceContext->IASetInputLayout(m_pSkyboxLayout);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_pDeviceContext->VSSetShader(m_pSkyboxVS, nullptr, 0);
    m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pSkyboxVPBuffer);

    m_pDeviceContext->PSSetShader(m_pSkyboxPS, nullptr, 0);
    m_pDeviceContext->PSSetShaderResources(0, 1, &m_pSkyboxSRV);
    m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);

    m_pDeviceContext->Draw(36, 0);

    pDSStateSkybox->Release();
    if (pSkyboxRS) {
        pSkyboxRS->Release();
        m_pDeviceContext->RSSetState(nullptr);
    }

    m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthView);

    m_CubeAngle += 0.01f;
    if (m_CubeAngle > XM_2PI) m_CubeAngle -= XM_2PI;
    XMMATRIX model = XMMatrixRotationY(m_CubeAngle);

    XMMATRIX vp = view * proj;
    XMMATRIX mT = XMMatrixTranspose(model);
    XMMATRIX vpT = XMMatrixTranspose(vp);

    m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, &mT, 0, 0);

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = m_pDeviceContext->Map(m_pVPBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (SUCCEEDED(hr)) {
        memcpy(mappedResource.pData, &vpT, sizeof(XMMATRIX));
        m_pDeviceContext->Unmap(m_pVPBuffer, 0);
    }
    m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pModelBuffer);
    m_pDeviceContext->VSSetConstantBuffers(1, 1, &m_pVPBuffer);
}

HRESULT RenderClass::ConfigureBackBuffer(UINT width, UINT height) {
    if (m_pPostProcessTexture) m_pPostProcessTexture->Release();
    if (m_pPostProcessRTV) m_pPostProcessRTV->Release();
    if (m_pPostProcessSRV) m_pPostProcessSRV->Release();
    if (m_pRenderTargetView) m_pRenderTargetView->Release();
    if (m_pDepthView) m_pDepthView->Release();

    m_pPostProcessTexture = nullptr;
    m_pPostProcessRTV = nullptr;
    m_pPostProcessSRV = nullptr;
    m_pRenderTargetView = nullptr;
    m_pDepthView = nullptr;

    ID3D11Texture2D* pBackBuffer = nullptr;
    HRESULT hr = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    if (FAILED(hr))
        return hr;

    hr = m_pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_pRenderTargetView);
    pBackBuffer->Release();
    if (FAILED(hr))
        return hr;

    D3D11_TEXTURE2D_DESC descDepth = {};
    descDepth.Width = width;
    descDepth.Height = height;
    descDepth.MipLevels = 1;
    descDepth.ArraySize = 1;
    descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    descDepth.SampleDesc.Count = 1;
    descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ID3D11Texture2D* pDepthStencil = nullptr;
    hr = m_pDevice->CreateTexture2D(&descDepth, nullptr, &pDepthStencil);
    if (FAILED(hr))
        return hr;

    hr = m_pDevice->CreateDepthStencilView(pDepthStencil, nullptr, &m_pDepthView);
    pDepthStencil->Release();
    if (FAILED(hr))
        return hr;

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    hr = m_pDevice->CreateTexture2D(&texDesc, nullptr, &m_pPostProcessTexture);
    if (FAILED(hr)) return hr;

    hr = m_pDevice->CreateRenderTargetView(m_pPostProcessTexture, nullptr, &m_pPostProcessRTV);
    if (FAILED(hr)) return hr;

    hr = m_pDevice->CreateShaderResourceView(m_pPostProcessTexture, nullptr, &m_pPostProcessSRV);
    if (FAILED(hr)) return hr;

    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    m_pDeviceContext->RSSetViewports(1, &vp);

    return hr;
}

void RenderClass::Resize(HWND hWnd) {
    if (m_pRenderTargetView) {
        m_pRenderTargetView->Release();
        m_pRenderTargetView = nullptr;
    }

    if (m_pDepthView) {
        m_pDepthView->Release();
        m_pDepthView = nullptr;
    }

    if (m_pSwapChain) {
        HRESULT hr;

        RECT rc;
        GetClientRect(hWnd, &rc);
        UINT width = rc.right - rc.left;
        UINT height = rc.bottom - rc.top;

        hr = m_pSwapChain->ResizeBuffers(1, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
        if (FAILED(hr)) {
            MessageBox(nullptr, L"ResizeBuffers failed.", L"Error", MB_OK);
            return;
        }

        HRESULT resultBack = ConfigureBackBuffer(width, height);
        if (FAILED(resultBack)) {
            MessageBox(nullptr, L"Configure back buffer failed.", L"Error", MB_OK);
            return;
        }

        m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthView);

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
HRESULT RenderClass::InitParallelogram() {
    ID3DBlob* pVertBlob = nullptr;
    HRESULT hr = CompileShader(L"ParallelogramVertex.vs", &m_pParallelogramVS, nullptr, &pVertBlob);
    if (SUCCEEDED(hr))
        hr = CompileShader(L"ParallelogramPixel.ps", nullptr, &m_pParallelogramPS);
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    if (SUCCEEDED(hr))
        hr = m_pDevice->CreateInputLayout(layout, 1, pVertBlob->GetBufferPointer(), pVertBlob->GetBufferSize(), &m_pParallelogramLayout);
    ParallelogramVertex verts[] = {
        {-0.5f, -1.0f, 0.0f},
        {-0.2f,  1.0f, 0.0f},
        { 1.0f,  0.8f, 0.0f},
        { 0.8f, -0.8f, 0.0f}
    };
    WORD indices[] = { 0, 1, 2, 0, 2, 3 };
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(verts);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = verts;
    hr = m_pDevice->CreateBuffer(&bd, &initData, &m_pParallelogramVB);
    if (FAILED(hr))
        return hr;
    bd.ByteWidth = sizeof(indices);
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    initData.pSysMem = indices;
    hr = m_pDevice->CreateBuffer(&bd, &initData, &m_pParallelogramIB);
    if (FAILED(hr))
        return hr;
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(ColorBuffer);
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pColorBuffer);
    if (FAILED(hr))
        return hr;
    D3D11_BLEND_DESC bsDesc = {};
    bsDesc.RenderTarget[0].BlendEnable = true;
    bsDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bsDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bsDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bsDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bsDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    bsDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bsDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    hr = m_pDevice->CreateBlendState(&bsDesc, &m_pBlendState);
    if (FAILED(hr))
        return hr;
    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = true;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
    hr = m_pDevice->CreateDepthStencilState(&dsDesc, &m_pDepthStateParallelogram);
    if (FAILED(hr))
        return hr;
    return hr;
}

void RenderClass::TerminateParallelogram() {
    if (m_pParallelogramVB) m_pParallelogramVB->Release();
    if (m_pParallelogramIB) m_pParallelogramIB->Release();
    if (m_pParallelogramPS) m_pParallelogramPS->Release();
    if (m_pParallelogramVS) m_pParallelogramVS->Release();
    if (m_pParallelogramLayout) m_pParallelogramLayout->Release();
    if (m_pBlendState) m_pBlendState->Release();
    if (m_pDepthStateParallelogram) m_pDepthStateParallelogram->Release();
    if (m_pColorBuffer) m_pColorBuffer->Release();
}

void RenderClass::RenderParallelogram() {
    D3D11_RASTERIZER_DESC rsDesc = {};
    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.CullMode = D3D11_CULL_NONE;
    rsDesc.FrontCounterClockwise = false;

    ID3D11RasterizerState* pRS = nullptr;
    m_pDevice->CreateRasterizerState(&rsDesc, &pRS);
    m_pDeviceContext->RSSetState(pRS);
    m_pDeviceContext->OMSetDepthStencilState(m_pDepthStateParallelogram, 0);
    m_pDeviceContext->OMSetBlendState(m_pBlendState, nullptr, 0xffffffff);

    UINT stride = sizeof(ParallelogramVertex), offset = 0;

    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pParallelogramVB, &stride, &offset);
    m_pDeviceContext->IASetIndexBuffer(m_pParallelogramIB, DXGI_FORMAT_R16_UINT, 0);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->IASetInputLayout(m_pParallelogramLayout);
    m_pDeviceContext->VSSetShader(m_pParallelogramVS, nullptr, 0);
    m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pModelBuffer);
    m_pDeviceContext->VSSetConstantBuffers(1, 1, &m_pVPBuffer);
    m_pDeviceContext->PSSetShader(m_pParallelogramPS, nullptr, 0);
    m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pColorBuffer);

    static float t = 0.0f;
    t += 0.03f;

    XMMATRIX model1 = XMMatrixTranslation(sinf(t) * 2.5f, 0.8f, -3.0f);
    XMMATRIX m1T = XMMatrixTranspose(model1);
    XMFLOAT4 color1 = XMFLOAT4(0.0f, 0.5f, 0.5f, 0.5f);

    XMMATRIX model2 = XMMatrixTranslation(-cosf(t) * 2.5f, 0.8f, -2.0f);
    XMMATRIX m2T = XMMatrixTranspose(model2);
    XMFLOAT4 color2 = XMFLOAT4(0.5f, 0.0f, 0.5f, 0.5f);

    XMFLOAT3 pos1, pos2;
    XMStoreFloat3(&pos1, model1.r[3]);
    XMStoreFloat3(&pos2, model2.r[3]);

    XMVECTOR cam = XMLoadFloat3(&m_CameraPosition);

    XMVECTOR v1 = XMLoadFloat3(&pos1);
    XMVECTOR v2 = XMLoadFloat3(&pos2);

    float d1 = XMVectorGetX(XMVector3Length(v1 - cam));
    float d2 = XMVectorGetX(XMVector3Length(v2 - cam));

    if (d1 >= d2) {
        m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, &m1T, 0, 0);
        m_pDeviceContext->UpdateSubresource(m_pColorBuffer, 0, nullptr, &color1, 0, 0);
        m_pDeviceContext->DrawIndexed(6, 0, 0);
        m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, &m2T, 0, 0);
        m_pDeviceContext->UpdateSubresource(m_pColorBuffer, 0, nullptr, &color2, 0, 0);
        m_pDeviceContext->DrawIndexed(6, 0, 0);
    }
    else {
        m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, &m2T, 0, 0);
        m_pDeviceContext->UpdateSubresource(m_pColorBuffer, 0, nullptr, &color2, 0, 0);
        m_pDeviceContext->DrawIndexed(6, 0, 0);
        m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, &m1T, 0, 0);
        m_pDeviceContext->UpdateSubresource(m_pColorBuffer, 0, nullptr, &color1, 0, 0);
        m_pDeviceContext->DrawIndexed(6, 0, 0);
    }
    pRS->Release();
}

void RenderClass::RenderSkybox(XMMATRIX proj) {
    XMMATRIX rY = XMMatrixRotationY(-m_LRAngle);
    XMMATRIX rX = XMMatrixRotationX(-m_UDAngle);
    XMMATRIX vMat = rY * rX;
    XMMATRIX vpMat = XMMatrixTranspose(vMat * proj);
    D3D11_MAPPED_SUBRESOURCE mapRes;

    if (SUCCEEDED(m_pDeviceContext->Map(m_pSkyboxVPBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapRes))) {
        memcpy(mapRes.pData, &vpMat, sizeof(XMMATRIX));
        m_pDeviceContext->Unmap(m_pSkyboxVPBuffer, 0);
    }

    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = true;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

    ID3D11DepthStencilState* pDS = nullptr;
    m_pDevice->CreateDepthStencilState(&dsDesc, &pDS);
    m_pDeviceContext->OMSetDepthStencilState(pDS, 0);

    D3D11_RASTERIZER_DESC rsDesc = {};
    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.CullMode = D3D11_CULL_FRONT;
    rsDesc.FrontCounterClockwise = false;

    ID3D11RasterizerState* pRS = nullptr;
    if (SUCCEEDED(m_pDevice->CreateRasterizerState(&rsDesc, &pRS)))
        m_pDeviceContext->RSSetState(pRS);

    UINT stride = sizeof(SkyboxVertex), offset = 0;

    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pSkyboxVB, &stride, &offset);
    m_pDeviceContext->IASetInputLayout(m_pSkyboxLayout);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->VSSetShader(m_pSkyboxVS, nullptr, 0);
    m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pSkyboxVPBuffer);
    m_pDeviceContext->PSSetShader(m_pSkyboxPS, nullptr, 0);
    m_pDeviceContext->PSSetShaderResources(0, 1, &m_pSkyboxSRV);
    m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);
    m_pDeviceContext->Draw(36, 0);

    pDS->Release();
    if (pRS) { pRS->Release(); m_pDeviceContext->RSSetState(nullptr); }
}

void RenderClass::RenderCubes(XMMATRIX view, XMMATRIX proj)
{
    m_pDeviceContext->OMSetRenderTargets(1, &m_pPostProcessRTV, m_pDepthView); m_pDeviceContext->OMSetDepthStencilState(nullptr, 0);
    m_pDeviceContext->OMSetDepthStencilState(nullptr, 0);

    CameraBuffer cameraBuffer;
    cameraBuffer.vp = XMMatrixTranspose(view * proj);
    cameraBuffer.cameraPos = m_CameraPosition;

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = m_pDeviceContext->Map(m_pVPBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (SUCCEEDED(hr))
    {
        memcpy(mappedResource.pData, &cameraBuffer, sizeof(CameraBuffer));
        m_pDeviceContext->Unmap(m_pVPBuffer, 0);
    }

    UINT stride = sizeof(CubeVertex);
    UINT offset = 0;
    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);
    m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->IASetInputLayout(m_pLayout);

    m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);
    m_pDeviceContext->VSSetConstantBuffers(1, 1, &m_pVPBuffer);


    m_pDeviceContext->PSSetShaderResources(0, 1, &m_pTextureView);
    m_pDeviceContext->PSSetShaderResources(1, 1, &m_pNormalMapView);
    m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);

    UpdateFrustum(view * proj);

    m_CubeAngle += 0.01f;
    if (m_CubeAngle > XM_2PI) m_CubeAngle -= XM_2PI;

    if (m_pComputeShader)
    {
        for (int i = 0; i < m_modelInstances.size(); i++)
        {
            XMFLOAT3 position;
            XMStoreFloat3(&position, m_modelInstances[i].model.r[3]);
            m_modelInstances[i].model = XMMatrixScaling(m_fixedScale, m_fixedScale, m_fixedScale) *
                XMMatrixRotationY(m_CubeAngle) *
                XMMatrixTranslation(position.x, position.y, position.z);
        }
    
        D3D11_MAPPED_SUBRESOURCE mapped;
        if (SUCCEEDED(m_pDeviceContext->Map(m_pFrustumPlanesBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            memcpy(mapped.pData, m_frustumPlanes, sizeof(XMVECTOR) * 6);
            m_pDeviceContext->Unmap(m_pFrustumPlanesBuffer, 0);
        }

        UINT initialArgs[5] = { 36, 0, 0, 0, 0 };
        m_pDeviceContext->UpdateSubresource(m_pIndirectArgsBuffer, 0, nullptr, initialArgs, 0, 0);

        m_pDeviceContext->CSSetShader(m_pComputeShader, nullptr, 0);
        m_pDeviceContext->CSSetConstantBuffers(0, 1, &m_pFrustumPlanesBuffer);
        m_pDeviceContext->CSSetUnorderedAccessViews(0, 1, &m_pIndirectArgsUAV, nullptr);
        m_pDeviceContext->CSSetUnorderedAccessViews(1, 1, &m_pObjectsIdsUAV, nullptr);
        m_pDeviceContext->CSSetShaderResources(0, 1, &m_pInstanceDataSRV);

        m_pDeviceContext->Dispatch((MaxInst + 63) / 64, 1, 1);

        auto args = ReadUintBufferData(m_pDeviceContext, m_pIndirectArgsBuffer, 2);
        m_visibleCubes = args[1];

        if (m_visibleCubes > 0)
        {
            auto visibleIds = ReadUintBufferData(m_pDeviceContext, m_pObjectsIdsBuffer, m_visibleCubes);

            std::vector<InstanceData> visibleInstances(m_visibleCubes);
            for (UINT i = 0; i < m_visibleCubes; i++)
            {
                UINT id = visibleIds[i];
                visibleInstances[i].model = XMMatrixTranspose(m_modelInstances[id].model);
                visibleInstances[i].texInd = m_modelInstances[id].texInd;
            }

            m_pDeviceContext->UpdateSubresource(
                m_pModelBufferInst,
                0,
                nullptr,
                visibleInstances.data(),
                0,
                sizeof(InstanceData) * m_visibleCubes
            );
        }
    

        m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pModelBufferInst);
        m_pDeviceContext->DrawIndexedInstancedIndirect(m_pIndirectArgsBuffer, 0);

        ID3D11UnorderedAccessView* nullUAVs[2] = { nullptr, nullptr };
        m_pDeviceContext->CSSetUnorderedAccessViews(0, 2, nullUAVs, nullptr);
        ID3D11ShaderResourceView* nullSRVs[1] = { nullptr };
        m_pDeviceContext->CSSetShaderResources(0, 1, nullSRVs);
        m_pDeviceContext->CSSetShader(nullptr, nullptr, 0);
    }
    else
    {
        std::vector<InstanceData> visibleInstances;
        visibleInstances.reserve(m_modelInstances.size());
        m_visibleCubes = 0;

        for (int i = 0; i < m_modelInstances.size(); i++) {
            XMFLOAT3 position;
            XMStoreFloat3(&position, m_modelInstances[i].model.r[3]);

            m_modelInstances[i].model = XMMatrixScaling(m_fixedScale, m_fixedScale, m_fixedScale) *
                XMMatrixRotationY(m_CubeAngle) *
                XMMatrixTranslation(position.x, position.y, position.z);

            float size = m_fixedScale * 0.95f;
            if (IsAABBInFrustum(position, size))
            {
                InstanceData data;
                data.model = XMMatrixTranspose(m_modelInstances[i].model);
                data.texInd = m_modelInstances[i].texInd;
                visibleInstances.push_back(data);
                m_visibleCubes++;
            }
        }
        if (!visibleInstances.empty())
        {
            m_pDeviceContext->UpdateSubresource(
                m_pModelBufferInst,
                0,
                nullptr,
                visibleInstances.data(),
                0,
                sizeof(InstanceData) * m_visibleCubes
            );

            m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pModelBufferInst);
            m_pDeviceContext->DrawIndexedInstanced(36, visibleInstances.size(), 0, 0, 0);
        }
    }


    static float lightOrbit = 0;
    lightOrbit += 0.01f;
    if (lightOrbit > XM_2PI)
        lightOrbit -= XM_2PI;

  
    PointLight lights[3];
    float lightRadius = 2.0f;

    lights[0].Position = XMFLOAT3(0.0f, lightRadius * cosf(lightOrbit), lightRadius * sin(-lightOrbit));
    lights[0].Range = 3.0f;
    lights[0].Color = XMFLOAT3(1.0f, 1.0f, 1.0f);
    lights[0].Intensity = 1.0f;

    lights[1].Position = XMFLOAT3(lightRadius * cosf(lightOrbit), 0.0f, lightRadius * sin(lightOrbit));
    lights[1].Range = 3.0f;
    lights[1].Color = XMFLOAT3(1.0f, 1.0f, 0.13f);
    lights[1].Intensity = 1.0f;

    lightRadius = 8.0f;
    lights[2].Position = XMFLOAT3(lightRadius * cosf(lightOrbit), 0.0f, lightRadius * sinf(-lightOrbit));
    lights[2].Range = 5.0f;
    lights[2].Color = XMFLOAT3(1.0f, 1.0f, 1.0f);
    lights[2].Intensity = 1.0f;

    D3D11_MAPPED_SUBRESOURCE mappedResourceLight;
    hr = m_pDeviceContext->Map(m_pLightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResourceLight);
    if (SUCCEEDED(hr))
    {
        memcpy(mappedResourceLight.pData, lights, sizeof(PointLight) * 3);
        m_pDeviceContext->Unmap(m_pLightBuffer, 0);
    }

    m_pDeviceContext->PSSetConstantBuffers(2, 1, &m_pLightBuffer);
    m_pDeviceContext->PSSetShader(m_pLightPixelShader, nullptr, 0);

    for (int i = 0; i < 3; i++)
    {
        XMMATRIX lightModel = XMMatrixScaling(0.1f, 0.1f, 0.1f) * XMMatrixTranslation(lights[i].Position.x, lights[i].Position.y, lights[i].Position.z);
        XMMATRIX lightModelTransposed = XMMatrixTranspose(lightModel);
        XMFLOAT4X4 lightModelTStored;
        XMStoreFloat4x4(&lightModelTStored, lightModelTransposed);

        m_pDeviceContext->UpdateSubresource(m_pModelBufferInst, 0, nullptr, &lightModelTStored, 0, 0);
        m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pModelBufferInst);

        XMFLOAT4 lightColor = XMFLOAT4(lights[i].Color.x, lights[i].Color.y, lights[i].Color.z, 1.0f);
        m_pDeviceContext->UpdateSubresource(m_pColorBuffer, 0, nullptr, &lightColor, 0, 0);

        m_pDeviceContext->DrawIndexed(36, 0, 0);
    }
}


void RenderClass::InitImGui(HWND hWnd)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontDefault();

    ImGui::StyleColorsLight();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.97f, 0.97f, 0.98f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.50f, 0.90f, 0.45f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.20f, 0.50f, 0.90f, 0.80f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.50f, 0.90f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.26f, 0.54f, 0.85f, 0.40f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.54f, 0.85f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);

    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX11_Init(m_pDevice, m_pDeviceContext);
}

void RenderClass::RenderImGui()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_Once);
    ImGui::Begin("PostProcess", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Checkbox("Negative", &m_useNegative);
    ImGui::End();

    ImGui::SetNextWindowSize(ImVec2(300, 140), ImGuiCond_Once);
    ImGui::Begin("Clipping", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("All:     %d", MaxInst);
    ImGui::Text("Visible:    %d", m_visibleCubes);
    ImGui::Text("Cut off:  %d", MaxInst - m_visibleCubes);
    ImGui::End();

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

