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

float4 LoadUpperRay(uint2 probeCoord, uint rayIndex)
{
    uint upperRaysPerProbe = RaysPerProbe * CascadeBranchingFactor;
    uint probeIndex = probeCoord.y * UpperProbeCountX + probeCoord.x;
    uint linearIndex = probeIndex * upperRaysPerProbe + rayIndex;

    uint textureWidth;
    uint textureHeight;
    UpperCascade.GetDimensions(textureWidth, textureHeight);

    uint2 texelCoord = uint2(linearIndex % textureWidth, linearIndex / textureWidth);
    if (texelCoord.y >= textureHeight)
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    return UpperCascade.Load(int3(texelCoord, 0));
}

float4 SampleUpperProbe(uint2 probeCoord, uint currentRayIndex)
{
    uint firstUpperRay = currentRayIndex * CascadeBranchingFactor;
    float4 radiance = 0.0;

    [unroll]
    for (uint rayOffset = 0; rayOffset < CascadeBranchingFactor; ++rayOffset)
    {
        radiance += LoadUpperRay(probeCoord, firstUpperRay + rayOffset);
    }

    return radiance / float(CascadeBranchingFactor);
}

float4 SampleUpperCascade(float2 probePosition, uint currentRayIndex)
{
    if (UpperProbeCountX == 0 || UpperProbeCountY == 0)
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    float upperProbeSpacing = ProbeSpacing * 2.0;
    float upperProbeOffset = -0.5 * upperProbeSpacing;
    float2 upperGridPosition = (probePosition - upperProbeOffset) / upperProbeSpacing;

    int2 lowerProbe = int2(floor(upperGridPosition));
    float2 interpolation = frac(upperGridPosition);
    int2 upperProbeLimit = int2(UpperProbeCountX, UpperProbeCountY) - 1;

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
    uint2 texelCoord = uint2(input.position.xy);
    uint linearIndex = texelCoord.y * uint(RadianceTextureSize.x) + texelCoord.x;

    uint probeIndex = linearIndex / RaysPerProbe;
    uint rayIndex = linearIndex % RaysPerProbe;

    uint2 probeCoord = uint2(probeIndex % ProbeCountX, probeIndex / ProbeCountX);

    if (probeCoord.x >= ProbeCountX || probeCoord.y >= ProbeCountY)
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    float2 probePosition = float2(probeCoord) * ProbeSpacing + ProbeOffset;

    float angle = (float(rayIndex) + 0.5) / float(RaysPerProbe) * Tau;
    float2 rayDirection = float2(cos(angle), sin(angle));

    float4 localResult = RayMarchInterval(probePosition, rayDirection, IntervalStart, IntervalEnd);

    if (MergeUpperCascade != 0 && CascadeIndex + 1 < CascadeCount && localResult.a > 0.001)
    {
        float4 upperResult = SampleUpperCascade(probePosition, rayIndex);
        localResult.rgb += localResult.a * upperResult.rgb;
        localResult.a *= upperResult.a;
    }

    return localResult;
}
