cbuffer UniformBlock : register(b0, space3) {
  float4 LeftColor : packoffset(c0);
  float4 RightColor : packoffset(c1);
  float Progress : packoffset(c2);
};

float4 main(float2 TexCoord : TEXCOORD0) : SV_Target0 {
  if (TexCoord.x > Progress) {
    return RightColor;
  } else {
    return LeftColor;
  }
}