﻿#include "framework.h"
#include "Lab2.h"
#include "RenderClass.h"
#include <dxgi.h>
#include <d3d11.h>
#include <string>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

#define MAX_LOADSTRING 100

// Глобальные переменные
WCHAR szTitle[MAX_LOADSTRING] = L"Advanced Lab2 Kazakevich";
WCHAR szWindowClass[MAX_LOADSTRING] = L"AdvancedLab2Class";

RenderClass* g_Render = nullptr; // Указатель на класс рендеринга

// Прототипы функций
ATOM RegisterWindowClass(HINSTANCE hInstance);
BOOL InitializeApplication(HINSTANCE hInstance, int nCmdShow);
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void HandleWindowResize(HWND hWnd);

// Точка входа в приложение
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Регистрация класса окна
    if (!RegisterWindowClass(hInstance))
    {
        OutputDebugString(_T("Не удалось зарегистрировать класс окна\n"));
        return FALSE;
    }

    // Инициализация приложения
    if (!InitializeApplication(hInstance, nCmdShow))
    {
        OutputDebugString(_T("Не удалось инициализировать приложение\n"));
        return FALSE;
    }

    // Основной цикл сообщений
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
            // Рендеринг, если нет сообщений
            if (g_Render)
            {
                g_Render->Render();
            }
        }
    }

    // Очистка ресурсов
    if (g_Render)
    {
        g_Render->Terminate();
        delete g_Render;
        g_Render = nullptr;
    }

    return (int)msg.wParam;
}

// Регистрация класса окна
ATOM RegisterWindowClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW; // Перерисовка при изменении размера
    wcex.lpfnWndProc = WindowProc; // Указатель на процедуру окна
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_LAB2));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

// Инициализация приложения
BOOL InitializeApplication(HINSTANCE hInstance, int nCmdShow)
{
    // Создание окна
    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0,
        nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        OutputDebugString(_T("Не удалось создать окно\n"));
        return FALSE;
    }

    // Инициализация рендерера
    g_Render = new RenderClass();
    if (FAILED(g_Render->Init(hWnd)))
    {
        OutputDebugString(_T("Не удалось инициализировать рендерер\n"));
        delete g_Render;
        g_Render = nullptr;
        return FALSE;
    }

    // Отображение и обновление окна
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

// Обработка сообщений окна
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SIZE:
        HandleWindowResize(hWnd); // Обработка изменения размера окна
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        // Дополнительная отрисовка, если необходимо
        EndPaint(hWnd, &ps);
    }
    break;

    case WM_DESTROY:
        PostQuitMessage(0); // Завершение приложения
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Обработка изменения размера окна
void HandleWindowResize(HWND hWnd)
{
    if (g_Render)
    {
        g_Render->Resize(hWnd);
    }
}
