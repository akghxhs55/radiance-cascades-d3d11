#pragma once

#include <d3dcommon.h>

#include <algorithm>

struct DirtyRect final
{
    int left = 0;
    int top = 0;
    int right = 0;  // exclusive
    int bottom = 0; // exclusive
    bool valid = false;

    void Include(int x, int y, UINT sceneWidth, UINT sceneHeight);
};

struct SceneChange final
{
    DirtyRect dirtyRect;
    bool obstacleChanged = false;
    bool emissionChanged = false;
};

inline void DirtyRect::Include(int const x, int const y, UINT const sceneWidth, UINT const sceneHeight)
{
    if (x < 0 || y < 0 || x >= sceneWidth || y >= sceneHeight) return;

    if (!valid)
    {
        left = x;
        top = y;
        right = x + 1;
        bottom = y + 1;
        valid = true;
    }
    else
    {
        left = std::min(left, x);
        top = std::min(top, y);
        right = std::max(right, x + 1);
        bottom = std::max(bottom, y + 1);
    }
}
