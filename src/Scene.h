#pragma once

#include <array>
#include <DirectXMath.h>
#include <span>

struct Circle
{
    DirectX::XMFLOAT2 center;
    float radius;
    DirectX::XMFLOAT3 emission;
};

struct Box
{
    DirectX::XMFLOAT2 center;
    DirectX::XMFLOAT2 halfExtent;
};

struct Scene
{
    std::span<Circle const> circles;
    std::span<Box const> boxes;
};
