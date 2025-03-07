cbuffer UniformBlock : register(b0, space3)
{
    float4 Color : packoffset(c0);
};

float4 main() : SV_Target0
{
    return Color;
}