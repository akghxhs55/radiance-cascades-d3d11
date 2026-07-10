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

inline constexpr std::array SampleCircles = {
    Circle {
        .center = { -0.65f, 0.20f },
        .radius = 0.10f,
        .emission = { 12.0f, 5.0f, 1.0f },
    },
    Circle {
        .center = { 0.60f, 0.58f },
        .radius = 0.06f,
        .emission = { 0.5f, 3.0f, 12.0f },
    },
};

inline constexpr std::array SampleBoxes = {
    Box {
        .center = { -0.08f, 0.08f },
        .halfExtent = { 0.06f, 0.58f },
    },
    Box {
        .center = { -0.48f, -0.48f },
        .halfExtent = { 0.28f, 0.05f },
    },
    Box {
        .center = { 0.42f, 0.12f },
        .halfExtent = { 0.05f, 0.25f },
    },
};

inline constexpr Scene SampleScene = {
    .circles = SampleCircles,
    .boxes = SampleBoxes,
};
