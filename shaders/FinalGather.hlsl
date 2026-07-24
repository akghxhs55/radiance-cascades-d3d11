#include "CascadeConstants.hlsli"

Texture2D<float4> Cascade0 : register(t0);

float4 LoadCascadeRay(int2 probeCoord, uint rayIndex)
{
    uint probeIndex = probeCoord.y * ProbeCountX + probeCoord.x;
    uint linearIndex = probeIndex * RaysPerProbe + rayIndex;
    uint textureWidth = uint(RadianceTextureSize.x);
    uint2 texelCoord = uint2(linearIndex % textureWidth, linearIndex / textureWidth);

    return Cascade0.Load(int3(texelCoord, 0));
}

float3 GatherProbeRadiance(uint2 probeCoord)
{
    float3 radiance = 0.0;

    [loop]
    for (uint rayIndex = 0; rayIndex < RaysPerProbe; ++rayIndex)
    {
        radiance += LoadCascadeRay(probeCoord, rayIndex).rgb;
    }

    return radiance / float(RaysPerProbe);
}

float3 GatherCascadeAtPosition(float2 scenePosition)
{
    float2 gridPosition = (scenePosition - ProbeOffset) / ProbeSpacing;

    int2 lowerProbe = int2(floor(gridPosition));
    float2 interpolation = frac(gridPosition);

    int2 probeLimit = int2(ProbeCountX, ProbeCountY) - 1;

    uint2 probe00 = uint2(clamp(lowerProbe, int2(0, 0), probeLimit));
    uint2 probe10 = uint2(clamp(lowerProbe + int2(1, 0), int2(0, 0), probeLimit));
    uint2 probe01 = uint2(clamp(lowerProbe + int2(0, 1), int2(0, 0), probeLimit));
    uint2 probe11 = uint2(clamp(lowerProbe + int2(1, 1), int2(0, 0), probeLimit));

    float3 radiance00 = GatherProbeRadiance(probe00);
    float3 radiance10 = GatherProbeRadiance(probe10);
    float3 radiance01 = GatherProbeRadiance(probe01);
    float3 radiance11 = GatherProbeRadiance(probe11);

    float3 upperRow = lerp(radiance00, radiance10, interpolation.x);
    float3 lowerRow = lerp(radiance01, radiance11, interpolation.x);

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

    float3 radiance = GatherCascadeAtPosition(scenePosition);

    float exposure = 0.25;

    float3 inverseGamma = 1.0 / 2.2;
    float3 color = 1.0 - exp(-radiance * exposure);
    color = pow(saturate(color), inverseGamma);

    return float4(color, 1.0);
}

float4 PSDebugCascade(PSInput input) : SV_Target
{
	uint width;
	uint height;
	Cascade0.GetDimensions(width, height);

	uint2 texel = min(
		uint2(saturate(input.uv) * float2(width, height)),
		uint2(width - 1, height - 1)
	);

    float4 sample = Cascade0.Load(int3(texel, 0));

    float3 color = 1.0 - exp(-sample.rgb * 0.25);
    return float4(color, 1.0);
}