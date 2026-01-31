struct PixelInput {
  float4 Position : SV_Position;
  float4 Color : COLOR;
};

float4 main(PixelInput input) : SV_Target0 {
  return input.Color;
}