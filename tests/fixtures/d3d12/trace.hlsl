RaytracingAccelerationStructure scene : register(t0);
RWByteAddressBuffer outbuf : register(u0);
struct Payload { uint hit; };

[shader("raygeneration")] void raygen()
{
    uint2 id = DispatchRaysIndex().xy;
    uint2 dim = DispatchRaysDimensions().xy;
    RayDesc ray;
    ray.Origin = float3((id.x + 0.5) / dim.x * 2 - 1, (id.y + 0.5) / dim.y * 2 - 1, 1);
    ray.Direction = float3(0, 0, -1);
    ray.TMin = 0.001;
    ray.TMax = 100;
    Payload p;
    p.hit = 0xffffffff;
    TraceRay(scene, RAY_FLAG_NONE, 0xff, 0, 1, 0, ray, p);
    outbuf.Store((id.y * dim.x + id.x) * 4, p.hit);
}

[shader("closesthit")] void chit(inout Payload p, BuiltInTriangleIntersectionAttributes a)
{
    p.hit = 1 + InstanceID();
}

[shader("miss")] void miss(inout Payload p)
{
    p.hit = 0;
}
