struct VsOut { float4 pos : SV_Position; float4 color : COLOR0; };
cbuffer Tint : register(b0) { float4 tint; };
VsOut vs(float2 pos : POSITION, float4 color : COLOR) { VsOut o; o.pos = float4(pos, 0, 1); o.color = color; return o; }
float4 ps(VsOut i) : SV_Target { return i.color * tint; }
