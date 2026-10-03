#include "Renderer.h"
#include "Scene.h"
#include "SceneEditor.h"

#include <Windows.h>
#include <imgui_impl_win32.h>
#include <windowsx.h>

#include <optional>
#include <cstdint>

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
            scene.SetObstacle(x, y);
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

struct AppState final
{
    Renderer* renderer;
    SceneEditor* editor;

    enum class StrokeButton
    {
        None,
        Left,
        Right,
        Middle,
    };

    StrokeButton activeButton = StrokeButton::None;

    UINT pendingWidth = 0;
    UINT pendingHeight = 0;
    bool resizePending = false;
    bool isInSizeMove = false;
    bool hasDeferredResize = false;
};

static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
static std::optional<WPARAM> ProcessWindowMessages();

static void QueuePendingResize(AppState& state)
{
    if (state.pendingWidth == 0 || state.pendingHeight == 0)
    {
        return;
    }

    state.renderer->OnWindowSize(state.pendingWidth, state.pendingHeight, false);
    state.resizePending = true;
}

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

    Renderer renderer{window};

    Scene scene = MakeSampleScene();
    renderer.SetScene(scene);

    SceneEditor editor{scene};

    AppState state{
        .renderer = &renderer,
        .editor = &editor
    };

    SetWindowLongPtr(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&state));
    ShowWindow(window, showCommand);
    UpdateWindow(window);

    auto drawUi = [&editor]
    {
        ImGui::Begin("Scene Editor");

        int brushRadius = editor.GetBrushRadius();
        if (ImGui::SliderInt("Brush Radius", &brushRadius, 1, 64))
        {
            editor.SetBrushRadius(brushRadius);
        }

        ImGui::Text("Left Click: Emissive");
        ImGui::Text("Right Click: Obstacle");
        ImGui::Text("Middle Click: Eraser");

        ImGui::End();
    };

    while (true)
    {
        if (auto const exitCode = ProcessWindowMessages())
        {
            return static_cast<int>(*exitCode);
        }

        renderer.BeginFrame();

        if (state.resizePending)
        {
            editor.EndStroke();
            state.activeButton = AppState::StrokeButton::None;

            if (GetCapture() == window)
            {
                ReleaseCapture();
            }
            static_cast<void>(editor.TakePendingChange());

            scene.Resize(state.pendingWidth, state.pendingHeight);
            renderer.SetScene(scene);

            state.resizePending = false;
        }
        else
        {
            renderer.UpdateScene(scene, editor.TakePendingChange());
        }

        renderer.Render(drawUi);
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

static bool HandleEditorMouseMessage(
    HWND const windowHandle,
    UINT const message,
    LPARAM const lParam,
    bool const imguiWantsMouse,
    AppState& state)
{
    auto const beginStroke = [&](SceneEditor::EditTool const tool, AppState::StrokeButton const button)
    {
        if (imguiWantsMouse || state.activeButton != AppState::StrokeButton::None)
        {
            return false;
        }

        POINT const position{
            .x = GET_X_LPARAM(lParam),
            .y = GET_Y_LPARAM(lParam),
        };

        state.editor->BeginStroke(tool, position);
        state.activeButton = button;
        SetCapture(windowHandle);
        return true;
    };

    auto const endStroke = [&](AppState::StrokeButton const button)
    {
        if (state.activeButton != button)
        {
            return false;
        }

        state.editor->EndStroke();
        state.activeButton = AppState::StrokeButton::None;

        if (GetCapture() == windowHandle)
        {
            ReleaseCapture();
        }

        return true;
    };

    switch (message)
    {
        case WM_LBUTTONDOWN:
            return beginStroke(SceneEditor::EditTool::Emissive, AppState::StrokeButton::Left);

        case WM_RBUTTONDOWN:
            return beginStroke(SceneEditor::EditTool::Obstacle, AppState::StrokeButton::Right);

        case WM_MBUTTONDOWN:
            return beginStroke(SceneEditor::EditTool::Eraser, AppState::StrokeButton::Middle);

        case WM_MOUSEMOVE:
            if (state.activeButton == AppState::StrokeButton::None)
            {
                return false;
            }

            state.editor->ContinueStroke({
                .x = GET_X_LPARAM(lParam),
                .y = GET_Y_LPARAM(lParam),
            });
            return true;

        case WM_LBUTTONUP:
            return endStroke(AppState::StrokeButton::Left);

        case WM_RBUTTONUP:
            return endStroke(AppState::StrokeButton::Right);

        case WM_MBUTTONUP:
            return endStroke(AppState::StrokeButton::Middle);

        case WM_CAPTURECHANGED:
        case WM_CANCELMODE:
        case WM_KILLFOCUS:
            if (state.activeButton == AppState::StrokeButton::None)
            {
                return false;
            }

            state.editor->EndStroke();
            state.activeButton = AppState::StrokeButton::None;
            return true;

        default:
            return false;
    }
}

LRESULT CALLBACK WndProc(HWND const windowHandle, UINT const message, WPARAM const wParam, LPARAM const lParam)
{
    auto* const state = reinterpret_cast<AppState*>(GetWindowLongPtr(windowHandle, GWLP_USERDATA));
    if (state == nullptr)
    {
        return DefWindowProc(windowHandle, message, wParam, lParam);
    }

    switch (message)
    {
        case WM_ENTERSIZEMOVE:
            state->isInSizeMove = true;
            state->hasDeferredResize = false;
            return 0;

        case WM_EXITSIZEMOVE:
            state->isInSizeMove = false;
            if (state->hasDeferredResize)
            {
                QueuePendingResize(*state);
            }
            return 0;

        case WM_SIZE:
        {
            UINT const width = LOWORD(lParam);
            UINT const height = HIWORD(lParam);
            bool const isMinimized = wParam == SIZE_MINIMIZED;

            if (isMinimized)
            {
                state->renderer->OnWindowSize(width, height, true);
                return 0;
            }

            if (width > 0 && height > 0)
            {
                state->pendingWidth = width;
                state->pendingHeight = height;

                if (!state->isInSizeMove)
                {
                    QueuePendingResize(*state);
                }
                else
                {
                    state->hasDeferredResize = true;
                }
            }

            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            break;
    }

    if (HandleEditorMouseMessage(windowHandle, message, lParam, ImGui::GetIO().WantCaptureMouse, *state))
    {
        return 0;
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
