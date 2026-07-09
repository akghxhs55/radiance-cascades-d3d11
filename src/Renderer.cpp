#include "Renderer.h"

#include <array>
#include <d3dcompiler.h>

#include "D3DUtils.h"
#include "Vertex.h"

Renderer::Renderer(HWND const windowHandle)
{
    CreateDeviceAndSwapChain(windowHandle);
    CreateRenderTargetView();
    CreateRasterizerState();
    CreateShaders();
}

Renderer::~Renderer() noexcept
{
    if (deviceContext)
    {
        deviceContext->ClearState();
        deviceContext->Flush();
    }

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
    std::array constexpr clearColor = { 0.0f, 0.0f, 0.2f, 1.0f };

    deviceContext->ClearRenderTargetView(renderTargetView.Get(),clearColor.data());

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

    deviceContext->Draw(vertexCount, 0);

    ThrowIfFailed(
        swapChain->Present(1, 0),
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

void Renderer::CreateDeviceAndSwapChain(HWND const windowHandle)
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
