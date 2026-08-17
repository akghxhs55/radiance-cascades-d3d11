#include "Renderer.h"

#include <d3dcompiler.h>

#include <array>
#include <algorithm>
#include <string>
#include <cmath>
#include <cstdint>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include "D3DUtils.h"
#include "Scene.h"
#include "Vertex.h"

namespace
{
    constexpr std::uint32_t MaxCircles = 16;
    constexpr std::uint32_t MaxBoxes = 16;

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
        std::uint32_t circleCount = 0u;
        std::uint32_t boxCount = 0u;
        std::array<std::uint32_t, 2> padding = {};
    };
    static_assert(sizeof(SceneConstants) % 16 == 0);

    struct FinalGatherConstants
    {
        std::uint32_t displayMode;
        std::array<std::uint32_t, 3> padding;
    };
    static_assert(sizeof(FinalGatherConstants) % 16 == 0);

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

    bool SliderUInt(char const* const label, std::uint32_t* const value, std::uint32_t const min, std::uint32_t const max)
    {
        return ImGui::SliderScalar(label, ImGuiDataType_U32, value, &min, &max);
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
    CreateFinalGatherConstantBuffer();
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

    finalGatherConstantBuffer.Reset();
    cascadeConstantBuffer.Reset();
    sceneConstantBuffer.Reset();
    cascadePixelShader.Reset();
    fullscreenVertexShader.Reset();
    rasterizerState.Reset();
    renderTargetView.Reset();
    swapChain.Reset();
    adapter.Reset();
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
    constexpr std::array featureLevels = { D3D_FEATURE_LEVEL_11_0 };

    DXGI_SWAP_CHAIN_DESC const swapChainDesc{
        .BufferDesc = {
            .Width = 0u,
            .Height = 0u,
            .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
        },
        .SampleDesc = {
            .Count = 1u,
        },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = 2u,
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
            featureLevels.size(),
            D3D11_SDK_VERSION,
            &swapChainDesc,
            swapChain.ReleaseAndGetAddressOf(),
            device.ReleaseAndGetAddressOf(),
            nullptr,
            deviceContext.ReleaseAndGetAddressOf()
        ),
        "D3D11CreateDeviceAndSwapChain failed"
    );

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    ThrowIfFailed(
        device.As(&dxgiDevice),
        "ID3D11Device::QueryInterface for IDXGIDevice failed"
    );

    Microsoft::WRL::ComPtr<IDXGIAdapter> dxgiAdapter;
    ThrowIfFailed(
        dxgiDevice->GetAdapter(dxgiAdapter.GetAddressOf()),
        "IDXGIDevice::GetAdapter failed"
    );

    dxgiAdapter.As(&adapter);

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

    constexpr D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc{
        .Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
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
    constexpr D3D11_RASTERIZER_DESC rasterizerDesc{
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
    constexpr D3D11_BUFFER_DESC bufferDesc{
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
    constexpr D3D11_BUFFER_DESC bufferDesc{
        .ByteWidth = static_cast<UINT>(sizeof(CascadePassConstants)),
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
    };

    ThrowIfFailed(
        device->CreateBuffer(&bufferDesc, nullptr, cascadeConstantBuffer.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateBuffer for cascade constants failed"
    );
}

void Renderer::CreateFinalGatherConstantBuffer()
{
    constexpr D3D11_BUFFER_DESC bufferDesc{
        .ByteWidth = static_cast<UINT>(sizeof(FinalGatherConstants)),
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
    };

    ThrowIfFailed(
        device->CreateBuffer(&bufferDesc, nullptr, finalGatherConstantBuffer.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateBuffer for final gather constants failed"
    );
}

void Renderer::CreateCascadeResources()
{
    UINT maxWidth = 0u;
    UINT maxHeight = 0u;
    for (auto cascadeIndex = 0u; cascadeIndex < cascadeCount; ++cascadeIndex)
    {
        auto const [width, height] = CalculateCascadeDimensions(cascadeIndex);
        maxWidth = std::max(maxWidth, width);
        maxHeight = std::max(maxHeight, height);
    }

    cascadeResources[0] = CreateCascadeResource(maxWidth, maxHeight);
    cascadeResources[1] = CreateCascadeResource(maxWidth, maxHeight);
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

    for (auto const& circle : scene.circles)
    {
        if (constants.circleCount == MaxCircles)
        {
            break;
        }

        std::uint32_t const index = constants.circleCount++;
        constants.circleData[index] = {
            .center = circle.center,
            .radius = circle.radius,
        };
        constants.circleEmission[index] = {
            .emission = circle.emission,
        };
    }

    for (auto const& box : scene.boxes)
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

    if (displayMode == 1)
    {
        auto const localCascadeIndex = static_cast<std::uint32_t>(std::clamp(debugCascadeIndex, 0, static_cast<int>(cascadeCount) - 1));
        RenderCascade(localCascadeIndex, false);
    }

    ID3D11ShaderResourceView* const nullSRV = nullptr;
    deviceContext->PSSetShaderResources(0, 1, &nullSRV);

    ID3D11RenderTargetView* const nullRTV = nullptr;
    deviceContext->OMSetRenderTargets(1, &nullRTV, nullptr);
}

void Renderer::RenderCascade(std::uint32_t const cascadeIndex, bool const mergeUpperCascade)
{
    CascadeResource& current = cascadeResources[cascadeIndex % 2];

    ID3D11ShaderResourceView* const nullSRV = nullptr;
    deviceContext->PSSetShaderResources(0, 1, &nullSRV);

    auto const [width, height] = CalculateCascadeDimensions(cascadeIndex);

    D3D11_VIEWPORT const cascadeViewport{
        .TopLeftX = 0.0f,
        .TopLeftY = 0.0f,
        .Width = static_cast<float>(width),
        .Height = static_cast<float>(height),
        .MinDepth = 0.0f,
        .MaxDepth = 1.0f,
    };

    deviceContext->RSSetViewports(1, &cascadeViewport);
    deviceContext->OMSetRenderTargets(1, current.renderTargetView.GetAddressOf(), nullptr);

    CascadePassConstants const constants = BuildCascadeConstants(cascadeIndex, mergeUpperCascade);
    deviceContext->UpdateSubresource(cascadeConstantBuffer.Get(), 0, nullptr, &constants, 0, 0);

    if (mergeUpperCascade && cascadeIndex + 1 < cascadeCount)
    {
        CascadeResource& upperCascade = cascadeResources[(cascadeIndex + 1) % 2];
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

    auto const selectedCascadeIndex = displayMode == 1
        ? static_cast<std::uint32_t>(std::clamp(debugCascadeIndex, 0, static_cast<int>(cascadeCount) - 1))
        : 0u;

    CascadePassConstants const cascadeConstants = BuildCascadeConstants(selectedCascadeIndex);
    deviceContext->UpdateSubresource(cascadeConstantBuffer.Get(), 0, nullptr, &cascadeConstants, 0, 0);

    FinalGatherConstants const finalGatherConstants{
        .displayMode = static_cast<std::uint32_t>(displayMode),
    };
    deviceContext->UpdateSubresource(finalGatherConstantBuffer.Get(), 0, nullptr, &finalGatherConstants, 0, 0);

    ID3D11Buffer* const constantBuffers[] = {
        finalGatherConstantBuffer.Get(),
        cascadeConstantBuffer.Get(),
    };
    deviceContext->PSSetConstantBuffers(0, 2, constantBuffers);
    deviceContext->PSSetShaderResources(
        0,
        1,
        cascadeResources[selectedCascadeIndex % 2].shaderResourceView.GetAddressOf()
    );

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

    constexpr double bytesPerMiB = 1024.0 * 1024.0;
    std::uint64_t cascadeTextureBytes = 0u;
    for (CascadeResource const& cascadeResource : cascadeResources)
    {
        cascadeTextureBytes += static_cast<std::uint64_t>(cascadeResource.dimensions.width)
            * cascadeResource.dimensions.height * 8u;
    }
    ImGui::Text("Cascade textures (est.): %.2f MiB", static_cast<double>(cascadeTextureBytes) / bytesPerMiB);

    DXGI_QUERY_VIDEO_MEMORY_INFO localMemoryInfo{};
    if (adapter && SUCCEEDED(adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &localMemoryInfo)))
    {
        ImGui::Text("Local VRAM: %.2f / %.2f MiB",
            static_cast<double>(localMemoryInfo.CurrentUsage) / bytesPerMiB,
            static_cast<double>(localMemoryInfo.Budget) / bytesPerMiB
        );
    }

    ImGui::Checkbox("VSync", &vSyncEnabled);

    constexpr char const* displayModes[] = {
        "Final Image",
        "Selected Cascade",
        "Visibility",
    };
    ImGui::Combo("Display Mode", &displayMode, displayModes, IM_ARRAYSIZE(displayModes));

    if (displayMode == 1)
    {
        ImGui::SliderInt("Cascade", &debugCascadeIndex, 0, static_cast<int>(cascadeCount) - 1);
    }
    if (displayMode == 2)
    {
        ImGui::TextUnformatted("White: visible, Black: occluded");
    }

    bool cascadeLayoutChanged = false;
    cascadeLayoutChanged |= SliderUInt("Cascade Count", &cascadeCount, 1, 8);
    cascadeLayoutChanged |= SliderUInt("Base Probe Spacing", &baseProbeSpacing, 1, 32);
    cascadeLayoutChanged |= SliderUInt("Base Rays Exponent", &baseRayExponent, 2, 5);
    ImGui::SliderFloat("Base Interval Length", &baseIntervalLength, 1.0f, 32.0f);

    if (cascadeLayoutChanged)
    {
        CreateCascadeResources();
    }

    ImGui::End();

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

Renderer::CascadeDimensions Renderer::CalculateCascadeDimensions(std::uint32_t const cascadeIndex) const
{
    std::uint32_t const scale = 1u << cascadeIndex;
    std::uint32_t const probeSpacing = baseProbeSpacing * scale;
    std::uint32_t const probeCountX = (static_cast<std::uint32_t>(viewport.Width) + probeSpacing / 2) / probeSpacing + 2;
    std::uint32_t const probeCountY = (static_cast<std::uint32_t>(viewport.Height) + probeSpacing / 2) / probeSpacing + 2;

    std::uint32_t const widthExponent = baseRayExponent / 2 + cascadeIndex;
    std::uint32_t const heightExponent = (baseRayExponent + 1) / 2 + cascadeIndex;
    std::uint32_t const rayBlockWidth = (1u << widthExponent) / 2u;
    std::uint32_t const rayBlockHeight = (1u << heightExponent) / 2u;

    return {
        .width = rayBlockWidth * probeCountX,
        .height = rayBlockHeight * probeCountY,
    };
}

Renderer::CascadeResource Renderer::CreateCascadeResource(std::uint32_t const width, std::uint32_t const height)
{
    CascadeResource resource{
        .dimensions = {
            .width = width,
            .height = height,
        }
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

Renderer::CascadePassConstants Renderer::BuildCascadeConstants(std::uint32_t const cascadeIndex, bool const mergeUpperCascade) const
{
    return {
        .sceneWidth = static_cast<std::uint32_t>(viewport.Width),
        .sceneHeight = static_cast<std::uint32_t>(viewport.Height),

        .cascadeIndex = static_cast<std::uint32_t>(cascadeIndex),
        .cascadeCount = cascadeCount,

        .baseProbeSpacing = baseProbeSpacing,
        .baseRayExponent = baseRayExponent,
        .baseIntervalLength = baseIntervalLength,

        .mergeUpperCascade = static_cast<std::uint32_t>(mergeUpperCascade),
    };
}
