cbuffer CascadeConstants : register(b1)
{
    uint CascadeIndex;
    uint CascadeCount;

    uint ProbeCountX;
    uint ProbeCountY;
    float ProbeSpacing;
    float ProbeOffset;

    uint RaysPerProbe;
    float IntervalStart;
    float IntervalEnd;

    float2 RadianceTextureSize;

    uint UpperProbeCountX;
    uint UpperProbeCountY;

    uint3 CascadePadding;
};
