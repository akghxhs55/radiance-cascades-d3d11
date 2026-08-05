static const uint MaxCircles = 16;
static const uint MaxBoxes = 16;
static const uint CascadeBranchingFactor = 4;
static const uint MaxRayMarchSteps = 128;

static const uint HitNothing = 0;
static const uint HitCircle = 1;
static const uint HitBox = 2;

static const float HitEpsilon = 0.2;
static const float Tau = 6.28318530718;

cbuffer SceneConstants : register(b0)
{
    float4 CircleData[MaxCircles];
    float4 CircleEmission[MaxCircles];
    float4 BoxData[MaxBoxes];

    uint CircleCount;
    uint BoxCount;

    uint2 ScenePadding;
};

#include "CascadeConstants.hlsli"

Texture2D<float4> UpperCascade : register(t0);

float SignedDistanceToBox(float2 position, float4 boxData)
{
    float2 offset = abs(position - boxData.xy) - boxData.zw;
    return length(max(offset, 0.0)) + min(max(offset.x, offset.y), 0.0);
}

float DistanceToScene(float2 position, out uint hitType, out uint hitIndex)
{
    float nearestDistance = 3.402823466e+38;
    hitType = HitNothing;
    hitIndex = 0;

    [loop]
    for (uint circleIndex = 0; circleIndex < CircleCount; ++circleIndex)
    {
        float circleDistance = length(position - CircleData[circleIndex].xy) - CircleData[circleIndex].z;

        if (circleDistance < nearestDistance)
        {
            nearestDistance = circleDistance;
            hitType = HitCircle;
            hitIndex = circleIndex;
        }
    }

    [loop]
    for (uint boxIndex = 0; boxIndex < BoxCount; ++boxIndex)
    {
        float boxDistance = SignedDistanceToBox(position, BoxData[boxIndex]);

        if (boxDistance < nearestDistance)
        {
            nearestDistance = boxDistance;
            hitType = HitBox;
            hitIndex = boxIndex;
        }
    }

    return nearestDistance;
}

float4 RayMarchInterval(float2 probePosition, float2 rayDirection, float intervalStart, float intervalEnd)
{
    float distanceAlongRay = max(intervalStart, 0.0);

    [loop]
    for (uint stepIndex = 0; stepIndex < MaxRayMarchSteps; ++stepIndex)
    {
        if (distanceAlongRay >= intervalEnd) break;

        float2 samplePosition = probePosition + rayDirection * distanceAlongRay;

        uint hitType;
        uint hitIndex;
        float sceneDistance = DistanceToScene(samplePosition, hitType, hitIndex);

        if (sceneDistance <= HitEpsilon)
        {
            if (hitType == HitCircle)
            {
                return float4(CircleEmission[hitIndex].rgb, 0.0);
            }

            return float4(0.0, 0.0, 0.0, 0.0);
        }

        distanceAlongRay += max(sceneDistance, HitEpsilon);
    }

    return float4(0.0, 0.0, 0.0, 1.0);
}

float4 LoadUpperCascadeRay(uint2 probeCoord, uint rayIndex)
{
    uint2 upperRayBlockSize = RayBlockSizeForCascade(CascadeIndex + 1);

    uint2 rayCoord = uint2(rayIndex % upperRayBlockSize.x, rayIndex / upperRayBlockSize.x);
    uint2 texelCoord = probeCoord * upperRayBlockSize + rayCoord;

    return UpperCascade.Load(int3(texelCoord, 0));
}

float4 SampleUpperProbe(uint2 probeCoord, uint currentRayIndex)
{
    uint firstUpperRay = currentRayIndex * CascadeBranchingFactor;

    float4 radiance = 0.0;
    [unroll]
    for (uint rayOffset = 0; rayOffset < CascadeBranchingFactor; ++rayOffset)
    {
        radiance += LoadUpperCascadeRay(probeCoord, firstUpperRay + rayOffset);
    }

    return radiance / float(CascadeBranchingFactor);
}

float4 SampleUpperCascade(float2 probePosition, uint currentRayIndex)
{
    uint2 upperProbeCount = ProbeCountForCascade(CascadeIndex + 1);

    if (upperProbeCount.x == 0 || upperProbeCount.y == 0)
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    float upperProbeSpacing = ProbeSpacingForCascade(CascadeIndex + 1);
    float2 upperGridPosition = probePosition / upperProbeSpacing + 0.5;

    int2 lowerProbe = int2(floor(upperGridPosition));
    float2 interpolation = frac(upperGridPosition);
    int2 upperProbeLimit = int2(upperProbeCount.x, upperProbeCount.y) - 1;

    uint2 probe00 = uint2(clamp(lowerProbe, int2(0, 0), upperProbeLimit));
    uint2 probe10 = uint2(clamp(lowerProbe + int2(1, 0), int2(0, 0), upperProbeLimit));
    uint2 probe01 = uint2(clamp(lowerProbe + int2(0, 1), int2(0, 0), upperProbeLimit));
    uint2 probe11 = uint2(clamp(lowerProbe + int2(1, 1), int2(0, 0), upperProbeLimit));

    float4 radiance00 = SampleUpperProbe(probe00, currentRayIndex);
    float4 radiance10 = SampleUpperProbe(probe10, currentRayIndex);
    float4 radiance01 = SampleUpperProbe(probe01, currentRayIndex);
    float4 radiance11 = SampleUpperProbe(probe11, currentRayIndex);

    float4 upperRow = lerp(radiance00, radiance10, interpolation.x);
    float4 lowerRow = lerp(radiance01, radiance11, interpolation.x);
    return lerp(upperRow, lowerRow, interpolation.y);
}

struct PSInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 PSCascade(PSInput input) : SV_Target
{
    uint2 probeCount = ProbeCountForCascade(CascadeIndex);
    uint probeSpacing = ProbeSpacingForCascade(CascadeIndex);
    uint raysPerProbe = RaysPerProbeForCascade(CascadeIndex);
    uint2 rayBlockSize = RayBlockSizeForCascade(CascadeIndex);
    float2 interval = IntervalForCascade(CascadeIndex);
    float intervalStart = interval.x;
    float intervalEnd = interval.y;

    uint2 texelCoord = uint2(input.position.xy);
    uint2 probeCoord = texelCoord / rayBlockSize;

    if (probeCoord.x >= probeCount.x || probeCoord.y >= probeCount.y)
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    float2 probePosition = (float2(probeCoord) - 0.5) * probeSpacing;

    uint rayIndex = (texelCoord.x % rayBlockSize.x) + (texelCoord.y % rayBlockSize.y) * rayBlockSize.x;

    float angle = (float(rayIndex) + 0.5) / float(raysPerProbe) * Tau;
    float2 rayDirection = float2(cos(angle), sin(angle));

    float4 localResult = RayMarchInterval(probePosition, rayDirection, intervalStart, intervalEnd);

    if (MergeUpperCascade != 0 && CascadeIndex + 1 < CascadeCount && localResult.a > 0.001)
    {
        float4 upperResult = SampleUpperCascade(probePosition, rayIndex);
        localResult.rgb += localResult.a * upperResult.rgb;
        localResult.a *= upperResult.a;
    }

    return localResult;
}
