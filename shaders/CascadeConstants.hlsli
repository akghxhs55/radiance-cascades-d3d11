cbuffer CascadePassConstants : register(b1)
{
    uint2 SceneSize;

    uint CascadeIndex;
    uint CascadeCount;

    uint BaseProbeSpacing;
    uint BaseRayExponent;
    float BaseIntervalLength;
    uint MergeUpperCascade;
};

uint CascadeScale(uint cascadeIndex)
{
    return 1u << cascadeIndex;
}

uint CascadeScaleSquared(uint cascadeIndex)
{
    return 1u << (cascadeIndex * 2);
}

uint ProbeSpacingForCascade(uint cascadeIndex)
{
    return BaseProbeSpacing << cascadeIndex;
}

uint2 ProbeCountForCascade(uint cascadeIndex)
{
    uint spacing = ProbeSpacingForCascade(cascadeIndex);

    // round(sceneSize / spacing) + 2
    return (SceneSize + spacing / 2) / spacing + 2;
}

uint RaysPerProbeForCascade(uint cascadeIndex)
{
    return 1u << (BaseRayExponent + cascadeIndex * 2);
}

uint2 RayBlockSizeForCascade(uint cascadeIndex)
{
    uint widthExponent = BaseRayExponent / 2 + cascadeIndex;
    uint heightExponent = (BaseRayExponent + 1) / 2 + cascadeIndex;

    return uint2(1u << widthExponent, 1u << heightExponent);
}

float2 IntervalForCascade(uint cascadeIndex)
{
    float scaleSquared = float(CascadeScaleSquared(cascadeIndex));

    float intervalStart = BaseIntervalLength * (scaleSquared - 1.0) / 3.0;
    float intervalEnd = BaseIntervalLength * (scaleSquared * 4.0 - 1.0) / 3.0;

    return float2(intervalStart, intervalEnd);
}
