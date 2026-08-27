#pragma once

#include "SceneChange.h"

#include <d3dcommon.h>
#include <DirectXMath.h>

#include <random>

struct Scene;

class SceneEditor final
{
public:
    enum class EditTool
    {
        Emissive,
        Obstacle,
        Eraser,
    };

    explicit SceneEditor(Scene& scene);

    void BeginStroke(EditTool tool, POINT currentPosition);
    void ContinueStroke(POINT currentPosition);
    void EndStroke();

    [[nodiscard]]
    SceneChange TakePendingChange();
    [[nodiscard]]
    int GetBrushRadius() const;
    void SetBrushRadius(int radius);
    [[nodiscard]]
    bool IsStrokeActive() const;

private:
    void PaintBrush(POINT center);
    void PaintPixel(int x, int y);

private:
    Scene& scene;

    EditTool activeTool = EditTool::Emissive;
    DirectX::XMFLOAT3 activeColor{};
    int brushRadius = 8;
    bool strokeActive = false;

    POINT previousPosition{};

    SceneChange pendingChange{};

    std::mt19937 randomEngine{ std::random_device{}() };
    std::uniform_real_distribution<float> hueOffsetDistribution{ 0.2f, 0.8f };
    std::uniform_real_distribution<float> saturationDistribution{ 0.65f, 0.9f };
    std::uniform_real_distribution<float> valueDistribution{ 0.85f, 1.0f };
    float lastHue = 0.0f;
};
