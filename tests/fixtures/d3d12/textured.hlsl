Texture2D tex : register(t0);
SamplerState point_sampler : register(s0);
struct VsOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };
VsOut vs(uint id : SV_VertexID)
{
    VsOut o;
    float2 uv = float2((id << 1) & 2, id & 2);
    o.uv = uv;
    o.pos = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return o;
}
float4 ps(VsOut i) : SV_Target { return tex.Sample(point_sampler, i.uv); }
