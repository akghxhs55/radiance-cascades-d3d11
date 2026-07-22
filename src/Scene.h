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

// Screen-space scene data for a 1024 x 1024 render area.
// The origin is at the top-left, with +X pointing right and +Y pointing down.
inline constexpr std::array SampleCircles = {
    Circle {
        .center = { 179.2f, 409.6f },
        .radius = 51.2f,
        .emission = { 12.0f, 5.0f, 1.0f },
    },
    Circle {
        .center = { 819.2f, 215.04f },
        .radius = 30.72f,
        .emission = { 0.5f, 3.0f, 12.0f },
    },
};

inline constexpr std::array SampleBoxes = {
    Box {
        .center = { 471.04f, 471.04f },
        .halfExtent = { 30.72f, 296.96f },
    },
    Box {
        .center = { 266.24f, 757.76f },
        .halfExtent = { 143.36f, 25.6f },
    },
    Box {
        .center = { 727.04f, 450.56f },
        .halfExtent = { 25.6f, 128.0f },
    },
};

inline constexpr Scene SampleScene = {
    .circles = SampleCircles,
    .boxes = SampleBoxes,
};
