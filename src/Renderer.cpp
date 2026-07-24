#include "Renderer.h"

#include <array>
#include <cmath>
#include <d3dcompiler.h>
#include <string>

#include "D3DUtils.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "Vertex.h"

namespace
{
    constexpr UINT MaxCircles = 16;
    constexpr UINT MaxBoxes = 16;

    struct GpuCircleData
    {
        DirectX::XMFLOAT2 center;
        float radius;
        float padding;
    };
    static_assert(sizeof(GpuCircleData) == 16);

    struct GpuCircleEmission
    {
        DirectX::XMFLOAT3 emission;
        float padding;
    };
    static_assert(sizeof(GpuCircleEmission) == 16);

    struct GpuBox
    {
        DirectX::XMFLOAT2 center;
        DirectX::XMFLOAT2 halfExtent;
    };
    static_assert(sizeof(GpuBox) == 16);

    struct SceneConstants
    {
        std::array<GpuCircleData, MaxCircles> circleData = {};
        std::array<GpuCircleEmission, MaxCircles> circleEmission = {};
        std::array<GpuBox, MaxBoxes> boxes = {};
        uint32_t circleCount = 0;
        uint32_t boxCount = 0;
        std::array<uint32_t, 2> padding = {};
    };
    static_assert(sizeof(SceneConstants) % 16 == 0);

    UINT CalculateProbeCount(float const sceneSize, UINT const probeSpacing)
    {
        return static_cast<UINT>(std::floor(sceneSize / static_cast<float>(probeSpacing) + 0.5f)) + 2;
    }

    [[nodiscard]]
    Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
        wchar_t const* const filePath,
        char const* const entryPoint,
        char const* const target)
    {
        Microsoft::WRL::ComPtr<ID3DBlob> shaderBlob;
        Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

        HRESULT const result = D3DCompileFromFile(
            filePath,
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entryPoint,
            target,
            0,
            0,
            shaderBlob.ReleaseAndGetAddressOf(),
            errorBlob.ReleaseAndGetAddressOf()
        );

        OutputBlob(errorBlob.Get());

        std::string operation = "D3DCompileFromFile failed for entry point ";
        operation += entryPoint;
        ThrowIfFailed(result, operation);

        return shaderBlob;
    }
}

Renderer::Renderer(HWND const windowHandle)
    : windowHandle(windowHandle)
{
    CreateDeviceAndSwapChain();
    CreateRenderTargetView();
    CreateRasterizerState();
    CreateShaders();

    CreateSceneConstantBuffer();
    CreateCascadeConstantBuffer();
    CreateCascadeResources();

    InitializeImGui();
}

Renderer::~Renderer() noexcept
{
    if (deviceContext)
    {
        deviceContext->ClearState();
        deviceContext->Flush();
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    cascadeResources.clear();
    cascadeConstantBuffer.Reset();
    sceneConstantBuffer.Reset();
    cascadePixelShader.Reset();
    fullscreenVertexShader.Reset();
    rasterizerState.Reset();
    renderTargetView.Reset();
    swapChain.Reset();
    deviceContext.Reset();
    device.Reset();
}

void Renderer::Render(Scene const& scene)
{
    UpdateSceneConstantBuffer(scene);

    RenderRadianceCascades();
    RenderFinalImage();

    DrawDebugUi();

    ThrowIfFailed(
        swapChain->Present(vSyncEnabled ? 1 : 0, 0),
        "IDXGISwapChain::Present failed"
    );
}

void Renderer::CreateDeviceAndSwapChain()
{
    std::array constexpr featureLevels = { D3D_FEATURE_LEVEL_11_0 };

    DXGI_SWAP_CHAIN_DESC const swapChainDesc{
        .BufferDesc = {
            .Width = 0,
            .Height = 0,
            .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
        },
        .SampleDesc = {
            .Count = 1,
        },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = 2,
        .OutputWindow = windowHandle,
        .Windowed = true,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
    };

    UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifndef NDEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    ThrowIfFailed(
        D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            createDeviceFlags,
            featureLevels.data(),
            static_cast<UINT>(featureLevels.size()),
            D3D11_SDK_VERSION,
            &swapChainDesc,
            swapChain.ReleaseAndGetAddressOf(),
            device.ReleaseAndGetAddressOf(),
            nullptr,
            deviceContext.ReleaseAndGetAddressOf()
        ),
        "D3D11CreateDeviceAndSwapChain failed"
    );

    DXGI_SWAP_CHAIN_DESC createdAtSwapChainDesc;
    ThrowIfFailed(
        swapChain->GetDesc(&createdAtSwapChainDesc),
        "IDXGISwapChain::GetDesc failed"
    );

    viewport = {
        .TopLeftX = 0.0f,
        .TopLeftY = 0.0f,
        .Width = static_cast<float>(createdAtSwapChainDesc.BufferDesc.Width),
        .Height = static_cast<float>(createdAtSwapChainDesc.BufferDesc.Height),
        .MinDepth = 0.0f,
        .MaxDepth = 1.0f,
    };
}

void Renderer::CreateRenderTargetView()
{
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    ThrowIfFailed(
        swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.ReleaseAndGetAddressOf())),
        "IDXGISwapChain::GetBuffer failed"
    );

    D3D11_RENDER_TARGET_VIEW_DESC constexpr renderTargetViewDesc{
        .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
        .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
    };

    ThrowIfFailed(
        device->CreateRenderTargetView(
            backBuffer.Get(),
            &renderTargetViewDesc,
            renderTargetView.ReleaseAndGetAddressOf()
        ),
        "ID3D11Device::CreateRenderTargetView failed"
    );
}

void Renderer::CreateRasterizerState()
{
    D3D11_RASTERIZER_DESC constexpr rasterizerDesc{
        .FillMode = D3D11_FILL_SOLID,
        .CullMode = D3D11_CULL_BACK,
    };

    ThrowIfFailed(
        device->CreateRasterizerState(
            &rasterizerDesc,
            rasterizerState.ReleaseAndGetAddressOf()
        ),
        "ID3D11Device::CreateRasterizerState failed"
    );
}

void Renderer::CreateShaders()
{
    Microsoft::WRL::ComPtr<ID3DBlob> shaderBlob = CompileShader(L"shaders/FullScreen.hlsl", "VSMain", "vs_5_0");
    ThrowIfFailed(
        device->CreateVertexShader(
            shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr,
            fullscreenVertexShader.ReleaseAndGetAddressOf()
        ),
        "ID3D11Device::CreateVertexShader failed"
    );

    shaderBlob = CompileShader(L"shaders/Cascade.hlsl", "PSCascade", "ps_5_0");
    ThrowIfFailed(
        device->CreatePixelShader(
            shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr,
            cascadePixelShader.ReleaseAndGetAddressOf()
        ),
        "ID3D11Device::CreatePixelShader failed"
    );

    shaderBlob = CompileShader(L"shaders/FinalGather.hlsl", "PSFinalGather", "ps_5_0");
    ThrowIfFailed(
        device->CreatePixelShader(
            shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr,
            finalGatherPixelShader.ReleaseAndGetAddressOf()
        ),
        "ID3D11Device::CreatePixelShader failed"
    );
}

void Renderer::CreateSceneConstantBuffer()
{
    D3D11_BUFFER_DESC constexpr bufferDesc{
        .ByteWidth = static_cast<UINT>(sizeof(SceneConstants)),
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
    };

    ThrowIfFailed(
        device->CreateBuffer(&bufferDesc, nullptr, sceneConstantBuffer.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateBuffer for scene constants failed"
    );
}

void Renderer::CreateCascadeConstantBuffer()
{
    D3D11_BUFFER_DESC constexpr bufferDesc{
        .ByteWidth = static_cast<UINT>(sizeof(CascadeConstants)),
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
    };

    ThrowIfFailed(
        device->CreateBuffer(&bufferDesc, nullptr, cascadeConstantBuffer.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateBuffer for cascade constants failed"
    );
}

void Renderer::CreateCascadeResources()
{
    cascadeResources.clear();
    cascadeResources.reserve(cascadeCount);

    for (UINT i = 0; i < cascadeCount; ++i)
    {
        UINT const scale = 1u << i;

        UINT const probeCountX = CalculateProbeCount(viewport.Width, baseProbeSpacing * scale);
        UINT const probeCountY = CalculateProbeCount(viewport.Height, baseProbeSpacing * scale);

        UINT const totalTexels = probeCountX * probeCountY * baseRaysPerProbe * scale * scale;

        UINT const width = std::min<UINT>(totalTexels, 4096u);
        UINT const height = (totalTexels + width - 1) / width;

        cascadeResources.push_back(CreateCascadeResource(width, height));
    }
}

void Renderer::InitializeImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplWin32_Init(windowHandle);
    ImGui_ImplDX11_Init(device.Get(), deviceContext.Get());
}

void Renderer::UpdateSceneConstantBuffer(Scene const& scene)
{
    SceneConstants constants = {};

    for (Circle const& circle : scene.circles)
    {
        if (constants.circleCount == MaxCircles)
        {
            break;
        }

        UINT const index = constants.circleCount++;
        constants.circleData[index] = {
            .center = circle.center,
            .radius = circle.radius,
        };
        constants.circleEmission[index] = {
            .emission = circle.emission,
        };
    }

    for (Box const& box : scene.boxes)
    {
        if (constants.boxCount == MaxBoxes)
        {
            break;
        }

        constants.boxes[constants.boxCount++] = {
            .center = box.center,
            .halfExtent = box.halfExtent,
        };
    }

    deviceContext->UpdateSubresource(sceneConstantBuffer.Get(), 0, nullptr, &constants, 0, 0);
}

void Renderer::RenderRadianceCascades()
{
    deviceContext->IASetInputLayout(nullptr);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    deviceContext->VSSetShader(fullscreenVertexShader.Get(), nullptr, 0);
    deviceContext->PSSetShader(cascadePixelShader.Get(), nullptr, 0);

    deviceContext->RSSetState(rasterizerState.Get());
    deviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

    ID3D11Buffer* const constantBuffers[] = {
        sceneConstantBuffer.Get(),
        cascadeConstantBuffer.Get(),
    };
    deviceContext->PSSetConstantBuffers(0, 2, constantBuffers);

    for (int cascadeIndex = static_cast<int>(cascadeCount) - 1; cascadeIndex >= 0; --cascadeIndex)
    {
        RenderCascade(cascadeIndex);
    }

    ID3D11ShaderResourceView* const nullSRV = nullptr;
    deviceContext->PSSetShaderResources(0, 1, &nullSRV);

    ID3D11RenderTargetView* const nullRTV = nullptr;
    deviceContext->OMSetRenderTargets(1, &nullRTV, nullptr);
}

void Renderer::RenderCascade(UINT const cascadeIndex)
{
    CascadeResource& current = cascadeResources[cascadeIndex];

    ID3D11ShaderResourceView* const nullSRV = nullptr;
    deviceContext->PSSetShaderResources(0, 1, &nullSRV);

    D3D11_VIEWPORT const cascadeViewport{
        .TopLeftX = 0.0f,
        .TopLeftY = 0.0f,
        .Width = static_cast<float>(current.width),
        .Height = static_cast<float>(current.height),
        .MinDepth = 0.0f,
        .MaxDepth = 1.0f,
    };

    deviceContext->RSSetViewports(1, &cascadeViewport);
    deviceContext->OMSetRenderTargets(1, current.renderTargetView.GetAddressOf(), nullptr);

    const CascadeConstants constants = BuildCascadeConstants(cascadeIndex);
    deviceContext->UpdateSubresource(cascadeConstantBuffer.Get(), 0, nullptr, &constants, 0, 0);

    if (cascadeIndex + 1 < cascadeCount)
    {
        CascadeResource& upperCascade = cascadeResources[cascadeIndex + 1];
        deviceContext->PSSetShaderResources(0, 1, upperCascade.shaderResourceView.GetAddressOf());
    }
    else
    {
        deviceContext->PSSetShaderResources(0, 1, &nullSRV);
    }

    deviceContext->Draw(3, 0);

    ID3D11RenderTargetView* const nullRTV = nullptr;
    deviceContext->OMSetRenderTargets(1, &nullRTV, nullptr);
    deviceContext->PSSetShaderResources(0, 1, &nullSRV);
}

void Renderer::RenderFinalImage()
{
    deviceContext->IASetInputLayout(nullptr);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    deviceContext->VSSetShader(fullscreenVertexShader.Get(), nullptr, 0);
    deviceContext->PSSetShader(finalGatherPixelShader.Get(), nullptr, 0);

    CascadeConstants const constants = BuildCascadeConstants(0);
    deviceContext->UpdateSubresource(cascadeConstantBuffer.Get(), 0, nullptr, &constants, 0, 0);

    ID3D11Buffer* const constantBuffers[] = { cascadeConstantBuffer.Get() };
    deviceContext->PSSetConstantBuffers(1, 1, constantBuffers);
    deviceContext->PSSetShaderResources(0, 1, cascadeResources[0].shaderResourceView.GetAddressOf());

    deviceContext->RSSetViewports(1, &viewport);
    deviceContext->RSSetState(rasterizerState.Get());

    deviceContext->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), nullptr);
    deviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

    deviceContext->Draw(3, 0);

    ID3D11ShaderResourceView* const nullSRV = nullptr;
    deviceContext->PSSetShaderResources(0, 1, &nullSRV);
}

void Renderer::DrawDebugUi()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGuiIO const& io = ImGui::GetIO();

    ImGui::Begin("Renderer");
    ImGui::Text("FPS: %.1f", io.Framerate);
    ImGui::Text("Frame Time: %.3f ms", io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f);
    ImGui::Checkbox("VSync", &vSyncEnabled);
    ImGui::End();

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

Renderer::CascadeResource Renderer::CreateCascadeResource(UINT const width, UINT const height)
{
    CascadeResource resource{
        .width = width,
        .height = height,
    };

    D3D11_TEXTURE2D_DESC const desc{
        .Width = width,
        .Height = height,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_R16G16B16A16_FLOAT,
        .SampleDesc = {
            .Count = 1,
        },
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
    };

    ThrowIfFailed(
        device->CreateTexture2D(&desc, nullptr, resource.texture.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateTexture2D failed"
    );

    ThrowIfFailed(
        device->CreateRenderTargetView(resource.texture.Get(), nullptr, resource.renderTargetView.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateRenderTargetView failed");

    ThrowIfFailed(
        device->CreateShaderResourceView(resource.texture.Get(), nullptr, resource.shaderResourceView.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateShaderResourceView failed"
    );

    return resource;
}

Renderer::CascadeConstants Renderer::BuildCascadeConstants(UINT const cascadeIndex) const
{
    uint32_t const scale = 1u << cascadeIndex;
    uint32_t const upperScale = scale * 2;

    return {
        .cascadeIndex = cascadeIndex,
        .cascadeCount = cascadeCount,
        .probeCountX = static_cast<uint32_t>(std::floor(viewport.Width / static_cast<float>(baseProbeSpacing * scale) + 0.5f) + 2),
        .probeCountY = static_cast<uint32_t>(std::floor(viewport.Height / static_cast<float>(baseProbeSpacing * scale) + 0.5f) + 2),
        .probeSpacing = static_cast<float>(baseProbeSpacing * scale),
        .probeOffset = static_cast<float>(-0.5 * baseProbeSpacing * scale),
        .raysPerProbe = baseRaysPerProbe * scale * scale,
        .intervalStart = baseIntervalLength * static_cast<float>(scale * scale - 1.0) / 3.0f,
        .intervalEnd = baseIntervalLength * static_cast<float>(scale * scale * 4 - 1.0) / 3.0f,
        .radianceTextureSize = DirectX::XMFLOAT2{
            static_cast<float>(cascadeResources[cascadeIndex].width),
            static_cast<float>(cascadeResources[cascadeIndex].height),
        },
        .upperProbeCountX = cascadeIndex + 1 < cascadeCount
            ? static_cast<uint32_t>(std::floor(viewport.Width / static_cast<float>(baseProbeSpacing * upperScale) + 0.5f) + 2)
            : 0,
        .upperProbeCountY = cascadeIndex + 1 < cascadeCount
            ? static_cast<uint32_t>(std::floor(viewport.Height / static_cast<float>(baseProbeSpacing * upperScale) + 0.5f) + 2)
            : 0,
    };
}
