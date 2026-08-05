#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <vector>
#include <cstdint>

#include "Scene.h"
#include "Vertex.h"

class Renderer
{
public:
    explicit Renderer(HWND windowHandle);
    ~Renderer() noexcept;

    Renderer(Renderer const&) = delete;
    Renderer& operator=(Renderer const&) = delete;

    void Render(Scene const& scene);

private:
    void CreateDeviceAndSwapChain();
    void CreateRenderTargetView();
    void CreateRasterizerState();
    void CreateShaders();
    void CreateSceneConstantBuffer();
    void CreateCascadeConstantBuffer();
    void CreateFinalGatherConstantBuffer();
    void CreateCascadeResources();
    void InitializeImGui();

    void UpdateSceneConstantBuffer(Scene const& scene);
    void RenderRadianceCascades();
    void RenderCascade(UINT cascadeIndex, bool mergeUpperCascade = true);
    void RenderFinalImage();
    void DrawDebugUi();

private:
    HWND const windowHandle;

    bool vSyncEnabled = true;
    std::uint32_t cascadeCount = 5;
    std::uint32_t baseProbeSpacing = 1;
    std::uint32_t baseRayExponent = 3;
    float baseIntervalLength = 8.0f;
    int displayMode = 0;
    int debugCascadeIndex = 0;

    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> deviceContext;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
    D3D11_VIEWPORT viewport{};
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> fullscreenVertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> cascadePixelShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> finalGatherPixelShader;

    Microsoft::WRL::ComPtr<ID3D11Buffer> sceneConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> cascadeConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> finalGatherConstantBuffer;

    struct CascadeResource
    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView;

        UINT width;
        UINT height;
    };

    [[nodiscard]]
    CascadeResource CreateCascadeResource(UINT width, UINT height);

    std::vector<CascadeResource> cascadeResources;

    struct CascadePassConstants
    {
        std::uint32_t sceneWidth;
        std::uint32_t sceneHeight;

        std::uint32_t cascadeIndex;
        std::uint32_t cascadeCount;

        std::uint32_t baseProbeSpacing;
        std::uint32_t baseRayExponent;
        float baseIntervalLength;

        std::uint32_t mergeUpperCascade;
    };
    static_assert(sizeof(CascadePassConstants) % 16 == 0);

    [[nodiscard]]
    CascadePassConstants BuildCascadeConstants(UINT cascadeIndex, bool mergeUpperCascade = true) const;

};
