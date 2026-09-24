RWStructuredBuffer<uint> output : register(u0);
cbuffer Params : register(b0) { uint base; };
[numthreads(64, 1, 1)] void cs(uint3 id : SV_DispatchThreadID) { output[id.x] = base + id.x * 3; }
