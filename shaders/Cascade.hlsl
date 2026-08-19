static const float Tau = 6.28318530718;

static const uint MaxRayMarchSteps = 128;
static const float DistanceFieldSafetyMargin = 1.0;
static const float MinimumRayMarchStep = 0.25;

#include "CascadeConstants.hlsli"

Texture2D<float4> UpperCascade : register(t0);
Texture2D<float> Obstacle : register(t1);
Texture2D<float4> Emission : register(t2);
Texture2D<float> DistanceField : register(t3);

bool IsInsideScene(float2 scenePosition)
{
    return all(scenePosition >= 0.0) && all(scenePosition < float2(SceneSize));
}

float DistanceToSceneBounds(float2 position)
{
	float2 outsideDistance = max(max(-position, position - float2(SceneSize)), 0.0);
	return length(outsideDistance);
}

float4 RayMarchInterval(float2 probePosition, float2 rayDirection, float intervalStart, float intervalEnd)
{
    float distanceAlongRay = max(intervalStart, 0.0);

    [loop]
    for (uint stepIndex = 0; stepIndex < MaxRayMarchSteps; ++stepIndex)
    {
        if (distanceAlongRay >= intervalEnd)
        {
            break;
        }

        float2 samplePosition = probePosition + rayDirection * distanceAlongRay;

        if (!IsInsideScene(samplePosition))
        {
            float distanceToScene = DistanceToSceneBounds(samplePosition);

            distanceAlongRay += max(distanceToScene, MinimumRayMarchStep);

            continue;
        }

        uint2 texel = uint2(floor(samplePosition));

        if (Obstacle.Load(int3(texel, 0)) > 0.5)
        {
            float3 emission = Emission.Load(int3(texel, 0)).rgb;
            return float4(emission, 0.0);
        }

        float distanceToObstacle = DistanceField.Load(int3(texel, 0));

        distanceAlongRay += max(distanceToObstacle - DistanceFieldSafetyMargin, MinimumRayMarchStep);
    }

    return float4(0.0, 0.0, 0.0, 1.0);
}

float4 LoadUpperProbe(uint2 probeCoord, uint rayIndex)
{
    uint2 storedUpperRayBlockSize = StoredRayBlockSizeForCascade(CascadeIndex + 1);

    uint2 rayCoord = uint2(rayIndex % storedUpperRayBlockSize.x, rayIndex / storedUpperRayBlockSize.x);
    uint2 texelCoord = probeCoord * storedUpperRayBlockSize + rayCoord;

    return UpperCascade.Load(int3(texelCoord, 0));
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

    float4 radiance00 = LoadUpperProbe(probe00, currentRayIndex);
    float4 radiance10 = LoadUpperProbe(probe10, currentRayIndex);
    float4 radiance01 = LoadUpperProbe(probe01, currentRayIndex);
    float4 radiance11 = LoadUpperProbe(probe11, currentRayIndex);

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
    uint2 storedRayBlockSize = StoredRayBlockSizeForCascade(CascadeIndex);
    float2 interval = IntervalForCascade(CascadeIndex);
    float intervalStart = interval.x;
    float intervalEnd = interval.y;

    uint2 texelCoord = uint2(input.position.xy);
    uint2 probeCoord = texelCoord / storedRayBlockSize;

    if (probeCoord.x >= probeCount.x || probeCoord.y >= probeCount.y)
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    float2 probePosition = (float2(probeCoord) - 0.5) * probeSpacing;

    uint storedRayIndex = (texelCoord.x % storedRayBlockSize.x) + (texelCoord.y % storedRayBlockSize.y) * storedRayBlockSize.x;

    float4 accumulatedResult = 0.0;

    for (uint rayOffset = 0; rayOffset < 4; ++rayOffset)
    {
        uint tracedRayIndex = storedRayIndex * 4 + rayOffset;

        float angle = (float(tracedRayIndex) + 0.5) / float(raysPerProbe) * Tau;
        float2 rayDirection = float2(cos(angle), sin(angle));

        float4 localResult = RayMarchInterval(probePosition, rayDirection, intervalStart, intervalEnd);

        if (MergeUpperCascade != 0 && CascadeIndex + 1 < CascadeCount && localResult.a > 0.001)
        {
            float4 upperResult = SampleUpperCascade(probePosition, tracedRayIndex);
            localResult.rgb += localResult.a * upperResult.rgb;
            localResult.a *= upperResult.a;
        }

        accumulatedResult += localResult;
    }

    return accumulatedResult * 0.25;
}
