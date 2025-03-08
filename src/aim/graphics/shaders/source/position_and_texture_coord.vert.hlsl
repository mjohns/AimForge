cbuffer UniformBlock : register(b0, space1)
{
    float4 TexScale : packoffset(c0);
    float4x4 MatrixTransform : packoffset(c1);
};

struct Input
{
    float3 Position : POSITION0;
    float2 TexCoord : TEXCOORD1;
};


struct Output
{
    float2 TexCoord : TEXCOORD0;
    float4 Position : SV_Position;
};

Output main(Input input)
{
    Output output;
    output.TexCoord.x = input.TexCoord.x * TexScale.x;
    output.TexCoord.y = input.TexCoord.y * TexScale.y;
    output.Position = mul(MatrixTransform, float4(input.Position, 1.0f));
    return output;
}