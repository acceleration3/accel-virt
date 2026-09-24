// computeparticles, computeshader, particlefire, raytracing, bloom, radialblur, offscreen
#include "framework.h"

namespace samples {

namespace {

// ---------------------------------------------------------------- shared pieces

#define COMPUTE_QUAD_HLSL "struct Quad { float4 position : SV_Position; float2 uv : TEXCOORD; };\n"

// A full-screen triangle from SV_VertexID alone: no vertex buffer, no layout.
const char *const quad_source = COMPUTE_QUAD_HLSL R"(
Quad vs(uint id : SV_VertexID) {
    Quad o;
    o.uv = float2((id << 1) & 2, id & 2);
    o.position = float4(o.uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return o;
}
)";

void draw_quad(ID3D12GraphicsCommandList *list)
{
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->DrawInstanced(3, 1, 0, 0);
}

enum class Blend { opaque, additive, alpha };

void set_blend(D3D12_GRAPHICS_PIPELINE_STATE_DESC &desc, Blend mode)
{
    D3D12_RENDER_TARGET_BLEND_DESC &rt = desc.BlendState.RenderTarget[0];
    rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    rt.BlendOp = rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D12_BLEND_ONE;
    rt.DestBlendAlpha = D3D12_BLEND_ONE;
    rt.SrcBlend = D3D12_BLEND_ONE;
    rt.DestBlend = D3D12_BLEND_ZERO;
    rt.BlendEnable = FALSE;
    if (mode == Blend::additive) {
        rt.BlendEnable = TRUE;
        rt.DestBlend = D3D12_BLEND_ONE;
    } else if (mode == Blend::alpha) {
        rt.BlendEnable = TRUE;
        rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    }
}

void set_depth(D3D12_GRAPHICS_PIPELINE_STATE_DESC &desc, bool test, bool write)
{
    desc.DepthStencilState.DepthEnable = test ? TRUE : FALSE;
    desc.DepthStencilState.DepthWriteMask = write ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
}

// The procedural meshes here are not all wound one way, so nothing is culled;
// and a pass drawn into `format` with or without a depth buffer.
D3D12_GRAPHICS_PIPELINE_STATE_DESC pass(Gpu &gpu, ID3D12RootSignature *root, D3D12_SHADER_BYTECODE vs,
                                        D3D12_SHADER_BYTECODE ps, DXGI_FORMAT format = Gpu::color_format,
                                        bool depth = true)
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = gpu.graphics(root, vs, ps);
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.RTVFormats[0] = format;
    if (!depth) {
        desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
        set_depth(desc, false, false);
    }
    return desc;
}

// A full-screen pass: no input layout, no depth.
D3D12_GRAPHICS_PIPELINE_STATE_DESC quad_pass(Gpu &gpu, ID3D12RootSignature *root, D3D12_SHADER_BYTECODE vs,
                                             D3D12_SHADER_BYTECODE ps, DXGI_FORMAT format = Gpu::color_format,
                                             bool depth_target = true)
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = pass(gpu, root, vs, ps, format, depth_target);
    desc.InputLayout = {nullptr, 0};
    set_depth(desc, false, false);
    return desc;
}

// An off-screen colour target, optionally with its own depth, readable as a
// texture through its own one-descriptor table; `uav` makes it a compute
// shader's output instead. Tracks its state between passes.
struct RenderTexture {
    ID3D12Resource             *texture = nullptr;
    ID3D12Resource             *depth_texture = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = {};
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = {};
    Descriptors                 srv;
    Descriptors                 uav;
    D3D12_RESOURCE_STATES       state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    uint32_t                    width = 0;
    uint32_t                    height = 0;

    void create(Gpu &gpu, uint32_t w, uint32_t h, DXGI_FORMAT format, bool unordered, bool with_depth)
    {
        width = w;
        height = h;
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = w;
        desc.Height = h;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.Flags = unordered ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        D3D12_CLEAR_VALUE clear = {};
        clear.Format = format;
        clear.Color[3] = 1.0f;
        state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        texture = gpu.resource(desc, state, unordered ? nullptr : &clear);
        if (texture == nullptr) return;
        srv = gpu.views(1);
        gpu.texture_view(texture, srv.cpu);
        if (unordered) {
            uav = gpu.views(1);
            gpu.device->CreateUnorderedAccessView(texture, nullptr, nullptr, uav.cpu);
        } else {
            rtv = gpu.rtvs(1).cpu;
            gpu.device->CreateRenderTargetView(texture, nullptr, rtv);
        }
        if (!with_depth) return;
        desc.Format = Gpu::depth_format;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        clear.Format = Gpu::depth_format;
        clear.DepthStencil = {1.0f, 0};
        depth_texture = gpu.resource(desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear);
        if (depth_texture == nullptr) return;
        dsv = gpu.dsvs(1).cpu;
        gpu.device->CreateDepthStencilView(depth_texture, nullptr, dsv);
    }
    void become(Gpu &gpu, D3D12_RESOURCE_STATES next)
    {
        gpu.barrier(texture, state, next);
        state = next;
    }
    void begin(Gpu &gpu, float r, float g, float b)
    {
        become(gpu, D3D12_RESOURCE_STATE_RENDER_TARGET);
        const float clear[4] = {r, g, b, 1.0f};
        gpu.list->OMSetRenderTargets(1, &rtv, FALSE, depth_texture != nullptr ? &dsv : nullptr);
        gpu.list->ClearRenderTargetView(rtv, clear, 0, nullptr);
        if (depth_texture != nullptr)
            gpu.list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
        gpu.viewport(0, 0, static_cast<float>(width), static_cast<float>(height));
    }
    void release()
    {
        safe_release(texture);
        safe_release(depth_texture);
    }
};

// ---- CPU-side procedural content

float smooth(float a, float b, float x)
{
    const float t = clamp((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float lattice(int x, int y, uint32_t seed)
{
    uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    h ^= h >> 16u;
    return static_cast<float>(h & 0xFFFFFFu) / 16777216.0f;
}

// Value noise that tiles with `period` lattice cells (period 0: no tiling).
float value_noise(float x, float y, uint32_t seed, int period)
{
    const float fx = floor(x), fy = floor(y);
    int ix = static_cast<int>(fx), iy = static_cast<int>(fy);
    const float tx = x - fx, ty = y - fy;
    const float sx = tx * tx * (3 - 2 * tx), sy = ty * ty * (3 - 2 * ty);
    auto wrap = [period](int v) { return period > 0 ? ((v % period) + period) % period : v; };
    const int x0 = wrap(ix), x1 = wrap(ix + 1), y0 = wrap(iy), y1 = wrap(iy + 1);
    const float a = lattice(x0, y0, seed), b = lattice(x1, y0, seed);
    const float c = lattice(x0, y1, seed), d = lattice(x1, y1, seed);
    return mix(mix(a, b, sx), mix(c, d, sx), sy);
}

float fbm(float x, float y, uint32_t seed, int octaves, int period)
{
    float sum = 0, amplitude = 0.5f, total = 0;
    for (int o = 0; o < octaves; o++) {
        sum += value_noise(x, y, seed + static_cast<uint32_t>(o) * 31u, period) * amplitude;
        total += amplitude;
        x *= 2.0f;
        y *= 2.0f;
        if (period > 0) period *= 2;
        amplitude *= 0.5f;
    }
    return sum / total;
}

uint32_t pack_rgba(float r, float g, float b, float a)
{
    auto byte = [](float v) { return static_cast<uint32_t>(clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
    return byte(r) | (byte(g) << 8u) | (byte(b) << 16u) | (byte(a) << 24u);
}

// A tangent-space normal map from a tiling height field.
void normal_map(const float *height, uint32_t size, float strength, uint32_t *texels)
{
    for (uint32_t y = 0; y < size; y++)
        for (uint32_t x = 0; x < size; x++) {
            const float l = height[y * size + (x + size - 1) % size], r = height[y * size + (x + 1) % size];
            const float u = height[((y + size - 1) % size) * size + x], d = height[((y + 1) % size) * size + x];
            const float3 n = normalize({(l - r) * strength, (u - d) * strength, 1.0f});
            texels[y * size + x] = pack_rgba(n.x * 0.5f + 0.5f, n.y * 0.5f + 0.5f, n.z * 0.5f + 0.5f, 1);
        }
}

float3 transform_direction(float3 v, const float4x4 &m)
{
    return {v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0], v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1],
            v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2]};
}

// A rotation taking +y to `d`.
float4x4 basis_from_y(float3 d)
{
    const float3 x = normalize(cross(d, abs(d.x) < 0.9f ? float3{1, 0, 0} : float3{0, 0, 1}));
    const float3 z = cross(x, d);
    float4x4 r = float4x4::identity();
    r.m[0][0] = x.x; r.m[0][1] = x.y; r.m[0][2] = x.z;
    r.m[1][0] = d.x; r.m[1][1] = d.y; r.m[1][2] = d.z;
    r.m[2][0] = z.x; r.m[2][1] = z.y; r.m[2][2] = z.z;
    return r;
}

// A capped frustum around +y from 0 to `height`: a cylinder when both radii match, a cone when top is 0.
Mesh frustum_mesh(uint32_t sides, float bottom, float top, float height)
{
    Mesh mesh;
    const uint32_t vertices = (sides + 1) * 2 + (sides + 2) * 2, indices = sides * 6 + sides * 6;
    mesh.vertices = static_cast<Vertex *>(allocate(sizeof(Vertex) * vertices));
    mesh.indices = static_cast<uint32_t *>(allocate(sizeof(uint32_t) * indices));
    const float slope = (bottom - top) / height;
    for (uint32_t i = 0; i <= sides; i++) {
        const float u = static_cast<float>(i) / static_cast<float>(sides), a = u * 2 * pi;
        const float3 n = normalize({cos(a), slope, sin(a)});
        mesh.vertices[mesh.vertex_count++] = {{cos(a) * bottom, 0, sin(a) * bottom}, n, {u, 0}, {1, 1, 1}};
        mesh.vertices[mesh.vertex_count++] = {{cos(a) * top, height, sin(a) * top}, n, {u, 1}, {1, 1, 1}};
    }
    for (uint32_t i = 0; i < sides; i++) {
        const uint32_t a = i * 2;
        uint32_t *at = mesh.indices + mesh.index_count;
        at[0] = a; at[1] = a + 1; at[2] = a + 3;
        at[3] = a; at[4] = a + 3; at[5] = a + 2;
        mesh.index_count += 6;
    }
    for (int cap = 0; cap < 2; cap++) {
        const float y = cap == 0 ? 0.0f : height, r = cap == 0 ? bottom : top;
        const float3 n = {0, cap == 0 ? -1.0f : 1.0f, 0};
        const uint32_t center = mesh.vertex_count;
        mesh.vertices[mesh.vertex_count++] = {{0, y, 0}, n, {0.5f, 0.5f}, {1, 1, 1}};
        for (uint32_t i = 0; i <= sides; i++) {
            const float a = static_cast<float>(i) / static_cast<float>(sides) * 2 * pi;
            mesh.vertices[mesh.vertex_count++] = {{cos(a) * r, y, sin(a) * r}, n,
                                                  {0.5f + 0.5f * cos(a), 0.5f + 0.5f * sin(a)}, {1, 1, 1}};
        }
        for (uint32_t i = 0; i < sides; i++) {
            uint32_t *at = mesh.indices + mesh.index_count;
            at[0] = center; at[1] = center + 1 + i; at[2] = center + 2 + i;
            mesh.index_count += 3;
        }
    }
    return mesh;
}

// Several meshes baked into one, each placed, coloured, and given a glow
// amount (carried in uv.x) that the bloom and radial-blur passes read.
struct MeshBuilder {
    Mesh     mesh;
    uint32_t vertex_capacity = 0;
    uint32_t index_capacity = 0;

    MeshBuilder(uint32_t vertices, uint32_t indices) : vertex_capacity(vertices), index_capacity(indices)
    {
        mesh.vertices = static_cast<Vertex *>(allocate(sizeof(Vertex) * vertices));
        mesh.indices = static_cast<uint32_t *>(allocate(sizeof(uint32_t) * indices));
    }
    void append(const Mesh &part, float3 size, const float4x4 &rotation, float3 offset, float3 color, float glow)
    {
        if (mesh.vertex_count + part.vertex_count > vertex_capacity ||
            mesh.index_count + part.index_count > index_capacity)
            return;
        const uint32_t base = mesh.vertex_count;
        for (uint32_t i = 0; i < part.vertex_count; i++) {
            const Vertex &v = part.vertices[i];
            const float3 p = {v.position.x * size.x, v.position.y * size.y, v.position.z * size.z};
            const float3 n = {v.normal.x / size.x, v.normal.y / size.y, v.normal.z / size.z};
            mesh.vertices[mesh.vertex_count++] = {transform_direction(p, rotation) + offset,
                                                  normalize(transform_direction(n, rotation)), {glow, 0}, color};
        }
        for (uint32_t i = 0; i < part.index_count; i++) mesh.indices[mesh.index_count++] = base + part.indices[i];
    }
    void push(float3 p, float3 n, float3 color, float glow)
    {
        if (mesh.vertex_count < vertex_capacity) mesh.vertices[mesh.vertex_count++] = {p, n, {glow, 0}, color};
    }
    void triangle(uint32_t a, uint32_t b, uint32_t c)
    {
        if (mesh.index_count + 3 > index_capacity) return;
        mesh.indices[mesh.index_count++] = a;
        mesh.indices[mesh.index_count++] = b;
        mesh.indices[mesh.index_count++] = c;
    }
};

// Lit geometry that also glows: the bloom and radial blur scenes.
const char *const glow_scene_source = R"(
cbuffer Scene : register(b0) {
    row_major float4x4 world;
    row_major float4x4 view_projection;
    float4 light;   // xyz: direction to the light
    float4 eye;     // xyz: camera position
};
struct In { float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD; float3 color : COLOR; };
struct Out {
    float4 position : SV_Position; float3 normal : NORMAL; float3 world_position : WORLD;
    float3 color : COLOR; float glow : GLOW;
};
Out vs(In v) {
    Out o;
    float4 w = mul(float4(v.position, 1), world);
    o.position = mul(w, view_projection);
    o.world_position = w.xyz;
    o.normal = mul(float4(v.normal, 0), world).xyz;
    o.color = v.color;
    o.glow = v.uv.x;
    return o;
}
float4 phong(Out p) : SV_Target {
    float3 n = normalize(p.normal);
    float3 l = normalize(light.xyz);
    float3 v = normalize(eye.xyz - p.world_position);
    float diffuse = saturate(dot(n, l));
    float specular = pow(saturate(dot(reflect(-l, n), v)), 32.0);
    float3 lit = p.color * (0.12 + 0.88 * diffuse) + specular * 0.6;
    return float4(lerp(lit, p.color * (0.6 + 0.4 * diffuse) + specular * 0.3, p.glow), 1);
}
float4 emissive(Out p) : SV_Target { return float4(p.color * p.glow, 1); }
)";

struct GlowScene {
    float4x4 world;
    float4x4 view_projection;
    float4   light;
    float4   eye;
};
// ---------------------------------------------------------------- computeparticles

// A compute shader moves 256K particles between two typed buffers; a geometry
// shader expands each one into a sprite, blended additively.
class ComputeParticles final : public Sample {
public:
    const char *name() const override { return "computeparticles"; }
    bool init(Gpu &gpu) override
    {
        static const char *const sim_source = R"(
// A particle is two texels: (position, velocity) and (gradient position, -, -, -).
cbuffer Sim : register(b0) { float dt; float dest_x; float dest_y; uint count; float force; float3 unused; };
Buffer<float4> before : register(t0);
RWBuffer<float4> after : register(u0);
float2 attraction(float2 pos, float2 target) {
    float2 delta = target - pos;
    float inv = rsqrt(dot(delta, delta) + 0.5);
    return delta * inv * inv * inv * 0.0035;
}
float2 repulsion(float2 pos, float2 target) {
    float2 delta = target - pos;
    float d = sqrt(dot(delta, delta));
    return delta * (1.0 / (d * d * d)) * -0.000035;
}
[numthreads(256, 1, 1)]
void cs(uint3 id : SV_DispatchThreadID) {
    uint i = id.x;
    if (i >= count) return;
    float4 motion = before[i * 2];
    float4 gradient = before[i * 2 + 1];
    float2 pos = motion.xy, vel = motion.zw;
    float2 target = float2(dest_x, dest_y);
    vel += repulsion(pos, target) * 0.05 * force;
    float2 moved = pos + vel * dt;
    if (moved.x < -1.0 || moved.x > 1.0 || moved.y < -1.0 || moved.y > 1.0)
        vel = -vel * 0.1 + attraction(moved, target) * 12.0 * force;
    else
        pos = moved;
    gradient.x += 0.02 * dt;
    if (gradient.x > 1.0) gradient.x -= 1.0;
    after[i * 2] = float4(pos, vel);
    after[i * 2 + 1] = gradient;
}
)";
        static const char *const draw_source = R"(
Buffer<float4> particles : register(t0);
Texture2D ramp : register(t1);
SamplerState ramp_filter : register(s0);
cbuffer Sprite : register(b0) { float2 half_size; float intensity; float unused; };
struct Point { float4 position : SV_Position; float gradient : GRADIENT; };
struct Corner { float4 position : SV_Position; float2 uv : TEXCOORD; float gradient : GRADIENT; };
Point vs(uint id : SV_VertexID) {
    Point o;
    o.position = float4(particles[id * 2].xy, 0.5, 1);
    o.gradient = particles[id * 2 + 1].x;
    return o;
}
[maxvertexcount(4)]
void gs(point Point p[1], inout TriangleStream<Corner> strip) {
    for (uint k = 0; k < 4; k++) {
        Corner c;
        c.uv = float2(k & 1, k >> 1);
        c.position = p[0].position + float4((c.uv * 2 - 1) * half_size, 0, 0);
        c.gradient = p[0].gradient;
        strip.Append(c);
    }
}
float4 ps(Corner c) : SV_Target {
    float glow = saturate(1 - length(c.uv * 2 - 1));
    glow *= glow;
    return float4(ramp.SampleLevel(ramp_filter, float2(c.gradient, 0.5), 0).rgb * glow * intensity, 1);
}
)";
        sim_root_ = gpu.root_signature(RootLayout().constants(0, 8).srv_table(0, 1).uav_table(0, 1).no_input());
        sim_ = gpu.compute(sim_root_, gpu.cs("computeparticles_cs", sim_source));
        draw_root_ = gpu.root_signature(RootLayout()
                                            .constants(0, 4)
                                            .srv_table(0, 1, D3D12_SHADER_VISIBILITY_VERTEX)
                                            .srv_table(1, 1, D3D12_SHADER_VISIBILITY_PIXEL)
                                            .sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                                                     D3D12_SHADER_VISIBILITY_PIXEL)
                                            .no_input());
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc =
            quad_pass(gpu, draw_root_, gpu.vs("computeparticles_vs", draw_source), gpu.ps("computeparticles_ps", draw_source));
        desc.GS = gpu.gs("computeparticles_gs", draw_source);
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        set_blend(desc, Blend::additive);
        draw_ = gpu.pipeline(desc);
        if (gpu.failed) return false;

        Particle *start = static_cast<Particle *>(allocate(sizeof(Particle) * count));
        Random random(0x5EEDu);
        for (uint32_t i = 0; i < count; i++) {
            start[i].pos = {random.range(-1, 1), random.range(-1, 1)};
            start[i].gradient = {start[i].pos.x * 0.5f, 0, 0, 0};
        }
        const uint32_t bytes = count * static_cast<uint32_t>(sizeof(Particle));
        initial_ = gpu.buffer(start, bytes, D3D12_RESOURCE_STATE_COPY_SOURCE);
        D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
        srv.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Buffer.NumElements = count * 2;
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav = {};
        uav.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        uav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uav.Buffer.NumElements = count * 2;
        for (int i = 0; i < 2; i++) {
            particles_[i] = gpu.buffer(start, bytes, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                                       D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
            state_[i] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            if (gpu.failed) break;
            srv_[i] = gpu.views(1);
            uav_[i] = gpu.views(1);
            gpu.device->CreateShaderResourceView(particles_[i], &srv, srv_[i].cpu);
            gpu.device->CreateUnorderedAccessView(particles_[i], nullptr, &uav, uav_[i].cpu);
        }
        release_memory(start);
        if (gpu.failed) return false;

        uint32_t ramp[256];
        const float3 keys[5] = {{1.0f, 0.85f, 0.15f}, {1.0f, 0.2f, 0.75f}, {0.15f, 0.85f, 1.0f},
                                {0.45f, 0.35f, 1.0f}, {1.0f, 0.85f, 0.15f}};
        for (uint32_t i = 0; i < 256; i++) {
            const float t = static_cast<float>(i) / 256.0f * 4.0f;
            const int k = static_cast<int>(t);
            const float f = t - static_cast<float>(k);
            ramp[i] = pack_rgba(mix(keys[k].x, keys[k + 1].x, f), mix(keys[k].y, keys[k + 1].y, f),
                                mix(keys[k].z, keys[k + 1].z, f), 1);
        }
        ramp_ = gpu.texture(ramp, 256, 1);
        steps_ = 0;
        current_ = 0;
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        auto *l = gpu.list;
        // The state at `time` is a fixed number of fixed steps from the start,
        // so a jump back in time replays from the initial particles.
        const uint32_t target = static_cast<uint32_t>((time + preroll) * steps_per_second + 0.001f);
        if (target < steps_) {
            become(gpu, 0, D3D12_RESOURCE_STATE_COPY_DEST);
            l->CopyResource(particles_[0], initial_);
            steps_ = 0;
            current_ = 0;
        }
        l->SetComputeRootSignature(sim_root_);
        l->SetPipelineState(sim_);
        for (; steps_ < target; steps_++) {
            become(gpu, current_, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            become(gpu, 1 - current_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            const float t = static_cast<float>(steps_) / steps_per_second - preroll;
            const SimConstants sim = {2.5f / 60.0f, sin(t * 0.04f * 2 * pi) * 0.75f, 0, count, force, {0, 0, 0}};
            l->SetComputeRoot32BitConstants(0, 8, &sim, 0);
            l->SetComputeRootDescriptorTable(1, srv_[current_].gpu);
            l->SetComputeRootDescriptorTable(2, uav_[1 - current_].gpu);
            l->Dispatch(count / 256, 1, 1);
            current_ = 1 - current_;
        }
        become(gpu, current_, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        const float sprite[4] = {4.0f / static_cast<float>(gpu.width) * 2.0f,
                                 4.0f / static_cast<float>(gpu.height) * 2.0f, 0.2f, 0};
        gpu.begin_frame(0, 0, 0);
        l->SetGraphicsRootSignature(draw_root_);
        l->SetPipelineState(draw_);
        l->SetGraphicsRoot32BitConstants(0, 4, sprite, 0);
        l->SetGraphicsRootDescriptorTable(1, srv_[current_].gpu);
        l->SetGraphicsRootDescriptorTable(2, ramp_.srv.gpu);
        l->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
        l->DrawInstanced(count, 1, 0, 0);
    }
    void release() override
    {
        safe_release(sim_root_);
        safe_release(sim_);
        safe_release(draw_root_);
        safe_release(draw_);
        safe_release(initial_);
        for (ID3D12Resource *&buffer : particles_) safe_release(buffer);
        ramp_.release();
    }

private:
    struct SimConstants {
        float    dt;
        float    dest_x;
        float    dest_y;
        uint32_t count;
        float    force;
        float    unused[3];
    };
    struct Particle {
        float2 pos;
        float2 vel;
        float4 gradient;
    };
    static constexpr uint32_t count = 256u * 1024u;
    // Two steps a frame with the forces scaled up stand in for the original's
    // one step per frame at the ~1000 fps it was shown running at.
    static constexpr float steps_per_second = 120.0f;
    static constexpr float force = 8.0f;
    // Time zero shows the swarm already settled into its streams, as the
    // original looks after running a while; replaying reaches it in one frame.
    static constexpr float preroll = 8.0f;

    void become(Gpu &gpu, uint32_t which, D3D12_RESOURCE_STATES next)
    {
        gpu.barrier(particles_[which], state_[which], next);
        state_[which] = next;
    }

    ID3D12RootSignature  *sim_root_ = nullptr, *draw_root_ = nullptr;
    ID3D12PipelineState  *sim_ = nullptr, *draw_ = nullptr;
    ID3D12Resource       *initial_ = nullptr;
    // Ping-pong, as the D3D11 port: one buffer read, the other written.
    ID3D12Resource       *particles_[2] = {};
    D3D12_RESOURCE_STATES state_[2] = {};
    Descriptors           srv_[2], uav_[2];
    Texture               ramp_;
    uint32_t              steps_ = 0;
    uint32_t              current_ = 0;
};

// ---------------------------------------------------------------- computeshader

// A painting generated on the CPU, filtered by a compute shader into a UAV
// texture, shown beside the original. The filter changes every two seconds.
class ComputeShader final : public Sample {
public:
    const char *name() const override { return "computeshader"; }
    bool init(Gpu &gpu) override
    {
        static const char *const filter_source = R"(
Texture2D<float4> source : register(t0);
RWTexture2D<float4> filtered : register(u0);
float3 texel(int2 p) {
    uint w, h;
    source.GetDimensions(w, h);
    return source.Load(int3(clamp(p, int2(0, 0), int2(w - 1, h - 1)), 0)).rgb;
}
float grey(int2 p) { float3 c = texel(p); return (c.r + c.g + c.b) / 3.0; }
[numthreads(16, 16, 1)]
void sharpen(uint3 id : SV_DispatchThreadID) {
    int2 p = int2(id.xy);
    float3 sum = texel(p) * 9.0;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++)
            if (x != 0 || y != 0) sum -= texel(p + int2(x, y));
    filtered[id.xy] = float4(saturate(sum), 1);
}
[numthreads(16, 16, 1)]
void edgedetect(uint3 id : SV_DispatchThreadID) {
    int2 p = int2(id.xy);
    float sum = grey(p);
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++)
            if (x != 0 || y != 0) sum -= grey(p + int2(x, y)) / 8.0;
    filtered[id.xy] = float4(saturate(sum / 0.1).xxx, 1);
}
[numthreads(16, 16, 1)]
void emboss(uint3 id : SV_DispatchThreadID) {
    int2 p = int2(id.xy);
    float sum = -grey(p + int2(-1, -1)) - grey(p) + 2.0 * grey(p + int2(1, 1));
    filtered[id.xy] = float4(saturate(sum + 0.5).xxx, 1);
}
)";
        static const char *const show_source = COMPUTE_QUAD_HLSL R"(
Texture2D image : register(t0);
SamplerState filter : register(s0);
float4 ps(Quad q) : SV_Target { return image.Sample(filter, q.uv); }
)";
        filter_root_ = gpu.root_signature(RootLayout().srv_table(0, 1).uav_table(0, 1).no_input());
        filters_[0] = gpu.compute(filter_root_, gpu.cs("computeshader_sharpen_cs", filter_source, "sharpen"));
        filters_[1] = gpu.compute(filter_root_, gpu.cs("computeshader_edgedetect_cs", filter_source, "edgedetect"));
        filters_[2] = gpu.compute(filter_root_, gpu.cs("computeshader_emboss_cs", filter_source, "emboss"));
        show_root_ = gpu.root_signature(RootLayout().srv_table(0, 1, D3D12_SHADER_VISIBILITY_PIXEL)
                                            .sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                                                     D3D12_SHADER_VISIBILITY_PIXEL));
        D3D12_GRAPHICS_PIPELINE_STATE_DESC show =
            pass(gpu, show_root_, gpu.vs("computeshader_quad_vs", quad_source), gpu.ps("computeshader_ps", show_source));
        show.InputLayout = {nullptr, 0};
        show_ = gpu.pipeline(show);
        if (gpu.failed) return false;
        uint32_t *texels = static_cast<uint32_t *>(allocate(size * size * 4));
        paint(texels);
        image_ = gpu.texture(texels, size, size);
        release_memory(texels);
        filtered_.create(gpu, size, size, DXGI_FORMAT_R8G8B8A8_UNORM, true, false);
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        auto *l = gpu.list;
        const uint32_t which = static_cast<uint32_t>(time / 2.0f) % 3u;
        filtered_.become(gpu, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        l->SetComputeRootSignature(filter_root_);
        l->SetPipelineState(filters_[which]);
        l->SetComputeRootDescriptorTable(0, image_.srv.gpu);
        l->SetComputeRootDescriptorTable(1, filtered_.uav.gpu);
        l->Dispatch(size / 16, size / 16, 1);
        filtered_.become(gpu, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        gpu.begin_frame(0.03f, 0.03f, 0.03f);
        l->SetGraphicsRootSignature(show_root_);
        l->SetPipelineState(show_);
        const float w = static_cast<float>(gpu.width), h = static_cast<float>(gpu.height);
        const float side = min(h * 0.86f, w * 0.48f);
        const D3D12_GPU_DESCRIPTOR_HANDLE views[2] = {image_.srv.gpu, filtered_.srv.gpu};
        for (int i = 0; i < 2; i++) {
            gpu.viewport(w * (i == 0 ? 0.25f : 0.75f) - side * 0.5f, (h - side) * 0.5f, side, side);
            l->SetGraphicsRootDescriptorTable(0, views[i]);
            draw_quad(l);
        }
    }
    void release() override
    {
        safe_release(filter_root_);
        for (auto *&filter : filters_) safe_release(filter);
        safe_release(show_root_);
        safe_release(show_);
        image_.release();
        filtered_.release();
    }

private:
    static constexpr uint32_t size = 512;

    // Sky, clouds and smoke over a brown sea, and a tall ship: the original's
    // painting, in spirit.
    static void paint(uint32_t *texels)
    {
        for (uint32_t y = 0; y < size; y++)
            for (uint32_t x = 0; x < size; x++) {
                const float u = (static_cast<float>(x) + 0.5f) / size, v = (static_cast<float>(y) + 0.5f) / size;
                float3 c = sky(u, v);
                c = ship(u, v, c);
                texels[y * size + x] = pack_rgba(c.x, c.y, c.z, 1);
            }
    }
    static float3 sky(float u, float v)
    {
        const float horizon = 0.82f;
        if (v > horizon) {
            const float streak = fbm(u * 3.0f, v * 60.0f, 5u, 4, 0);
            const float3 sea = {0.46f, 0.38f, 0.29f};
            return sea * (0.75f + 0.45f * streak) * (1.0f - (v - horizon) * 1.2f);
        }
        const float3 top = {0.60f, 0.70f, 0.82f}, low = {0.82f, 0.82f, 0.78f};
        float3 c = top * (1 - v / horizon) + low * (v / horizon);
        const float n = fbm(u * 5.0f, v * 7.0f, 11u, 5, 0);
        const float cloud = smooth(0.42f, 0.7f, n + v * 0.25f - 0.1f);
        const float shade = 0.45f + 0.5f * fbm(u * 9.0f + 3.0f, v * 9.0f, 23u, 4, 0);
        c = c * (1 - cloud) + float3{shade, shade, shade * 1.02f} * cloud;
        // gun smoke low beside the hull
        const float dx = (u - 0.45f) / 0.18f, dy = (v - 0.72f) / 0.1f;
        const float puff = smooth(1.0f, 0.3f, dx * dx + dy * dy) * smooth(0.3f, 0.6f, fbm(u * 12, v * 12, 41u, 4, 0));
        const float white = 0.85f + 0.12f * fbm(u * 20, v * 20, 43u, 3, 0);
        return c * (1 - puff) + float3{white, white, white * 1.03f} * puff;
    }
    static float3 ship(float u, float v, float3 c)
    {
        const float grain = fbm(u * 40.0f, v * 40.0f, 77u, 3, 0);
        const float masts[3] = {0.57f, 0.68f, 0.78f};
        for (int m = 0; m < 3; m++) {
            if (abs(u - masts[m]) < 0.004f && v > 0.18f + m * 0.03f && v < 0.82f) c = {0.25f, 0.18f, 0.1f};
            for (int s = 0; s < 3; s++) {
                const float v0 = 0.24f + s * 0.16f + m * 0.03f, v1 = v0 + 0.13f;
                if (v < v0 || v > v1) continue;
                const float f = (v - v0) / (v1 - v0);
                const float half = 0.035f + 0.025f * s + 0.012f * f + 0.01f * sin(f * pi);
                if (abs(u - masts[m] + 0.01f) < half) {
                    const float light = 0.72f + 0.2f * sin(f * pi) + 0.1f * grain - 0.12f * (u - masts[m]) / half;
                    c = float3{0.9f, 0.84f, 0.74f} * light;
                }
            }
        }
        const float hx = (u - 0.68f) / 0.2f, hy = (v - 0.8f) / 0.075f;
        if (hy > -0.35f && hx * hx + hy * hy < 1.0f) c = float3{0.34f, 0.2f, 0.1f} * (0.6f + 0.5f * grain);
        if (hy > -0.45f && hy < -0.35f && abs(hx) < 0.9f) c = {0.55f, 0.32f, 0.14f};
        return c;
    }

    ID3D12RootSignature *filter_root_ = nullptr, *show_root_ = nullptr;
    ID3D12PipelineState *filters_[3] = {};
    ID3D12PipelineState *show_ = nullptr;
    Texture              image_;
    RenderTexture        filtered_;
};

// ---------------------------------------------------------------- particlefire

// A camp fire: normal-mapped ground and logs lit by the flickering fire, and
// flame and smoke billboards whose every property is a closed-form function
// of time and the instance id, expanded from SV_VertexID in the vertex shader.
class ParticleFire final : public Sample {
public:
    const char *name() const override { return "particlefire"; }
    bool init(Gpu &gpu) override
    {
        static const char *const scene_source = R"(
cbuffer Object : register(b0) {
    row_major float4x4 world;
    row_major float4x4 view_projection;
    float4 light;    // xyz: position, w: intensity
    float4 v_axis;   // world direction the texture's v grows along; w: 1 for the ground
    float4 tint;
};
Texture2D albedo : register(t0);
Texture2D normals : register(t1);
SamplerState filter : register(s0);
struct In { float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD; float3 color : COLOR; };
struct Out { float4 position : SV_Position; float3 world_position : WORLD; float3 normal : NORMAL; float2 uv : TEXCOORD; };
Out vs(In v) {
    Out o;
    float4 w = mul(float4(v.position, 1), world);
    o.position = mul(w, view_projection);
    o.world_position = w.xyz;
    o.normal = mul(float4(v.normal, 0), world).xyz;
    o.uv = v.uv;
    return o;
}
float4 ps(Out p) : SV_Target {
    float3 N = normalize(p.normal);
    float3 B = v_axis.xyz - N * dot(v_axis.xyz, N);
    B = dot(B, B) > 1e-4 ? normalize(B) : float3(1, 0, 0);
    float3 T = cross(B, N);
    float3 t = normals.Sample(filter, p.uv).xyz * 2 - 1;
    float3 n = normalize(T * t.x + B * t.y + N * t.z);
    float3 base = albedo.Sample(filter, p.uv).rgb * tint.rgb;
    float r = length(p.world_position.xz);
    float ash = smoothstep(1.35, 0.95, r) * v_axis.w;
    base = lerp(base, float3(0.22, 0.14, 0.08) * (0.5 + base.g * 2.0), ash);
    float3 L = light.xyz - p.world_position;
    float d = length(L);
    L /= d;
    float attenuation = light.w / (1.0 + 0.9 * d * d);
    float3 fire = float3(1.0, 0.62, 0.3);
    return float4(base * fire * saturate(dot(n, L)) * attenuation + base * 0.008, 1);
}
)";
        static const char *const particle_source = R"(
cbuffer Particles : register(b0) {
    row_major float4x4 view;
    row_major float4x4 projection;
    float time; uint kind; float2 unused;   // kind 0: flame, 1: smoke
};
Texture2D sprite : register(t0);
SamplerState filter : register(s0);
float hash(uint n) {
    n = (n << 13) ^ n;
    n = n * (n * n * 15731u + 789221u) + 1376312589u;
    return float(n & 0x7fffffffu) / 2147483648.0;
}
struct Out { float4 position : SV_Position; float2 uv : TEXCOORD; float alpha : ALPHA; float3 color : COLOR; };
Out vs(uint vertex : SV_VertexID, uint instance : SV_InstanceID) {
    uint seed = instance * 7u + kind * 104729u;
    float h0 = hash(seed), h1 = hash(seed + 1u), h2 = hash(seed + 2u), h3 = hash(seed + 3u), h4 = hash(seed + 4u);
    float angle = h3 * 6.2831853;
    float3 pos; float size; float alpha; float3 color; float spin;
    if (kind == 0) {
        float life = 0.7 + 0.6 * h0;
        float age = frac(time / life + h1);
        float r = 0.5 * sqrt(h2) * (1.0 - age * 0.75);
        pos = float3(cos(angle) * r, 0.12 + age * life * (1.0 + 0.7 * h4), sin(angle) * r);
        size = (0.38 + 0.26 * h4) * (1.0 - 0.55 * age);
        alpha = sin(age * 3.14159) * 0.55;
        color = lerp(float3(1.0, 0.8, 0.45), float3(1.0, 0.3, 0.06), age);
        spin = h0 * 6.28 + age * (h4 - 0.5) * 4.0;
    } else {
        float life = 3.5 + 1.5 * h0;
        float age = frac(time / life + h1);
        float r = 0.2 * sqrt(h2) + age * 0.35;
        pos = float3(cos(angle) * r + sin(time * 0.5 + h4 * 6.0) * 0.25 * age, 1.1 + age * life * 0.65, sin(angle) * r);
        size = 0.45 + 1.1 * age;
        alpha = 0.4 * smoothstep(0.0, 0.2, age) * (1.0 - age);
        color = (0.3 + 0.15 * h4).xxx;
        spin = h0 * 6.28 + age * (h4 - 0.5) * 3.0;
    }
    float2 corner = float2(vertex & 1, vertex >> 1);
    float4 center = mul(float4(pos, 1), view);
    center.xy += (corner * 2 - 1) * size;
    Out o;
    o.position = mul(center, projection);
    float s = sin(spin), c = cos(spin);
    float2 d = corner - 0.5;
    o.uv = float2(c * d.x - s * d.y, s * d.x + c * d.y) + 0.5;
    o.alpha = alpha;
    o.color = color;
    return o;
}
float4 flame(Out p) : SV_Target {
    float4 t = sprite.Sample(filter, p.uv);
    return float4(t.rgb * p.color * p.alpha, 0);
}
float4 smoke(Out p) : SV_Target {
    float4 t = sprite.Sample(filter, p.uv);
    return float4(t.rgb * p.color, t.a * p.alpha);
}
)";
        scene_root_ = gpu.root_signature(RootLayout().cbv(0).srv_table(0, 2, D3D12_SHADER_VISIBILITY_PIXEL)
                                             .sampler(0, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                                                      D3D12_SHADER_VISIBILITY_PIXEL));
        scene_ = gpu.pipeline(pass(gpu, scene_root_, gpu.vs("particlefire_scene_vs", scene_source),
                                   gpu.ps("particlefire_scene_ps", scene_source)));
        particle_root_ = gpu.root_signature(RootLayout().cbv(0).srv_table(0, 1, D3D12_SHADER_VISIBILITY_PIXEL)
                                                .sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                                                         D3D12_SHADER_VISIBILITY_PIXEL));
        D3D12_GRAPHICS_PIPELINE_STATE_DESC particles =
            pass(gpu, particle_root_, gpu.vs("particlefire_particle_vs", particle_source),
                 gpu.ps("particlefire_smoke_ps", particle_source, "smoke"));
        particles.InputLayout = {nullptr, 0};
        set_depth(particles, true, false);
        set_blend(particles, Blend::alpha);
        smoke_pipeline_ = gpu.pipeline(particles);
        particles.PS = gpu.ps("particlefire_flame_ps", particle_source, "flame");
        set_blend(particles, Blend::additive);
        flame_pipeline_ = gpu.pipeline(particles);
        if (gpu.failed) return false;

        Mesh ground = plane_mesh(16.0f, 1, 6.0f);
        ground_ = gpu.mesh(ground);
        ground.release();
        Mesh log = frustum_mesh(24, 1.0f, 1.0f, 1.0f);
        log_ = gpu.mesh(log);
        log.release();

        make_textures(gpu);
        const Texture *ground_maps[2] = {&grass_, &grass_normals_};
        ground_table_ = gpu.table(ground_maps, 2);
        const Texture *log_maps[2] = {&bark_, &bark_normals_};
        log_table_ = gpu.table(log_maps, 2);
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        auto *l = gpu.list;
        const float orbit = 0.25f + time * 0.08f;
        const float3 eye = {sin(orbit) * 5.8f, 2.5f, -cos(orbit) * 5.8f};
        const float4x4 view = look_at(eye, {0, 0.9f, 0}, {0, 1, 0});
        const float4x4 projection = perspective(radians(60), gpu.aspect(), 0.1f, 100);
        const float flicker = 2.4f + 0.35f * sin(time * 17.0f) + 0.2f * sin(time * 29.3f + 1.0f) +
                              0.15f * sin(time * 7.1f);

        gpu.begin_frame(0, 0, 0);
        l->SetGraphicsRootSignature(scene_root_);
        l->SetPipelineState(scene_);

        Object object = {};
        object.view_projection = view * projection;
        object.light = {0, 0.7f, 0, flicker};
        object.world = float4x4::identity();
        object.v_axis = {0, 0, 1, 1};
        object.tint = {1, 1, 1, 1};
        l->SetGraphicsRootConstantBufferView(0, gpu.constants(&object, sizeof object));
        l->SetGraphicsRootDescriptorTable(1, ground_table_.gpu);
        ground_.draw(l);

        l->SetGraphicsRootDescriptorTable(1, log_table_.gpu);
        for (int i = 0; i < 3; i++) {
            const float4x4 rotation = rotate_z(-pi * 0.5f + 0.22f) * rotate_y(radians(40.0f + 120.0f * i));
            object.world = scale({0.16f, 1.5f, 0.16f}) * translate({0, -0.35f, 0}) * rotation *
                           translate({0, 0.2f, 0});
            const float3 axis = transform_direction({0, 1, 0}, rotation);
            object.v_axis = {axis.x, axis.y, axis.z, 0};
            object.tint = {1, 1, 1, 1};
            l->SetGraphicsRootConstantBufferView(0, gpu.constants(&object, sizeof object));
            log_.draw(l);
        }

        Particles particles = {};
        particles.view = view;
        particles.projection = projection;
        particles.time = time;
        l->SetGraphicsRootSignature(particle_root_);
        l->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        particles.kind = 1;
        l->SetGraphicsRootConstantBufferView(0, gpu.constants(&particles, sizeof particles));
        l->SetPipelineState(smoke_pipeline_);
        l->SetGraphicsRootDescriptorTable(1, smoke_.srv.gpu);
        l->DrawInstanced(4, smoke_count, 0, 0);

        particles.kind = 0;
        l->SetGraphicsRootConstantBufferView(0, gpu.constants(&particles, sizeof particles));
        l->SetPipelineState(flame_pipeline_);
        l->SetGraphicsRootDescriptorTable(1, flame_.srv.gpu);
        l->DrawInstanced(4, flame_count, 0, 0);
    }
    void release() override
    {
        safe_release(scene_root_);
        safe_release(particle_root_);
        safe_release(scene_);
        safe_release(smoke_pipeline_);
        safe_release(flame_pipeline_);
        ground_.release();
        log_.release();
        grass_.release();
        grass_normals_.release();
        bark_.release();
        bark_normals_.release();
        flame_.release();
        smoke_.release();
    }

private:
    struct Object {
        float4x4 world;
        float4x4 view_projection;
        float4   light;
        float4   v_axis;
        float4   tint;
    };
    struct Particles {
        float4x4 view;
        float4x4 projection;
        float    time;
        uint32_t kind;
        float    unused[2];
    };
    static constexpr uint32_t flame_count = 320;
    static constexpr uint32_t smoke_count = 140;

    void make_textures(Gpu &gpu)
    {
        const uint32_t n = 256;
        float    *height = static_cast<float *>(allocate(n * n * sizeof(float)));
        uint32_t *texels = static_cast<uint32_t *>(allocate(n * n * 4));
        for (uint32_t y = 0; y < n; y++)
            for (uint32_t x = 0; x < n; x++) {
                const float u = static_cast<float>(x) / n, v = static_cast<float>(y) / n;
                const float blades = fbm(u * 32, v * 32, 3u, 3, 32);
                const float patch = fbm(u * 4, v * 4, 9u, 3, 4);
                height[y * n + x] = blades;
                const float3 green = float3{0.22f, 0.36f, 0.08f} * (0.5f + 0.8f * blades);
                const float3 soil = float3{0.3f, 0.24f, 0.12f} * (0.6f + 0.6f * blades);
                const float m = smooth(0.35f, 0.65f, patch);
                const float3 c = green * m + soil * (1 - m);
                texels[y * n + x] = pack_rgba(c.x, c.y, c.z, 1);
            }
        grass_ = gpu.texture(texels, n, n);
        normal_map(height, n, 6.0f, texels);
        grass_normals_ = gpu.texture(texels, n, n);
        for (uint32_t y = 0; y < n; y++)
            for (uint32_t x = 0; x < n; x++) {
                const float u = static_cast<float>(x) / n, v = static_cast<float>(y) / n;
                const float grain = fbm(u * 24, v * 3, 17u, 4, 0) * 0.7f + fbm(u * 64, v * 8, 19u, 2, 0) * 0.3f;
                height[y * n + x] = grain;
                const float3 c = float3{0.36f, 0.24f, 0.14f} * (0.35f + 0.9f * grain);
                texels[y * n + x] = pack_rgba(c.x, c.y, c.z, 1);
            }
        bark_ = gpu.texture(texels, n, n);
        normal_map(height, n, 8.0f, texels);
        bark_normals_ = gpu.texture(texels, n, n);
        const uint32_t s = 128;
        for (int kind = 0; kind < 2; kind++) {
            for (uint32_t y = 0; y < s; y++)
                for (uint32_t x = 0; x < s; x++) {
                    const float u = (static_cast<float>(x) + 0.5f) / s, v = (static_cast<float>(y) + 0.5f) / s;
                    const float dx = u * 2 - 1, dy = v * 2 - 1;
                    const float r = sqrt(dx * dx + dy * dy);
                    const float noise = fbm(u * 6, v * 6, kind == 0 ? 51u : 53u, 4, 0);
                    float f = clamp(1.0f - r, 0.0f, 1.0f);
                    if (kind == 0) {
                        f = f * f * (0.55f + 0.7f * noise);
                        texels[y * s + x] = pack_rgba(f, f * 0.9f, f * 0.75f, f);
                    } else {
                        f = f * sqrt(f) * smooth(0.25f, 0.75f, noise);
                        texels[y * s + x] = pack_rgba(0.8f, 0.8f, 0.8f, f);
                    }
                }
            (kind == 0 ? flame_ : smoke_) = gpu.texture(texels, s, s);
        }
        release_memory(height);
        release_memory(texels);
    }

    ID3D12RootSignature *scene_root_ = nullptr, *particle_root_ = nullptr;
    ID3D12PipelineState *scene_ = nullptr, *smoke_pipeline_ = nullptr, *flame_pipeline_ = nullptr;
    DrawMesh             ground_;
    DrawMesh             log_;
    Texture              grass_, grass_normals_, bark_, bark_normals_, flame_, smoke_;
    Descriptors          ground_table_, log_table_;
};

// ---------------------------------------------------------------- raytracing

// A compute shader ray traces three spheres over a plane (shadows, one
// reflection bounce, fog) into a UAV texture that a full-screen pass shows.
class Raytracing final : public Sample {
public:
    const char *name() const override { return "raytracing"; }
    bool init(Gpu &gpu) override
    {
        static const char *const trace_source = R"(
RWTexture2D<float4> result : register(u0);
cbuffer Scene : register(b0) { float3 light_position; float aspect; float3 camera; float unused; uint2 size; uint2 unused2; };
static const float3 centers[3] = { float3(-2.25, 1.0, 0.0), float3(0.0, 2.5, 0.0), float3(2.25, 1.0, 0.0) };
static const float3 colors[3] = { float3(1, 0, 0), float3(0, 0, 1), float3(0, 1, 0) };
float sphere_hit(float3 o, float3 d, float3 center) {
    float3 oc = o - center;
    float b = dot(oc, d);
    float h = b * b - (dot(oc, oc) - 1.0);
    return h < 0.0 ? -1.0 : -b - sqrt(h);
}
int intersect(float3 o, float3 d, out float t) {
    int id = -1;
    t = 1000.0;
    for (int i = 0; i < 3; i++) {
        float s = sphere_hit(o, d, centers[i]);
        if (s > 0.0001 && s < t) { t = s; id = i; }
    }
    float p = -o.y / d.y;
    if (p > 0.0001 && p < t) { t = p; id = 3; }
    return id;
}
float shadow(float3 o, float3 d) {
    for (int i = 0; i < 3; i++)
        if (sphere_hit(o, d, centers[i]) > 0.0001) return 0.5;
    return 1.0;
}
float3 shade(inout float3 o, inout float3 d) {
    float t;
    int id = intersect(o, d, t);
    if (id < 0) return float3(0, 0, 0);
    float3 p = o + t * d;
    float3 l = normalize(light_position - p);
    float3 n = float3(0, 1, 0);
    float3 color;
    if (id == 3) {
        color = saturate(dot(n, l)).xxx;
    } else {
        n = p - centers[id];
        float diffuse = saturate(dot(n, l));
        float specular = pow(saturate(dot(n, normalize(l + normalize(camera)))), 32.0);
        color = diffuse * colors[id] + specular * 2.0;
    }
    color *= shadow(p, l);
    color = lerp(color, float3(0, 0, 0), saturate(t / 20.0));
    d = reflect(d, n);
    o = p;
    return color;
}
[numthreads(16, 16, 1)]
void cs(uint3 id : SV_DispatchThreadID) {
    if (id.x >= size.x || id.y >= size.y) return;
    float2 uv = (float2(id.xy) + 0.5) / float2(size);
    float3 o = camera;
    float3 d = normalize(float3((uv.x * 2 - 1) * aspect, 1 - uv.y * 2, -1));
    float3 color = shade(o, d);
    float3 reflected = shade(o, d);
    color = 0.6 * color + 0.4 * lerp(reflected, color, 0.6);
    result[id.xy] = float4(color, 1);
}
)";
        static const char *const show_source = COMPUTE_QUAD_HLSL R"(
Texture2D image : register(t0);
float4 ps(Quad q) : SV_Target { return image.Load(int3(q.position.xy, 0)); }
)";
        trace_root_ = gpu.root_signature(RootLayout().cbv(0).uav_table(0, 1).no_input());
        trace_ = gpu.compute(trace_root_, gpu.cs("raytracing_cs", trace_source));
        show_root_ = gpu.root_signature(RootLayout().srv_table(0, 1, D3D12_SHADER_VISIBILITY_PIXEL));
        show_ = gpu.pipeline(quad_pass(gpu, show_root_, gpu.vs("raytracing_quad_vs", quad_source),
                                       gpu.ps("raytracing_ps", show_source)));
        image_.create(gpu, gpu.width, gpu.height, DXGI_FORMAT_R8G8B8A8_UNORM, true, false);
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        auto *l = gpu.list;
        const float a = time / 8.0f * 2 * pi;
        const TraceConstants scene = {{sin(a) * 2.0f, 5.0f, cos(a) * 2.0f}, gpu.aspect(), {0, 1.5f, 4.0f}, 0,
                                      {gpu.width, gpu.height, 0, 0}};
        image_.become(gpu, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        l->SetComputeRootSignature(trace_root_);
        l->SetPipelineState(trace_);
        l->SetComputeRootConstantBufferView(0, gpu.constants(&scene, sizeof scene));
        l->SetComputeRootDescriptorTable(1, image_.uav.gpu);
        l->Dispatch((gpu.width + 15) / 16, (gpu.height + 15) / 16, 1);
        image_.become(gpu, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        gpu.begin_frame(0, 0, 0);
        l->SetGraphicsRootSignature(show_root_);
        l->SetPipelineState(show_);
        l->SetGraphicsRootDescriptorTable(0, image_.srv.gpu);
        draw_quad(l);
    }
    void release() override
    {
        safe_release(trace_root_);
        safe_release(trace_);
        safe_release(show_root_);
        safe_release(show_);
        image_.release();
    }

private:
    struct TraceConstants {
        float3   light;
        float    aspect;
        float3   camera;
        float    unused;
        uint32_t size[4];
    };
    ID3D12RootSignature *trace_root_ = nullptr, *show_root_ = nullptr;
    ID3D12PipelineState *trace_ = nullptr, *show_ = nullptr;
    RenderTexture        image_;
};

// ---------------------------------------------------------------- bloom

const char *const blur_source = COMPUTE_QUAD_HLSL R"(
Texture2D image : register(t0);
SamplerState filter : register(s0);
cbuffer Blur : register(b0) { float2 texel_step; float strength; float unused; };
float4 ps(Quad q) : SV_Target {
    const float weight[5] = { 0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216 };
    float3 sum = image.Sample(filter, q.uv).rgb * weight[0];
    for (int i = 1; i < 5; i++) {
        sum += image.Sample(filter, q.uv + texel_step * i).rgb * weight[i] * strength;
        sum += image.Sample(filter, q.uv - texel_step * i).rgb * weight[i] * strength;
    }
    return float4(sum, 1);
}
)";

// The root signature every glowing-scene pass shares: constants at b0, one
// texture at t0 with a clamping linear sampler.
ID3D12RootSignature *glow_root(Gpu &gpu)
{
    return gpu.root_signature(RootLayout().cbv(0).srv_table(0, 1, D3D12_SHADER_VISIBILITY_PIXEL)
                                  .sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                                           D3D12_SHADER_VISIBILITY_PIXEL));
}

// A flying saucer whose glowing parts are drawn again into a small target,
// blurred vertically there, then horizontally onto the frame, additively.
class Bloom final : public Sample {
public:
    const char *name() const override { return "bloom"; }
    bool init(Gpu &gpu) override
    {
        static const char *const sky_source = COMPUTE_QUAD_HLSL R"(
cbuffer Sky : register(b0) { float4 right; float4 up; float4 forward; };
float hash(float3 p) {
    p = frac(p * 0.3183099 + 0.1);
    p *= 17.0;
    return frac(p.x * p.y * p.z * (p.x + p.y + p.z));
}
float4 ps(Quad q) : SV_Target {
    float2 ndc = float2(q.uv.x * 2 - 1, 1 - q.uv.y * 2);
    float3 d = normalize(forward.xyz + ndc.x * right.xyz + ndc.y * up.xyz);
    float3 cell = floor(d * 260.0);
    float star = step(0.9965, hash(cell)) * (0.3 + 0.7 * hash(cell + 7.0));
    float y = abs(d.y + 0.05);
    float3 glow = float3(0.35, 0.55, 1.0) * (exp(-y * 5.0) * 0.35 + exp(-y * 30.0) * 0.6) + exp(-y * 220.0) * 0.9;
    return float4(glow + star * 0.6, 1);
}
)";
        root_ = glow_root(gpu);
        const D3D12_SHADER_BYTECODE scene_vs = gpu.vs("bloom_scene_vs", glow_scene_source);
        const D3D12_SHADER_BYTECODE quad_vs = gpu.vs("bloom_quad_vs", quad_source);
        const D3D12_SHADER_BYTECODE blur_ps = gpu.ps("bloom_blur_ps", blur_source);
        glow_pipeline_ = gpu.pipeline(pass(gpu, root_, scene_vs, gpu.ps("bloom_glow_ps", glow_scene_source, "emissive"),
                                           glow_format));
        blur_pipeline_ = gpu.pipeline(quad_pass(gpu, root_, quad_vs, blur_ps, glow_format, false));
        sky_pipeline_ = gpu.pipeline(quad_pass(gpu, root_, quad_vs, gpu.ps("bloom_sky_ps", sky_source)));
        phong_pipeline_ = gpu.pipeline(pass(gpu, root_, scene_vs, gpu.ps("bloom_phong_ps", glow_scene_source, "phong")));
        D3D12_GRAPHICS_PIPELINE_STATE_DESC add = quad_pass(gpu, root_, quad_vs, blur_ps);
        set_blend(add, Blend::additive);
        add_pipeline_ = gpu.pipeline(add);
        if (gpu.failed) return false;
        build_saucer(gpu);
        glow_.create(gpu, glow_size, glow_size, glow_format, false, true);
        blurred_.create(gpu, glow_size, glow_size, glow_format, false, false);
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        auto *l = gpu.list;
        const float spin = time / 8.0f * 2 * pi;
        const float3 eye = {0, 0.9f, -8.5f}, at = {0, -0.4f, 0};
        const float4x4 view = look_at(eye, at, {0, 1, 0});
        const float fov = radians(60);
        GlowScene scene = {};
        scene.world = rotate_y(spin) * rotate_x(-sin(spin) * 0.15f) *
                      translate({sin(spin) * 0.25f, 0, cos(spin) * 0.25f});
        scene.view_projection = view * perspective(fov, gpu.aspect(), 0.1f, 100);
        scene.light = {0.4f, 1.0f, -0.6f, 0};
        scene.eye = {eye.x, eye.y, eye.z, 1};
        l->SetGraphicsRootSignature(root_);
        const D3D12_GPU_VIRTUAL_ADDRESS scene_constants = gpu.constants(&scene, sizeof scene);

        glow_.begin(gpu, 0, 0, 0);
        l->SetPipelineState(glow_pipeline_);
        l->SetGraphicsRootConstantBufferView(0, scene_constants);
        saucer_.draw(l);
        glow_.become(gpu, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        const float vertical[4] = {0, 1.0f / glow_size, 1.5f, 0};
        blurred_.begin(gpu, 0, 0, 0);
        l->SetPipelineState(blur_pipeline_);
        l->SetGraphicsRootConstantBufferView(0, gpu.constants(vertical, sizeof vertical));
        l->SetGraphicsRootDescriptorTable(1, glow_.srv.gpu);
        draw_quad(l);
        blurred_.become(gpu, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        gpu.begin_frame(0, 0, 0);
        const float t = tan(fov * 0.5f);
        const float3 forward = normalize(at - eye), right = normalize(cross({0, 1, 0}, forward));
        const float3 up = cross(forward, right);
        const float sky[12] = {right.x * t * gpu.aspect(), right.y * t * gpu.aspect(), right.z * t * gpu.aspect(), 0,
                               up.x * t, up.y * t, up.z * t, 0, forward.x, forward.y, forward.z, 0};
        l->SetPipelineState(sky_pipeline_);
        l->SetGraphicsRootConstantBufferView(0, gpu.constants(sky, sizeof sky));
        draw_quad(l);
        l->SetPipelineState(phong_pipeline_);
        l->SetGraphicsRootConstantBufferView(0, scene_constants);
        saucer_.draw(l);

        const float horizontal[4] = {1.0f / glow_size, 0, 1.5f, 0};
        l->SetPipelineState(add_pipeline_);
        l->SetGraphicsRootConstantBufferView(0, gpu.constants(horizontal, sizeof horizontal));
        l->SetGraphicsRootDescriptorTable(1, blurred_.srv.gpu);
        draw_quad(l);
    }
    void release() override
    {
        safe_release(root_);
        safe_release(glow_pipeline_);
        safe_release(blur_pipeline_);
        safe_release(sky_pipeline_);
        safe_release(phong_pipeline_);
        safe_release(add_pipeline_);
        saucer_.release();
        glow_.release();
        blurred_.release();
    }

private:
    static constexpr uint32_t    glow_size = 256;
    static constexpr DXGI_FORMAT glow_format = DXGI_FORMAT_R16G16B16A16_FLOAT;

    void build_saucer(Gpu &gpu)
    {
        Mesh sphere = sphere_mesh(1.0f, 48, 24);
        Mesh ring = torus_mesh(1.0f, 0.03f, 64, 8);
        Mesh rod = frustum_mesh(12, 1.0f, 1.0f, 1.0f);
        Mesh cone = frustum_mesh(32, 0.3f, 1.0f, 1.0f);
        MeshBuilder builder(40000, 200000);
        const float4x4 none = float4x4::identity();
        const float3 hull = {0.32f, 0.4f, 0.05f}, cyan = {0.1f, 1.0f, 1.0f}, yellow = {1.0f, 0.9f, 0.1f};
        builder.append(sphere, {2.6f, 0.34f, 2.6f}, none, {0, 0, 0}, hull, 0);
        builder.append(sphere, {2.0f, 0.3f, 2.0f}, none, {0, 0.12f, 0}, {0.4f, 0.48f, 0.08f}, 0);
        builder.append(ring, {2.55f, 2.55f, 2.55f}, none, {0, 0.02f, 0}, yellow, 0.35f);
        builder.append(ring, {1.55f, 1.55f, 1.55f}, none, {0, 0.3f, 0}, yellow, 0.35f);
        builder.append(sphere, {1.05f, 0.85f, 1.05f}, none, {0, 0.25f, 0}, cyan, 1.0f);
        for (int i = 0; i < 6; i++) {
            const float a = static_cast<float>(i) / 6.0f * 2 * pi + 0.5f;
            builder.append(sphere, {0.36f, 0.3f, 0.36f}, none, {cos(a) * 1.75f, 0.22f, sin(a) * 1.75f}, cyan, 1.0f);
        }
        for (int i = 0; i < 10; i++) {
            const float a = static_cast<float>(i) / 10.0f * 2 * pi;
            builder.append(sphere, {0.14f, 0.1f, 0.14f}, none, {cos(a) * 1.5f, -0.24f, sin(a) * 1.5f},
                           {1.0f, 0.6f, 0.1f}, 1.0f);
        }
        builder.append(rod, {0.04f, 0.6f, 0.04f}, none, {0, 0.95f, 0}, hull, 0);
        builder.append(sphere, {0.14f, 0.14f, 0.14f}, none, {0, 1.6f, 0}, {1.0f, 0.1f, 0.05f}, 1.0f);
        for (int i = 0; i < 3; i++) {
            const float a = static_cast<float>(i) / 3.0f * 2 * pi + 0.3f;
            const float3 out = {cos(a), 0, sin(a)};
            const float3 top = out * 1.2f + float3{0, -0.1f, 0};
            const float3 down = normalize(out * 0.4f + float3{0, -1, 0});
            const float3 foot = down * 1.1f;
            builder.append(rod, {0.07f, 1.1f, 0.07f}, basis_from_y(down), top, hull, 0);
            builder.append(sphere, {0.3f, 0.07f, 0.3f}, none, top + foot, hull, 0);
            builder.append(ring, {0.2f, 0.4f, 0.2f}, none, top + foot * 0.85f, yellow, 0.35f);
        }
        builder.append(cone, {1.1f, -4.5f, 1.1f}, none, {0, -0.25f, 0}, yellow, 1.0f);
        saucer_ = gpu.mesh(builder.mesh);
        builder.mesh.release();
        sphere.release();
        ring.release();
        rod.release();
        cone.release();
    }

    ID3D12RootSignature *root_ = nullptr;
    ID3D12PipelineState *glow_pipeline_ = nullptr, *blur_pipeline_ = nullptr, *sky_pipeline_ = nullptr;
    ID3D12PipelineState *phong_pipeline_ = nullptr, *add_pipeline_ = nullptr;
    DrawMesh             saucer_;
    RenderTexture        glow_;
    RenderTexture        blurred_;
};

// ---------------------------------------------------------------- radialblur

// A caged glowing sphere: its glowing faces drawn into an off-screen target,
// then blurred radially from the centre and added over the lit scene.
class RadialBlur final : public Sample {
public:
    const char *name() const override { return "radialblur"; }
    bool init(Gpu &gpu) override
    {
        static const char *const radial_source = COMPUTE_QUAD_HLSL R"(
Texture2D image : register(t0);
SamplerState filter : register(s0);
cbuffer Radial : register(b0) { float2 texel; float blur_scale; float strength; float2 origin; float2 unused; };
float4 ps(Quad q) : SV_Target {
    float2 uv = q.uv + texel * 0.5 - origin;
    float4 color = 0;
    for (int i = 0; i < 16; i++) {
        float s = 1.0 - blur_scale * (i / 15.0);
        color += image.Sample(filter, uv * s + origin);
    }
    return color / 16.0 * strength;
}
)";
        root_ = glow_root(gpu);
        const D3D12_SHADER_BYTECODE scene_vs = gpu.vs("radialblur_scene_vs", glow_scene_source);
        glow_pipeline_ = gpu.pipeline(pass(gpu, root_, scene_vs, gpu.ps("radialblur_glow_ps", glow_scene_source, "emissive"),
                                           DXGI_FORMAT_R8G8B8A8_UNORM));
        phong_pipeline_ = gpu.pipeline(pass(gpu, root_, scene_vs, gpu.ps("radialblur_phong_ps", glow_scene_source, "phong")));
        D3D12_GRAPHICS_PIPELINE_STATE_DESC add =
            quad_pass(gpu, root_, gpu.vs("radialblur_quad_vs", quad_source), gpu.ps("radialblur_ps", radial_source));
        set_blend(add, Blend::additive);
        radial_pipeline_ = gpu.pipeline(add);
        if (gpu.failed) return false;
        build_cage(gpu);
        glow_.create(gpu, glow_size, glow_size, DXGI_FORMAT_R8G8B8A8_UNORM, false, true);
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        auto *l = gpu.list;
        const float3 eye = {0, 0, -12};
        GlowScene scene = {};
        scene.world = rotate_y(time / 8.0f * 2 * pi) * rotate_y(radians(-28.75f)) *
                      rotate_x(radians(-16.25f));
        scene.view_projection = look_at(eye, {0, 0, 0}, {0, 1, 0}) * perspective(radians(45), gpu.aspect(), 0.1f, 100);
        scene.light = {0.3f, 0.8f, -1.0f, 0};
        scene.eye = {eye.x, eye.y, eye.z, 1};
        l->SetGraphicsRootSignature(root_);
        l->SetGraphicsRootConstantBufferView(0, gpu.constants(&scene, sizeof scene));

        glow_.begin(gpu, 0, 0, 0);
        l->SetPipelineState(glow_pipeline_);
        cage_.draw(l);
        glow_.become(gpu, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        gpu.begin_frame(0, 0, 0);
        l->SetPipelineState(phong_pipeline_);
        cage_.draw(l);

        const float radial[8] = {1.0f / glow_size, 1.0f / glow_size, 0.25f, 0.75f, 0.5f, 0.5f, 0, 0};
        l->SetPipelineState(radial_pipeline_);
        l->SetGraphicsRootConstantBufferView(0, gpu.constants(radial, sizeof radial));
        l->SetGraphicsRootDescriptorTable(1, glow_.srv.gpu);
        draw_quad(l);
    }
    void release() override
    {
        safe_release(root_);
        safe_release(glow_pipeline_);
        safe_release(phong_pipeline_);
        safe_release(radial_pipeline_);
        cage_.release();
        glow_.release();
    }

private:
    static constexpr uint32_t glow_size = 512;

    // A once-subdivided icosahedron; each face becomes a glowing inner
    // triangle, set back a little, inside a dark frame.
    void build_cage(Gpu &gpu)
    {
        const float g = (1.0f + sqrt(5.0f)) * 0.5f;
        const float3 corners[12] = {{-1, g, 0}, {1, g, 0}, {-1, -g, 0}, {1, -g, 0}, {0, -1, g}, {0, 1, g},
                                    {0, -1, -g}, {0, 1, -g}, {g, 0, -1}, {g, 0, 1}, {-g, 0, -1}, {-g, 0, 1}};
        const uint8_t faces[20][3] = {{0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11}, {1, 5, 9}, {5, 11, 4},
                                      {11, 10, 2}, {10, 7, 6}, {7, 1, 8}, {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8},
                                      {3, 8, 9}, {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}};
        MeshBuilder builder(80 * 15, 80 * 21);
        const float radius = 3.0f;
        for (const auto &face : faces) {
            const float3 a = normalize(corners[face[0]]), b = normalize(corners[face[1]]), c = normalize(corners[face[2]]);
            const float3 ab = normalize(a + b), bc = normalize(b + c), ca = normalize(c + a);
            const float3 parts[4][3] = {{a, ab, ca}, {ab, b, bc}, {ca, bc, c}, {ab, bc, ca}};
            for (const auto &tri : parts) add_face(builder, tri[0] * radius, tri[1] * radius, tri[2] * radius);
        }
        cage_ = gpu.mesh(builder.mesh);
        builder.mesh.release();
    }
    static void add_face(MeshBuilder &builder, float3 a, float3 b, float3 c)
    {
        const float3 center = (a + b + c) * (1.0f / 3.0f);
        const float3 n = normalize(center);
        const float3 outer[3] = {a, b, c};
        float3 inner[3];
        for (int i = 0; i < 3; i++) inner[i] = center + (outer[i] - center) * 0.66f - n * 0.12f;
        const float3 yellow = {1.0f, 0.78f, 0.0f}, frame = {0.2f, 0.2f, 0.22f};
        const uint32_t base = builder.mesh.vertex_count;
        for (int i = 0; i < 3; i++) builder.push(inner[i], n, yellow, 1.0f);
        builder.triangle(base, base + 1, base + 2);
        for (int i = 0; i < 3; i++) {
            const int j = (i + 1) % 3;
            const uint32_t at = builder.mesh.vertex_count;
            builder.push(outer[i], n, frame, 0);
            builder.push(outer[j], n, frame, 0);
            builder.push(inner[j], n, frame, 0);
            builder.push(inner[i], n, frame, 0);
            builder.triangle(at, at + 1, at + 2);
            builder.triangle(at, at + 2, at + 3);
        }
    }

    ID3D12RootSignature *root_ = nullptr;
    ID3D12PipelineState *glow_pipeline_ = nullptr, *phong_pipeline_ = nullptr, *radial_pipeline_ = nullptr;
    DrawMesh             cage_;
    RenderTexture        glow_;
};

// ---------------------------------------------------------------- offscreen

// A mirror floor: the scene, reflected and clipped at the floor plane, is rendered into a texture that the floor samples
// with projected coordinates.
class Offscreen final : public Sample {
public:
    const char *name() const override { return "offscreen"; }
    bool init(Gpu &gpu) override
    {
        static const char *const phong_source = R"(
cbuffer Object : register(b0) {
    row_major float4x4 world;
    row_major float4x4 view_projection;
    float4 light;    // xyz: position
    float4 eye;
    float4 color;    // w: specular strength
    float4 clip_plane;   // the world position must be on its positive side
};
struct In { float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD; float3 color : COLOR; };
struct Out { float4 position : SV_Position; float3 normal : NORMAL; float3 world_position : WORLD; };
Out vs(In v) {
    Out o;
    float4 w = mul(float4(v.position, 1), world);
    o.position = mul(w, view_projection);
    o.world_position = w.xyz;
    o.normal = mul(float4(v.normal, 0), world).xyz;
    return o;
}
float4 ps(Out p) : SV_Target {
    clip(dot(float4(p.world_position, 1), clip_plane));
    float3 n = normalize(p.normal);
    float3 l = normalize(light.xyz - p.world_position);
    float3 v = normalize(eye.xyz - p.world_position);
    float diffuse = saturate(dot(n, l));
    float specular = pow(saturate(dot(reflect(-l, n), v)), 24.0) * color.w;
    return float4(color.rgb * (0.08 + 0.92 * diffuse) + specular, 1);
}
)";
        static const char *const mirror_source = R"(
cbuffer Object : register(b0) { row_major float4x4 world; row_major float4x4 view_projection; };
Texture2D reflection : register(t0);
Texture2D plate : register(t1);
SamplerState filter : register(s0);
SamplerState tiled : register(s1);
struct In { float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD; float3 color : COLOR; };
struct Out { float4 position : SV_Position; float2 uv : TEXCOORD; float4 projected : PROJECTED; };
Out vs(In v) {
    Out o;
    o.position = mul(mul(float4(v.position, 1), world), view_projection);
    o.projected = o.position;
    o.uv = v.uv;
    return o;
}
float4 ps(Out p) : SV_Target {
    float2 ndc = p.projected.xy / p.projected.w;
    float2 uv = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
    const float blur = 1.0 / 512.0;
    float4 mirrored = 0;
    for (int x = -3; x <= 3; x++)
        for (int y = -3; y <= 3; y++)
            mirrored += reflection.Sample(filter, uv + float2(x, y) * blur) / 49.0;
    float4 color = plate.Sample(tiled, p.uv);
    return float4(color.rgb * 0.25 + mirrored.rgb * 1.5 * color.r, 1);
}
)";
        phong_root_ = gpu.root_signature(RootLayout().cbv(0));
        const D3D12_SHADER_BYTECODE phong_vs = gpu.vs("offscreen_phong_vs", phong_source);
        const D3D12_SHADER_BYTECODE phong_ps = gpu.ps("offscreen_phong_ps", phong_source);
        mirrored_pipeline_ = gpu.pipeline(pass(gpu, phong_root_, phong_vs, phong_ps, DXGI_FORMAT_R8G8B8A8_UNORM));
        phong_pipeline_ = gpu.pipeline(pass(gpu, phong_root_, phong_vs, phong_ps));
        mirror_root_ = gpu.root_signature(RootLayout()
                                              .cbv(0, D3D12_SHADER_VISIBILITY_VERTEX)
                                              .srv_table(0, 2, D3D12_SHADER_VISIBILITY_PIXEL)
                                              .sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                                                       D3D12_SHADER_VISIBILITY_PIXEL)
                                              .sampler(1, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                                                       D3D12_SHADER_VISIBILITY_PIXEL));
        mirror_pipeline_ = gpu.pipeline(pass(gpu, mirror_root_, gpu.vs("offscreen_mirror_vs", mirror_source),
                                             gpu.ps("offscreen_mirror_ps", mirror_source)));
        if (gpu.failed) return false;
        Mesh knot = knot_mesh(384, 20);
        knot_ = gpu.mesh(knot);
        knot.release();
        Mesh ball = sphere_mesh(0.3f, 32, 16);
        ball_ = gpu.mesh(ball);
        ball.release();
        Mesh floor = plane_mesh(8.0f, 1, 2.0f);
        floor_ = gpu.mesh(floor);
        floor.release();
        make_plate(gpu);
        mirror_.create(gpu, mirror_size, mirror_size, DXGI_FORMAT_R8G8B8A8_UNORM, false, true);
        // t0 the reflection, t1 the plate, side by side for one table.
        table_ = gpu.views(2);
        if (table_.count != 0) {
            gpu.texture_view(mirror_.texture, table_.cpu_at(0));
            gpu.texture_view(plate_.resource, table_.cpu_at(1), &plate_.view);
        }
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        auto *l = gpu.list;
        const float3 eye = {0, 2.6f, -6.2f};
        const float4x4 view_projection =
            look_at(eye, {0, 0.6f, 0}, {0, 1, 0}) * perspective(radians(60), gpu.aspect(), 0.1f, 100);
        const float4x4 turn = rotate_y(0.6f + time * 0.25f);
        const float3 light = {1.5f, 4.0f, -2.5f};

        mirror_.begin(gpu, 0, 0, 0);
        draw_objects(gpu, mirrored_pipeline_, turn * scale({1, -1, 1}), view_projection, {light.x, -light.y, light.z},
                     eye, {0, -1, 0, 0}, time);
        mirror_.become(gpu, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        gpu.begin_frame(0, 0, 0);
        Object floor = {};
        floor.world = turn;
        floor.view_projection = view_projection;
        l->SetGraphicsRootSignature(mirror_root_);
        l->SetPipelineState(mirror_pipeline_);
        l->SetGraphicsRootConstantBufferView(0, gpu.constants(&floor, sizeof floor));
        l->SetGraphicsRootDescriptorTable(1, table_.gpu);
        floor_.draw(l);

        draw_objects(gpu, phong_pipeline_, turn, view_projection, light, eye, {0, 0, 0, 1}, time);
    }
    void release() override
    {
        safe_release(phong_root_);
        safe_release(mirror_root_);
        safe_release(mirrored_pipeline_);
        safe_release(phong_pipeline_);
        safe_release(mirror_pipeline_);
        knot_.release();
        ball_.release();
        floor_.release();
        plate_.release();
        mirror_.release();
    }

private:
    struct Object {
        float4x4 world;
        float4x4 view_projection;
        float4   light;
        float4   eye;
        float4   color;
        float4   clip;
    };
    static constexpr uint32_t mirror_size = 512;

    void draw_objects(Gpu &gpu, ID3D12PipelineState *pipeline, const float4x4 &placement,
                      const float4x4 &view_projection, float3 light, float3 eye, float4 clip, float time)
    {
        auto *l = gpu.list;
        l->SetGraphicsRootSignature(phong_root_);
        l->SetPipelineState(pipeline);
        Object object = {};
        object.view_projection = view_projection;
        object.light = {light.x, light.y, light.z, 1};
        object.eye = {eye.x, eye.y, eye.z, 1};
        object.clip = clip;
        object.world = rotate_y(time * 0.5f) * translate({0, 1.22f, 0.3f}) * placement;
        object.color = {0.85f, 0.02f, 0.02f, 0.8f};
        l->SetGraphicsRootConstantBufferView(0, gpu.constants(&object, sizeof object));
        knot_.draw(l);
        object.world = translate({0.2f, 0.3f, -1.15f}) * placement;
        object.color = {0.06f, 0.06f, 0.07f, 1.0f};
        l->SetGraphicsRootConstantBufferView(0, gpu.constants(&object, sizeof object));
        ball_.draw(l);
    }

    // A (2,3) torus knot tube, standing in for the original's dragon.
    static Mesh knot_mesh(uint32_t rings, uint32_t sides)
    {
        Mesh mesh;
        mesh.vertices = static_cast<Vertex *>(allocate(sizeof(Vertex) * (rings + 1) * (sides + 1)));
        mesh.indices = static_cast<uint32_t *>(allocate(sizeof(uint32_t) * rings * sides * 6));
        auto curve = [](float t) {
            const float r = 2.0f + cos(3.0f * t);
            return float3{r * cos(2.0f * t), r * sin(2.0f * t), sin(3.0f * t)} * 0.36f;
        };
        const float tube = 0.2f, e = 0.001f;
        for (uint32_t i = 0; i <= rings; i++) {
            const float t = static_cast<float>(i) / static_cast<float>(rings) * 2 * pi;
            const float3 p = curve(t), ahead = curve(t + e), behind = curve(t - e);
            const float3 tangent = normalize(ahead - behind);
            const float3 bend = ahead + behind - p * 2.0f;
            const float3 normal = normalize(bend - tangent * dot(bend, tangent));
            const float3 binormal = cross(tangent, normal);
            for (uint32_t s = 0; s <= sides; s++) {
                const float a = static_cast<float>(s) / static_cast<float>(sides) * 2 * pi;
                const float3 n = normal * cos(a) + binormal * sin(a);
                mesh.vertices[mesh.vertex_count++] = {p + n * tube, n,
                                                      {static_cast<float>(i) / rings, static_cast<float>(s) / sides},
                                                      {1, 1, 1}};
            }
        }
        for (uint32_t i = 0; i < rings; i++)
            for (uint32_t s = 0; s < sides; s++) {
                const uint32_t a = i * (sides + 1) + s, b = a + sides + 1;
                uint32_t *at = mesh.indices + mesh.index_count;
                at[0] = a; at[1] = b; at[2] = b + 1;
                at[3] = a; at[4] = b + 1; at[5] = a + 1;
                mesh.index_count += 6;
            }
        return mesh;
    }

    // Riveted, scratched metal plates.
    void make_plate(Gpu &gpu)
    {
        const uint32_t n = 512, panel = 256;
        uint32_t *texels = static_cast<uint32_t *>(allocate(n * n * 4));
        for (uint32_t y = 0; y < n; y++)
            for (uint32_t x = 0; x < n; x++) {
                const float u = static_cast<float>(x) / n, v = static_cast<float>(y) / n;
                float g = 0.3f + 0.12f * fbm(u * 8, v * 8, 61u, 4, 8);
                const float scratch = fbm(u * 3, v * 90, 67u, 3, 0);
                g += smooth(0.62f, 0.72f, scratch) * 0.12f;
                const uint32_t px = x % panel, py = y % panel;
                if (px < 3 || py < 3) g *= 0.25f;
                const float rx = static_cast<float>(px % 64) - 32.0f, ry = static_cast<float>(py) - 14.0f;
                const float ry2 = static_cast<float>(py) - (panel - 14.0f);
                const float d = min(rx * rx + ry * ry, rx * rx + ry2 * ry2);
                if (d < 36.0f) g = d < 12.0f ? 0.5f + 0.02f * d : 0.08f;
                texels[y * n + x] = pack_rgba(g, g, g * 1.02f, 1);
            }
        plate_ = gpu.texture(texels, n, n);
        release_memory(texels);
    }

    ID3D12RootSignature *phong_root_ = nullptr, *mirror_root_ = nullptr;
    ID3D12PipelineState *mirrored_pipeline_ = nullptr, *phong_pipeline_ = nullptr, *mirror_pipeline_ = nullptr;
    DrawMesh             knot_;
    DrawMesh             ball_;
    DrawMesh             floor_;
    Texture              plate_;
    RenderTexture        mirror_;
    Descriptors          table_;
};

}  // namespace

void register_compute(Sample **all, uint32_t &made)
{
    all[made++] = new ComputeParticles();
    all[made++] = new ComputeShader();
    all[made++] = new ParticleFire();
    all[made++] = new Raytracing();
    all[made++] = new Bloom();
    all[made++] = new RadialBlur();
    all[made++] = new Offscreen();
}

}  // namespace samples
