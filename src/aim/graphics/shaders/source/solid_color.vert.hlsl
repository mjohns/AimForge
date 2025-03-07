struct Input
{
    float3 Position : POSITION0;
};


struct Output
{
    float4 Color : COLOR;
    float4 Position : SV_Position;
};

Output main(Input input)
{
    Output output;
    output.Color = float4(1.0f, 1.0f, 0.0f, 1.0f);
    output.Position = float4(input.Position, 1.0f);
    //output.Position = mul(input.Transform, float4(input.Position, 1.0f));
    return output;
}