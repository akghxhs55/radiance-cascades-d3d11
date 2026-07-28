#include "CascadeConstants.hlsli"

static const uint DisplayModeVisibility = 2;

cbuffer FinalGatherConstants : register(b0)
{
    uint DisplayMode;
    uint3 FinalGatherPadding;
};

Texture2D<float4> SelectedCascade : register(t0);

float4 LoadCascadeRay(int2 probeCoord, uint rayIndex)
{
    uint probeIndex = probeCoord.y * ProbeCountX + probeCoord.x;
    uint linearIndex = probeIndex * RaysPerProbe + rayIndex;
    uint textureWidth = uint(RadianceTextureSize.x);
    uint2 texelCoord = uint2(linearIndex % textureWidth, linearIndex / textureWidth);

    return SelectedCascade.Load(int3(texelCoord, 0));
}

float4 GatherProbe(uint2 probeCoord)
{
    float4 result = 0.0;

    [loop]
    for (uint rayIndex = 0; rayIndex < RaysPerProbe; ++rayIndex)
    {
        result += LoadCascadeRay(probeCoord, rayIndex);
    }

    return result / float(RaysPerProbe);
}

float4 GatherCascadeAtPosition(float2 scenePosition)
{
    float2 gridPosition = (scenePosition - ProbeOffset) / ProbeSpacing;

    int2 lowerProbe = int2(floor(gridPosition));
    float2 interpolation = frac(gridPosition);

    int2 probeLimit = int2(ProbeCountX, ProbeCountY) - 1;

    uint2 probe00 = uint2(clamp(lowerProbe, int2(0, 0), probeLimit));
    uint2 probe10 = uint2(clamp(lowerProbe + int2(1, 0), int2(0, 0), probeLimit));
    uint2 probe01 = uint2(clamp(lowerProbe + int2(0, 1), int2(0, 0), probeLimit));
    uint2 probe11 = uint2(clamp(lowerProbe + int2(1, 1), int2(0, 0), probeLimit));

    float4 result00 = GatherProbe(probe00);
    float4 result10 = GatherProbe(probe10);
    float4 result01 = GatherProbe(probe01);
    float4 result11 = GatherProbe(probe11);

    float4 upperRow = lerp(result00, result10, interpolation.x);
    float4 lowerRow = lerp(result01, result11, interpolation.x);

    return lerp(upperRow, lowerRow, interpolation.y);
}

struct PSInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 PSFinalGather(PSInput input) : SV_Target
{
    float2 scenePosition = input.position.xy;

    float4 result = GatherCascadeAtPosition(scenePosition);

    if (DisplayMode == DisplayModeVisibility)
    {
        return float4(result.aaa, 1.0);
    }

    float exposure = 0.25;
    
    float3 color = 1.0 - exp(-result.rgb * exposure);

    return float4(color, 1.0);
}

float4 PSDebugCascade(PSInput input) : SV_Target
{
	uint width;
	uint height;
	SelectedCascade.GetDimensions(width, height);

	uint2 texel = min(
		uint2(saturate(input.uv) * float2(width, height)),
		uint2(width - 1, height - 1)
	);

    float4 sample = SelectedCascade.Load(int3(texel, 0));

    float3 color = 1.0 - exp(-sample.rgb * 0.25);
    return float4(color, 1.0);
}
