cbuffer MatrixBuffer : register(b0)
{
    matrix m;
};

cbuffer CameraBuffer : register(b1)
{
    matrix vp;
};

struct VSInput
{
    float3 pos : POSITION;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float3 WorldPos : TEXCOORD0;
};

VSOutput main(VSInput vertex)
{
    VSOutput output;
    float4 worldPos = mul(float4(vertex.pos, 1.0f), m);
    output.WorldPos = worldPos.xyz;
    output.pos = mul(worldPos, vp);
    return output;
}