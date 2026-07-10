struct VSInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

cbuffer SceneConstants : register(b0)
{
    float4 circleData[16];       // xy: center, z: radius
    float4 circleEmission[16];   // rgb: emitted radiance
    float4 boxData[16];          // xy: center, zw: half extent
    uint circleCount;
    uint boxCount;
    float2 padding;
}

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.position = float4(input.position, 1.0);
    output.uv = input.uv;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    // Scene.h uses (-1, -1) for bottom-left and (1, 1) for top-right.
    float2 scenePosition = float2(input.uv.x * 2.0 - 1.0, (1.0 - input.uv.y) * 2.0 - 1.0);
    float3 color = float3(0.015, 0.020, 0.035);

    for (uint boxIndex = 0; boxIndex < boxCount; ++boxIndex)
    {
        float2 offset = abs(scenePosition - boxData[boxIndex].xy) - boxData[boxIndex].zw;
        float signedDistance = length(max(offset, 0.0)) + min(max(offset.x, offset.y), 0.0);

        if (signedDistance <= 0.0)
        {
            color = float3(0.12, 0.14, 0.18);
        }
    }

    for (uint circleIndex = 0; circleIndex < circleCount; ++circleIndex)
    {
        float distanceToCenter = length(scenePosition - circleData[circleIndex].xy);

        if (distanceToCenter <= circleData[circleIndex].z)
        {
            color = circleEmission[circleIndex].rgb;
        }
    }

    // The sample uses HDR emission values; tone-map them for the UNORM back buffer.
    color = 1.0 - exp(-color * 0.25);
    return float4(color, 1.0);
}
