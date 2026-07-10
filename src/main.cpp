#include <array>
#include <optional>
#include <Windows.h>
#include <imgui_impl_win32.h>

#include "Renderer.h"
#include "Vertex.h"

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

std::optional<WPARAM> ProcessWindowMessages();

std::array constexpr TriangleVertices = {
    Vertex{ .position = { 0.0f, 0.5f, 0.0f },   .color = { 1.0f, 0.0f, 0.0f, 1.0f } },
    Vertex{ .position = { 0.5f, -0.5f, 0.0f },  .color = { 0.0f, 1.0f, 0.0f, 1.0f } },
    Vertex{ .position = { -0.5f, -0.5f, 0.0f }, .color = { 0.0f, 0.0f, 1.0f, 1.0f } },
};

int Run(HINSTANCE const instanceHandle, int const showCommand = SW_SHOWNORMAL)
{
    LPCWSTR constexpr WindowClass = L"RadianceCascades";
    LPCWSTR constexpr WindowTitle = L"Radiance Cascades";

    WNDCLASSW const wndClass = {
        .lpfnWndProc = WndProc,
        .hInstance = instanceHandle,
        .lpszClassName = WindowClass,
    };

    RegisterClassW(&wndClass);

    HWND const hWnd = CreateWindowExW(
        0,
        WindowClass,
        WindowTitle,
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 1024, 1024,
        nullptr,
        nullptr,
        instanceHandle,
        nullptr
    );

    ShowWindow(hWnd, showCommand);
    UpdateWindow(hWnd);

    Renderer renderer(hWnd);
    renderer.SetVertices(TriangleVertices);

    while (true)
    {
        if (const auto ExitCode = ProcessWindowMessages())
        {
            return static_cast<int>(*ExitCode);
        }

        renderer.Render();
    }
}

int main()
{
    HINSTANCE const hInstance = GetModuleHandle(nullptr);

    return Run(hInstance);
}

int WINAPI WinMain(
    _In_ HINSTANCE const hInstance,
    _In_opt_ HINSTANCE,
    _In_ LPSTR,
    _In_ int const nShowCmd)
{
    return Run(hInstance, nShowCmd);
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND const windowHandle, UINT const message, WPARAM const wParam, LPARAM const lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(windowHandle, message, wParam, lParam))
    {
        return true;
    }

    switch (message)
    {
        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(windowHandle, message, wParam, lParam);
    }

    return 0;
}

std::optional<WPARAM> ProcessWindowMessages()
{
    MSG Message = {};
    while (PeekMessage(&Message, nullptr, 0, 0, PM_REMOVE))
    {
        if (Message.message == WM_QUIT)
        {
            return Message.wParam;
        }

        TranslateMessage(&Message);
        DispatchMessage(&Message);
    }

    return std::nullopt;
}
