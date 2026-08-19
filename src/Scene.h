#pragma once

#include <DirectXMath.h>
#include <DirectXPackedVector.h>

#include <cstdint>
#include <vector>

struct Scene final
{
    Scene(std::uint32_t width, std::uint32_t height);

    void SetObstacle(int x, int y, bool solid);
    void SetEmissiveObstacle(int x, int y, DirectX::XMFLOAT3 radiance);
    bool IsInside(int x, int y) const;

    struct EmissionPixel
    {
        std::uint16_t r;
        std::uint16_t g;
        std::uint16_t b;
        std::uint16_t a;
    };
    static_assert(sizeof(EmissionPixel) == 8);

    std::uint32_t width;
    std::uint32_t height;

    std::vector<std::uint8_t> obstaclePixels;
    std::vector<EmissionPixel> emissionPixels;
};

inline Scene::Scene(std::uint32_t const width, std::uint32_t const height)
    : width(width)
    , height(height)
    , obstaclePixels(width * height, 0)
    , emissionPixels(width * height, EmissionPixel{})
{
}

inline void Scene::SetObstacle(int const x, int const y, bool const solid)
{
    if (!IsInside(x, y)) return;

    obstaclePixels[y * width + x] = solid ? 255u : 0u;
}

inline void Scene::SetEmissiveObstacle(int const x, int const y, DirectX::XMFLOAT3 const radiance)
{
    if (!IsInside(x, y)) return;

    obstaclePixels[y * width + x] = 255u;
    emissionPixels[y * width + x] = {
        .r = DirectX::PackedVector::XMConvertFloatToHalf(radiance.x),
        .g = DirectX::PackedVector::XMConvertFloatToHalf(radiance.y),
        .b = DirectX::PackedVector::XMConvertFloatToHalf(radiance.z),
        .a = DirectX::PackedVector::XMConvertFloatToHalf(1.0f),
    };
}

inline bool Scene::IsInside(int const x, int const y) const
{
    return x >= 0 && static_cast<std::uint32_t>(x) < width && y >= 0 && static_cast<std::uint32_t>(y) < height;
}
