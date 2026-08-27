#include "SceneEditor.h"

#include "Scene.h"
#include "SceneChange.h"

#include <cmath>
#include <algorithm>

namespace
{
    DirectX::XMFLOAT3 HsvToRgb(float const h, float const s, float const v)
    {
        float r = 0.0f, g = 0.0f, b = 0.0f;

        int const i = static_cast<int>(h * 6);
        float const f = h * 6 - static_cast<float>(i);
        float const p = v * (1 - s);
        float const q = v * (1 - f * s);
        float const t = v * (1 - (1 - f) * s);

        switch (i % 6)
        {
            case 0: r = v; g = t; b = p; break;
            case 1: r = q; g = v; b = p; break;
            case 2: r = p; g = v; b = t; break;
            case 3: r = p; g = q; b = v; break;
            case 4: r = t; g = p; b = v; break;
            case 5: r = v; g = p; b = q; break;
            default: break;
        }

        return DirectX::XMFLOAT3{r, g, b};
    }
}

SceneEditor::SceneEditor(Scene &scene)
    : scene(scene)
{
}

void SceneEditor::BeginStroke(EditTool const tool, POINT const currentPosition)
{
    activeTool = tool;

    if (tool == EditTool::Emissive)
    {
        float const hueOffset = hueOffsetDistribution(randomEngine);
        float const hue = std::fmod(lastHue + hueOffset, 1.0f);
        float const saturation = saturationDistribution(randomEngine);
        float const value = valueDistribution(randomEngine);

        DirectX::XMFLOAT3 const rgb = HsvToRgb(hue, saturation, value);
        activeColor = {
            rgb.x * 8.0f,
            rgb.y * 8.0f,
            rgb.z * 8.0f,
        };

        lastHue = hue;
    }

    strokeActive = true;

    previousPosition = currentPosition;

    PaintBrush(currentPosition);
}

void SceneEditor::ContinueStroke(POINT const currentPosition)
{
    if (!strokeActive)
    {
        return;
    }

    float const dx = static_cast<float>(currentPosition.x - previousPosition.x);
    float const dy = static_cast<float>(currentPosition.y - previousPosition.y);
    float const length = std::sqrt(dx * dx + dy * dy);

    float const spacing = std::max(1.0f, brushRadius * 0.5f);
    int const stepCount = std::max(1, static_cast<int>(std::ceil(length / spacing)));

    for (int step = 1; step <= stepCount; ++step)
    {
        float const t = static_cast<float>(step) / static_cast<float>(stepCount);

        POINT const position{
            .x = std::lround(previousPosition.x + dx * t),
            .y = std::lround(previousPosition.y + dy * t),
        };

        PaintBrush(position);
    }

    previousPosition = currentPosition;
}

void SceneEditor::EndStroke()
{
    strokeActive = false;
}

SceneChange SceneEditor::TakePendingChange()
{
    SceneChange const result = pendingChange;
    pendingChange = {};
    return result;
}

int SceneEditor::GetBrushRadius() const
{
    return brushRadius;
}

void SceneEditor::SetBrushRadius(int const radius)
{
    brushRadius = std::max(1, radius);
}

bool SceneEditor::IsStrokeActive() const
{
    return strokeActive;
}

void SceneEditor::PaintBrush(POINT const center)
{
    for (int y = center.y - brushRadius; y <= center.y + brushRadius; ++y)
    {
        for (int x = center.x - brushRadius; x <= center.x + brushRadius; ++x)
        {
            int const dx = x - center.x;
            int const dy = y - center.y;
            if (dx * dx + dy * dy > brushRadius * brushRadius)
            {
                continue;
            }

            PaintPixel(x, y);
        }
    }
}

void SceneEditor::PaintPixel(int const x, int const y)
{
    if (!scene.IsInside(x, y)) return;

    if (activeTool == EditTool::Emissive)
    {
        pendingChange.emissionChanged = true;
        pendingChange.obstacleChanged = true;
        scene.SetEmissiveObstacle(x, y, activeColor);
    }
    else if (activeTool == EditTool::Obstacle)
    {
        if (scene.IsEmissive(x, y))
        {
            pendingChange.emissionChanged = true;
        }
        pendingChange.obstacleChanged = true;
        scene.SetObstacle(x, y);
    }
    else if (activeTool == EditTool::Eraser)
    {
        if (scene.IsEmissive(x, y))
        {
            pendingChange.emissionChanged = true;
        }
        if (scene.IsObstacle(x, y))
        {
            pendingChange.obstacleChanged = true;
        }
        scene.Erase(x, y);
    }

    pendingChange.dirtyRect.Include(x, y, scene.width, scene.height);
}
