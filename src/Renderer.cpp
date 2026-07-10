#include "Renderer.h"

#include <array>
#include <d3dcompiler.h>

#include "D3DUtils.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "Vertex.h"

Renderer::Renderer(HWND const windowHandle)
    : windowHandle(windowHandle)
{
    CreateDeviceAndSwapChain();
    CreateRenderTargetView();
    CreateRasterizerState();
    CreateShaders();
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

    vertexBuffer.Reset();
    inputLayout.Reset();
    pixelShader.Reset();
    vertexShader.Reset();
    rasterizerState.Reset();
    renderTargetView.Reset();
    swapChain.Reset();
    deviceContext.Reset();
    device.Reset();
}

void Renderer::Render()
{
    PrepareFrame();

    deviceContext->Draw(vertexCount, 0);

    DrawDebugUi();

    ThrowIfFailed(
        swapChain->Present(vSyncEnabled ? 1 : 0, 0),
        "IDXGISwapChain::Present failed"
    );
}

void Renderer::SetVertices(std::span<Vertex const> const vertices)
{
    vertexCount = static_cast<UINT>(vertices.size());

    D3D11_BUFFER_DESC const bufferDesc = {
        .ByteWidth = static_cast<UINT>(sizeof(Vertex) * vertices.size()),
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
    };

    D3D11_SUBRESOURCE_DATA const subResourceData = {
        .pSysMem = vertices.data(),
    };

    ThrowIfFailed(
        device->CreateBuffer(
            &bufferDesc,
            &subResourceData,
            vertexBuffer.ReleaseAndGetAddressOf()
        ),
        "ID3D11Device::CreateBuffer failed"
    );
}

void Renderer::CreateDeviceAndSwapChain()
{
    std::array constexpr featureLevels = { D3D_FEATURE_LEVEL_11_0 };

    DXGI_SWAP_CHAIN_DESC const swapChainDesc = {
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

    D3D11_RENDER_TARGET_VIEW_DESC constexpr renderTargetViewDesc = {
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
    D3D11_RASTERIZER_DESC constexpr rasterizerDesc = {
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
    Microsoft::WRL::ComPtr<ID3DBlob> vertexShaderBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderBlob;

    ThrowIfFailed(
        D3DCompileFromFile(
            L"shaders/Shader.hlsl",
            nullptr,
            nullptr,
            "VSMain",
            "vs_5_0",
            0,
            0,
            vertexShaderBlob.ReleaseAndGetAddressOf(),
            nullptr
        ),
        "D3DCompileFromFile failed"
    );

    ThrowIfFailed(
        device->CreateVertexShader(
            vertexShaderBlob->GetBufferPointer(),
            vertexShaderBlob->GetBufferSize(),
            nullptr,
            vertexShader.ReleaseAndGetAddressOf()
        ),
        "ID3D11Device::CreateVertexShader failed"
    );

    ThrowIfFailed(
        D3DCompileFromFile(
            L"shaders/Shader.hlsl",
            nullptr,
            nullptr,
            "PSMain",
            "ps_5_0",
            0,
            0,
            pixelShaderBlob.ReleaseAndGetAddressOf(),
            nullptr
        ),
        "D3DCompileFromFile failed"
    );

    ThrowIfFailed(
        device->CreatePixelShader(
            pixelShaderBlob->GetBufferPointer(),
            pixelShaderBlob->GetBufferSize(),
            nullptr,
            pixelShader.ReleaseAndGetAddressOf()
        ),
        "ID3D11Device::CreatePixelShader failed"
    );

    std::array constexpr layout = {
        D3D11_INPUT_ELEMENT_DESC {
            .SemanticName = "POSITION",
            .SemanticIndex = 0,
            .Format = DXGI_FORMAT_R32G32B32_FLOAT,
            .InputSlot = 0,
            .AlignedByteOffset = offsetof(Vertex, position),
            .InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
            .InstanceDataStepRate = 0,
        },
        D3D11_INPUT_ELEMENT_DESC  {
            .SemanticName = "COLOR",
            .SemanticIndex = 0,
            .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .InputSlot = 0,
            .AlignedByteOffset = offsetof(Vertex, color),
            .InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
        }
    };

    ThrowIfFailed(
        device->CreateInputLayout(
            layout.data(),
            layout.size(),
            vertexShaderBlob->GetBufferPointer(),
            vertexShaderBlob->GetBufferSize(),
            inputLayout.ReleaseAndGetAddressOf()
        ),
        "ID3D11Device::CreateInputLayout failed"
    );
}

void Renderer::InitializeImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplWin32_Init(windowHandle);
    ImGui_ImplDX11_Init(device.Get(), deviceContext.Get());
}

void Renderer::PrepareFrame()
{
    deviceContext->ClearRenderTargetView(renderTargetView.Get(), clearColor);

    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    deviceContext->RSSetViewports(1,&viewport);
    deviceContext->RSSetState(rasterizerState.Get());

    deviceContext->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), nullptr);
    deviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

    deviceContext->VSSetShader(vertexShader.Get(), nullptr, 0);
    deviceContext->PSSetShader(pixelShader.Get(), nullptr, 0);
    deviceContext->IASetInputLayout(inputLayout.Get());

    UINT constexpr stride = sizeof(Vertex);
    UINT constexpr offset = 0;

    deviceContext->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
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
    ImGui::ColorEdit4("Clear Color", clearColor);
    ImGui::Checkbox("VSync", &vSyncEnabled);
    ImGui::End();

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}
