#include <Windows.h>
#include <imgui_impl_win32.h>

#include <optional>
#include <cstdint>

#include "Renderer.h"
#include "Scene.h"

static void FillObstacleRectangle(
    Scene& scene,
    int const left,
    int const top,
    int const right,
    int const bottom)
{
    for (int y = top; y < bottom; ++y)
    {
        for (int x = left; x < right; ++x)
        {
            scene.SetObstacle(x, y, true);
        }
    }
}

static void FillEmitterCircle(
    Scene& scene,
    int const x,
    int const y,
    std::uint32_t const radius,
    DirectX::XMFLOAT3 const radiance)
{
    int const radiusInt = static_cast<int>(radius);

    for (int dy = -radiusInt; dy < radiusInt; ++dy)
    {
        for (int dx = -radiusInt; dx < radiusInt; ++dx)
        {
            if (dx * dx + dy * dy > radius * radius) continue;

            scene.SetEmissiveObstacle(x + dx, y + dy, radiance);
        }
    }
}

static Scene MakeSampleScene()
{
    Scene scene(1024u, 1024u);

    FillObstacleRectangle(scene, 440, 175, 500, 767);
    FillObstacleRectangle(scene, 125, 730, 410, 780);
    FillObstacleRectangle(scene, 700, 320, 750, 580);

    FillEmitterCircle(
        scene,
        180, 410,
        50,
        { 12.0f, 5.0f, 1.0f }
    );

    FillEmitterCircle(
        scene,
        820, 215,
        30,
        { 0.5f, 3.0f, 12.0f }
    );

    return scene;
}

static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
static std::optional<WPARAM> ProcessWindowMessages();

static int Run(HINSTANCE const instanceHandle, int const showCommand = SW_SHOWNORMAL)
{
    constexpr LPCWSTR WindowClass = L"RadianceCascades";
    constexpr LPCWSTR WindowTitle = L"Radiance Cascades";

    WNDCLASSW const wndClass{
        .lpfnWndProc = WndProc,
        .hInstance = instanceHandle,
        .lpszClassName = WindowClass,
    };

    RegisterClassW(&wndClass);

    HWND const window = CreateWindowExW(
        0, WindowClass, WindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1024, 1024,
        nullptr, nullptr, instanceHandle, nullptr
    );

    Renderer renderer(window);

    SetWindowLongPtr(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&renderer));
    ShowWindow(window, showCommand);
    UpdateWindow(window);

    Scene const SampleScene = MakeSampleScene();
    renderer.SetScene(SampleScene);

    while (true)
    {
        if (auto const exitCode = ProcessWindowMessages())
        {
            return static_cast<int>(*exitCode);
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
    switch (message)
    {
        case WM_SIZE:
            if (auto* const renderer = reinterpret_cast<Renderer*>(GetWindowLongPtr(windowHandle, GWLP_USERDATA)))
            {
                renderer->OnWindowSize(LOWORD(lParam), HIWORD(lParam), wParam == SIZE_MINIMIZED);
            }
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            break;
    }

    if (ImGui_ImplWin32_WndProcHandler(windowHandle, message, wParam, lParam))
    {
        return 1;
    }

    return DefWindowProc(windowHandle, message, wParam, lParam);
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
