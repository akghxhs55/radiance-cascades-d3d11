#pragma once

#include <DirectXMath.h>
#include <DirectXPackedVector.h>

#include <cstdint>
#include <vector>

struct Scene final
{
    Scene(UINT width, UINT height);

    void Resize(UINT newWidth, UINT newHeight);
    void SetEmissiveObstacle(int x, int y, DirectX::XMFLOAT3 radiance);
    void SetObstacle(int x, int y);
    void Erase(int x, int y);
    bool IsEmissive(int x, int y) const;
    bool IsObstacle(int x, int y) const;
    bool IsInside(int x, int y) const;

    struct EmissionPixel
    {
        std::uint16_t r;
        std::uint16_t g;
        std::uint16_t b;
        std::uint16_t a;
    };
    static_assert(sizeof(EmissionPixel) == 8);

    UINT width;
    UINT height;

    std::vector<EmissionPixel> emissionPixels;
    std::vector<std::uint8_t> obstaclePixels;
};

inline Scene::Scene(UINT const width, UINT const height)
    : width(width)
    , height(height)
    , emissionPixels(width * height, EmissionPixel{})
    , obstaclePixels(width * height, 0)
{
}

inline void Scene::Resize(UINT const newWidth, UINT const newHeight)
{
    std::vector<EmissionPixel> newEmissionPixels(newWidth * newHeight, EmissionPixel{});
    std::vector<std::uint8_t> newObstaclePixels(newWidth * newHeight, 0);

    UINT const copyWidth = std::min(width, newWidth);
    UINT const copyHeight = std::min(height, newHeight);

    for (UINT y = 0; y < copyHeight; ++y)
    {
        for (UINT x = 0; x < copyWidth; ++x)
        {
            newEmissionPixels[y * newWidth + x] = emissionPixels[y * width + x];
            newObstaclePixels[y * newWidth + x] = obstaclePixels[y * width + x];
        }
    }

    emissionPixels = std::move(newEmissionPixels);
    obstaclePixels = std::move(newObstaclePixels);
    width = newWidth;
    height = newHeight;
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

inline void Scene::SetObstacle(int const x, int const y)
{
    if (!IsInside(x, y)) return;

    obstaclePixels[y * width + x] = 255u;
    emissionPixels[y * width + x] = {};
}

inline void Scene::Erase(int const x, int const y)
{
    if (!IsInside(x, y)) return;

    obstaclePixels[y * width + x] = 0u;
    emissionPixels[y * width + x] = {};
}

inline bool Scene::IsEmissive(int const x, int const y) const
{
    return IsInside(x, y) && emissionPixels[y * width + x].a != 0u;
}

inline bool Scene::IsObstacle(int const x, int const y) const
{
    return IsInside(x, y) && obstaclePixels[y * width + x] != 0u;
}

inline bool Scene::IsInside(int const x, int const y) const
{
    return x >= 0 && static_cast<std::uint32_t>(x) < width && y >= 0 && static_cast<std::uint32_t>(y) < height;
}
