cbuffer UniformBlock : register(b0, space3) {
  float4 Color : packoffset(c0);
};

Texture2D<float4> Texture : register(t0, space2);
SamplerState Sampler : register(s0, space2);

float4 main(float2 TexCoord : TEXCOORD0) : SV_Target0 {
  float4 FullColor = float4(Color.r, Color.g, Color.b, 1.0);
  return lerp(Texture.Sample(Sampler, TexCoord), FullColor, Color.a);
}