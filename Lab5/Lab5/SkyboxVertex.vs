cbuffer CameraBuffer : register(b0)
{
    matrix vp;
};

struct VS_INPUT
{
    float3 pos: POSITION;
};

struct PS_INPUT
{
    float4 pos: SV_POSITION;
    float3 tex: TEXCOORD;
};

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;
    output.pos = mul(float4(input.pos, 1.0), vp);
    output.pos = output.pos.xyww;
    output.tex = input.pos;
    return output;
}