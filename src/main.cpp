#include <array>
#include <optional>
#include <Windows.h>
#include <imgui_impl_win32.h>

#include "Renderer.h"
#include "Scene.h"

// Screen-space scene data for a 1024 x 1024 render area.
// The origin is at the top-left, with +X pointing right and +Y pointing down.
constexpr std::array SampleCircles = {
    Circle {
        .center = { 179.2f, 409.6f },
        .radius = 51.2f,
        .emission = { 12.0f, 5.0f, 1.0f },
    },
    Circle {
        .center = { 819.2f, 215.04f },
        .radius = 30.72f,
        .emission = { 0.5f, 3.0f, 12.0f },
    },
};

constexpr std::array SampleBoxes = {
    Box {
        .center = { 471.04f, 471.04f },
        .halfExtent = { 30.72f, 296.96f },
    },
    Box {
        .center = { 266.24f, 757.76f },
        .halfExtent = { 143.36f, 25.6f },
    },
    Box {
        .center = { 727.04f, 450.56f },
        .halfExtent = { 25.6f, 128.0f },
    },
};

constexpr Scene SampleScene = {
    .circles = SampleCircles,
    .boxes = SampleBoxes,
};

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
std::optional<WPARAM> ProcessWindowMessages();

int Run(HINSTANCE const instanceHandle, int const showCommand = SW_SHOWNORMAL)
{
    constexpr LPCWSTR WindowClass = L"RadianceCascades";
    constexpr LPCWSTR WindowTitle = L"Radiance Cascades";

    WNDCLASSW const wndClass{
        .lpfnWndProc = WndProc,
        .hInstance = instanceHandle,
        .lpszClassName = WindowClass,
    };

    RegisterClassW(&wndClass);

    HWND const hWnd = CreateWindowExW(
        0, WindowClass, WindowTitle,
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 1024, 1024,
        nullptr, nullptr, instanceHandle, nullptr
    );

    ShowWindow(hWnd, showCommand);
    UpdateWindow(hWnd);

    Renderer renderer(hWnd);

    while (true)
    {
        if (const auto exitCode = ProcessWindowMessages())
        {
            return static_cast<int>(*exitCode);
        }

        renderer.Render(SampleScene);
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
    MSG message{};
    while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
    {
        if (message.message == WM_QUIT)
        {
            return message.wParam;
        }

        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    return std::nullopt;
}
