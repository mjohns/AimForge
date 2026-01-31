struct InstanceData {
  float4x4 MatrixTransform;
  float4 Color;
};

// The structured buffer containing all instance data
StructuredBuffer<InstanceData> InstanceBuffer : register(t0, space0);

struct Input {
  float3 Position : POSITION0;
  uint InstanceID : SV_InstanceID;  // System-generated ID
};

struct Output {
  float4 Position : SV_Position;
  float4 Color : COLOR;
};

Output main(Input input) {
  Output output;

  // Pull the specific data for this instance using the ID
  InstanceData data = InstanceBuffer[input.InstanceID];

  output.Position = mul(data.MatrixTransform, float4(input.Position, 1.0f));
  output.Color = data.Color;

  return output;
}