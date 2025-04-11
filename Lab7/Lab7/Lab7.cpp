#include "framework.h"
#include "Lab7.h"
#include "RenderClass.h"
#include <dxgi.h>
#include <d3d11.h>
#include <string>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

#define MAX_LOADSTRING 100

// Глобальные переменные
WCHAR szTitle[MAX_LOADSTRING] = L"Advanced Lab7 Kazakevich";
WCHAR szWindowClass[MAX_LOADSTRING] = L"AdvancedLab5Class";
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

RenderClass* g_Render = nullptr; 

ATOM RegisterWindowClass(HINSTANCE hInstance);
BOOL InitializeApplication(HINSTANCE hInstance, int nCmdShow);
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void HandleWindowResize(HWND hWnd);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    if (!RegisterWindowClass(hInstance))
    {
        OutputDebugString(_T("Не удалось зарегистрировать класс окна\n"));
        return FALSE;
    }

    if (!InitializeApplication(hInstance, nCmdShow))
    {
        OutputDebugString(_T("Не удалось инициализировать приложение\n"));
        return FALSE;
    }

    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            if (g_Render)
                g_Render->Render();
        }
    }

    if (g_Render)
    {
        
        g_Render->Terminate();
        delete g_Render;
        g_Render = nullptr;
    }

    return (int)msg.wParam;
}

ATOM RegisterWindowClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WindowProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitializeApplication(HINSTANCE hInstance, int nCmdShow)
{
    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0,
        nullptr, nullptr, hInstance, nullptr);
    if (!hWnd)
    {
        OutputDebugString(_T("Не удалось создать окно\n"));
        return FALSE;
    }

    g_Render = new RenderClass();
    if (FAILED(g_Render->Init(hWnd, szTitle, szWindowClass)))
    {
        OutputDebugString(_T("Не удалось инициализировать рендерер\n"));
        delete g_Render;
        g_Render = nullptr;
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
        return true;
    switch (message)
    {
    case WM_SIZE:
        HandleWindowResize(hWnd);
        break;

    case WM_KEYDOWN:
        if (g_Render)
        {
            switch (wParam)
            {
            case 'W': // Вращение вверх
                g_Render->RotateCamera(0.0f, 0.01f);
                break;
            case 'S': // Вращение вниз
                g_Render->RotateCamera(0.0f, -0.01f);
                break;
            case 'A': // Вращение влево
                g_Render->RotateCamera(-0.01f, 0.0f);
                break;
            case 'D': // Вращение вправо
                g_Render->RotateCamera(0.01f, 0.0f);
                break;
            case VK_UP:
                g_Render->MoveCamera(0.0f, 1.0f, 0.0f);
                break;
            case VK_DOWN:
                g_Render->MoveCamera(0.0f, -1.0f, 0.0f);
                break;
            case VK_LEFT:
                g_Render->MoveCamera(-1.0f, 0.0f, 0.0f);
                break;
            case VK_RIGHT:
                g_Render->MoveCamera(1.0f, 0.0f, 0.0f);
                break;
            case VK_ADD:
            case 0xBB:
                g_Render->MoveCamera(0.0f, 0.0f, 1.0f);
                break;
            case VK_SUBTRACT:
            case 0xBD:
                g_Render->MoveCamera(0.0f, 0.0f, -1.0f);
                break;
            }
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void HandleWindowResize(HWND hWnd)
{
    if (g_Render)
        g_Render->Resize(hWnd);
}


