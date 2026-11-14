cbuffer Region : register(b0)
{
    uint4 origin_extent;
    uint4 size_encode;
};

Texture2D<float4> source : register(t0);

float4 vertex_main(uint id : SV_VertexID) : SV_Position
{
    float2 corner = float2((id << 1) & 2, id & 2);
    return float4(corner * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

float4 pixel_main(float4 position : SV_Position) : SV_Target
{
    uint2 pixel = uint2(position.xy);
    uint2 at = origin_extent.xy + pixel * origin_extent.zw / size_encode.xy;
    float4 colour = saturate(source.Load(int3(at, 0)));
    if (size_encode.z != 0u) {
        float3 curve = 1.055 * pow(colour.rgb, 1.0 / 2.4) - 0.055;
        colour.rgb = colour.rgb <= 0.0031308 ? colour.rgb * 12.92 : curve;
    }
    return colour;
}
