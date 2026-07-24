struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

static const float2 FullScreenPositions[3] =
{
    float2(-1.0, -1.0),
    float2(-1.0,  3.0),
    float2( 3.0, -1.0),
};

VSOutput VSMain(uint vertexId : SV_VertexID)
{
    VSOutput output;

    float2 position = FullScreenPositions[vertexId];

    output.position = float4(position, 0.0, 1.0);
    output.uv = position * float2(0.5, -0.5) + 0.5;

    return output;
}
