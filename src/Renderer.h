#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include <array>
#include <cstdint>
#include <functional>

struct Scene;
struct SceneChange;

class Renderer final
{
public:
    using DrawUiCallback = std::function<void()>;

    explicit Renderer(HWND windowHandle);
    ~Renderer() noexcept;

    Renderer(Renderer const&) = delete;
    Renderer& operator=(Renderer const&) = delete;

    void SetScene(Scene const& scene);
    void UpdateScene(Scene const& scene, SceneChange const& change);
    void Render(DrawUiCallback const& drawUi = {});
    void OnWindowSize(UINT width, UINT height, bool minimized) noexcept;

private:
    void CreateDeviceAndSwapChain();
    void CreateRenderTargetView();
    void CreateRasterizerState();
    void CreateShaders();
    void CreateCascadeConstantBuffer();
    void CreateFinalGatherConstantBuffer();
    void CreateCascadeResources();
    void CreateDistanceFieldConstantsBuffer();
    void InitializeImGui();

    void CreateSceneTextures(UINT width, UINT height);
    void UploadSceneTextures(Scene const& scene);
    void GenerateDistanceField();

    void ApplyPendingResize();
    void RenderRadianceCascades();
    void RenderCascade(std::uint32_t cascadeIndex, bool mergeUpperCascade = true);
    void RenderFinalImage();
    void DrawImGui(DrawUiCallback const& drawUi);
    void DrawRendererDebugUi();

    [[nodiscard]]
    std::uint32_t CalculateRequiredCascadeCount() const;

private:
    HWND const windowHandle;

    UINT pendingWidth = 0;
    UINT pendingHeight = 0;
    bool resizePending = false;
    bool isMinimized = false;

    bool vSyncEnabled = true;
    bool hasScene = false;

    std::uint32_t cascadeCount = 5;
    std::uint32_t baseProbeSpacing = 1;
    std::uint32_t baseRayExponent = 3;
    float baseIntervalLength = 8.0f;
    int displayMode = 0;
    int debugCascadeIndex = 0;

    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> deviceContext;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
    Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter;
    D3D11_VIEWPORT viewport{};
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> fullscreenVertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> cascadePixelShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> finalGatherPixelShader;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> distanceFieldInitShader;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> distanceFieldJumpFloodShader;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> distanceFieldFinalizeShader;

    Microsoft::WRL::ComPtr<ID3D11Buffer> cascadeConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> finalGatherConstantBuffer;

    struct CascadeDimensions
    {
        UINT width;
        UINT height;
    };

    [[nodiscard]]
    CascadeDimensions CalculateCascadeDimensions(std::uint32_t cascadeIndex) const;

    struct CascadeResource
    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;

        CascadeDimensions dimensions;
    };
    [[nodiscard]]
    CascadeResource CreateCascadeResource(UINT width, UINT height);

    std::array<CascadeResource, 2> cascadeResources{};

    std::uint32_t sceneWidth = 0;
    std::uint32_t sceneHeight = 0;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> obstacleTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> obstacleSrv;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> emissionTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> emissionSrv;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> distanceFieldTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> distanceFieldSrv;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> distanceFieldUav;
    Microsoft::WRL::ComPtr<ID3D11Buffer> distanceFieldConstantBuffer;

    struct DistanceFieldResource
    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    };
    [[nodiscard]]
    DistanceFieldResource CreateDistanceFieldResource(UINT width, UINT height);

    std::array<DistanceFieldResource, 2> distanceFieldResources{};

    struct DistanceFieldConstants
    {
        std::uint32_t width;
        std::uint32_t height;
        std::uint32_t jumpSize;
        std::uint32_t padding;
    };
    static_assert(sizeof(DistanceFieldConstants) % 16 == 0);

    struct CascadePassConstants
    {
        std::uint32_t viewportWidth;
        std::uint32_t viewportHeight;
        std::uint32_t sceneWidth;
        std::uint32_t sceneHeight;

        std::uint32_t cascadeIndex;
        std::uint32_t cascadeCount;

        std::uint32_t baseProbeSpacing;
        std::uint32_t baseRayExponent;
        float baseIntervalLength;

        std::uint32_t mergeUpperCascade;

        std::array<std::uint32_t, 2> _padding;
    };
    static_assert(sizeof(CascadePassConstants) % 16 == 0);

    [[nodiscard]]
    CascadePassConstants BuildCascadeConstants(std::uint32_t cascadeIndex, bool mergeUpperCascade = true) const;
};
