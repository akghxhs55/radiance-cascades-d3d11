#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <span>

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
    void SetVertices(std::span<Vertex const> vertices);

private:
    void CreateDeviceAndSwapChain();
    void CreateRenderTargetView();
    void CreateRasterizerState();
    void CreateShaders();
    void CreateSceneConstantBuffer();
    void InitializeImGui();

    void UpdateSceneConstantBuffer(Scene const& scene);
    void PrepareFrame();
    void DrawDebugUi();

private:
    HWND const windowHandle;

    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> deviceContext;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
    D3D11_VIEWPORT viewport = {};
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;

    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    UINT vertexCount = 0;
    Microsoft::WRL::ComPtr<ID3D11Buffer> sceneConstantBuffer;

    bool vSyncEnabled = true;
};
