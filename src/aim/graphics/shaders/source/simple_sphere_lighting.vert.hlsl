cbuffer UniformBlock : register(b0, space1) {
  float4 BaseColor : packoffset(c0);
  float4 LightColor : packoffset(c1);
  float4 LightDirection : packoffset(c2);
  float4x4 MatrixTransform : packoffset(c3);
};

struct Input {
  float3 Position : POSITION0;
};

struct Output {
  float4 Position : SV_Position;
  float4 Color : COLOR0;
};

Output main(Input input) {
  Output output;
  output.Position = mul(MatrixTransform, float4(input.Position, 1.0f));

  float diffuse = saturate(dot(input.Position, LightDirection.xyz));

  float4 diffuseColor = LightColor * diffuse;
  output.Color = saturate(diffuseColor * (1 - BaseColor.a) + BaseColor * BaseColor.a);
  output.Color.a = 1.0;
  return output;
}