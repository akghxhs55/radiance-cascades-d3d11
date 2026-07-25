#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <span>
#include <vector>

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
    void CreateCascadeResources();
    void InitializeImGui();

    void UpdateSceneConstantBuffer(Scene const& scene);
    void RenderRadianceCascades();
    void RenderCascade(UINT cascadeIndex);
    void RenderFinalImage();
    void DrawDebugUi();

private:
    HWND const windowHandle;

    bool vSyncEnabled = true;
    UINT cascadeCount = 5;
    UINT baseProbeSpacing = 1;
    UINT baseRaysPerProbe = 8;
    float baseIntervalLength = 8.0f;

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

    struct CascadeConstants
    {
        uint32_t cascadeIndex;
        uint32_t cascadeCount;

        uint32_t probeCountX;
        uint32_t probeCountY;
        float probeSpacing;
        float probeOffset;

        uint32_t raysPerProbe;
        float intervalStart;
        float intervalEnd;

        DirectX::XMFLOAT2 radianceTextureSize;

        uint32_t upperProbeCountX;
        uint32_t upperProbeCountY;

        std::array<uint32_t, 3> padding;
    };
    static_assert(sizeof(CascadeConstants) % 16 == 0);

    [[nodiscard]]
    CascadeConstants BuildCascadeConstants(UINT cascadeIndex) const;

};
