#include "framework.h"

namespace samples {

namespace {

float3 transform_point(const float4x4 &m, float3 p)
{
    return {p.x * m.m[0][0] + p.y * m.m[1][0] + p.z * m.m[2][0] + m.m[3][0],
            p.x * m.m[0][1] + p.y * m.m[1][1] + p.z * m.m[2][1] + m.m[3][1],
            p.x * m.m[0][2] + p.y * m.m[1][2] + p.z * m.m[2][2] + m.m[3][2]};
}

float3 transform_vector(const float4x4 &m, float3 v)
{
    return {v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0],
            v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1],
            v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2]};
}

float4x4 basis(float3 x, float3 y, float3 z, float3 origin)
{
    float4x4 r = float4x4::identity();
    const float3 rows[4] = {x, y, z, origin};
    for (int i = 0; i < 4; i++) {
        r.m[i][0] = rows[i].x;
        r.m[i][1] = rows[i].y;
        r.m[i][2] = rows[i].z;
    }
    return r;
}

float4x4 along(float3 from, float3 axis)
{
    const float3 y = normalize(axis);
    const float3 helper = abs(y.y) < 0.9f ? float3{0, 1, 0} : float3{1, 0, 0};
    const float3 x = normalize(cross(helper, y));
    const float3 z = cross(x, y);
    return basis(x, y, z, from);
}

float3 mul3(float3 a, float3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
float  smoothstep01(float t) { return t * t * (3.0f - 2.0f * t); }
float  saturate(float x) { return clamp(x, 0.0f, 1.0f); }

uint32_t hash_u32(uint32_t x)
{
    x ^= x >> 16u;
    x *= 0x7feb352du;
    x ^= x >> 15u;
    x *= 0x846ca68bu;
    x ^= x >> 16u;
    return x;
}

int wrap(int i, int period) { return period > 0 ? ((i % period) + period) % period : i; }

float lattice(int x, int y, int z, uint32_t seed)
{
    const uint32_t h = hash_u32(seed ^ hash_u32(static_cast<uint32_t>(x) * 73856093u ^
                                                hash_u32(static_cast<uint32_t>(y) * 19349663u ^
                                                         hash_u32(static_cast<uint32_t>(z) * 83492791u))));
    return static_cast<float>(h & 0xFFFFFFu) / 16777216.0f;
}

float noise3(float3 p, uint32_t seed, int period = 0)
{
    const float fx = floor(p.x), fy = floor(p.y), fz = floor(p.z);
    const int ix = static_cast<int>(fx), iy = static_cast<int>(fy), iz = static_cast<int>(fz);
    const float tx = smoothstep01(p.x - fx), ty = smoothstep01(p.y - fy), tz = smoothstep01(p.z - fz);
    float corners[2][2][2];
    for (int dz = 0; dz < 2; dz++)
        for (int dy = 0; dy < 2; dy++)
            for (int dx = 0; dx < 2; dx++)
                corners[dz][dy][dx] = lattice(wrap(ix + dx, period), wrap(iy + dy, period), iz + dz, seed);
    const float a = mix(mix(corners[0][0][0], corners[0][0][1], tx), mix(corners[0][1][0], corners[0][1][1], tx), ty);
    const float b = mix(mix(corners[1][0][0], corners[1][0][1], tx), mix(corners[1][1][0], corners[1][1][1], tx), ty);
    return mix(a, b, tz);
}

float fbm2(float u, float v, int cells, int octaves, uint32_t seed)
{
    float sum = 0.0f, amplitude = 0.5f, total = 0.0f;
    for (int o = 0; o < octaves; o++) {
        const int period = cells << o;
        sum += amplitude * noise3({u * static_cast<float>(period), v * static_cast<float>(period), 0.0f}, seed + o, period);
        total += amplitude;
        amplitude *= 0.5f;
    }
    return sum / total;
}

uint32_t rgba(float3 c)
{
    const uint32_t r = static_cast<uint32_t>(saturate(c.x) * 255.0f + 0.5f);
    const uint32_t g = static_cast<uint32_t>(saturate(c.y) * 255.0f + 0.5f);
    const uint32_t b = static_cast<uint32_t>(saturate(c.z) * 255.0f + 0.5f);
    return 0xFF000000u | (b << 16u) | (g << 8u) | r;
}

struct MeshBuilder {
    Mesh     mesh;
    uint32_t vertex_capacity = 0;
    uint32_t index_capacity = 0;

    void begin(uint32_t vertices, uint32_t indices)
    {
        mesh.vertices = static_cast<Vertex *>(allocate(sizeof(Vertex) * vertices));
        mesh.indices = static_cast<uint32_t *>(allocate(sizeof(uint32_t) * indices));
        vertex_capacity = vertices;
        index_capacity = indices;
    }

    uint32_t vertex(float3 p, float3 n, float2 uv, float3 c)
    {
        if (mesh.vertex_count >= vertex_capacity) return 0;
        mesh.vertices[mesh.vertex_count] = Vertex{p, n, uv, c};
        return mesh.vertex_count++;
    }

    void triangle(uint32_t a, uint32_t b, uint32_t c)
    {
        if (mesh.index_count + 3 > index_capacity) return;
        const Vertex *v = mesh.vertices;
        const float3 n = v[a].normal + v[b].normal + v[c].normal;
        if (dot(cross(v[b].position - v[a].position, v[c].position - v[a].position), n) < 0.0f) {
            const uint32_t swap = b;
            b = c;
            c = swap;
        }
        uint32_t *i = mesh.indices + mesh.index_count;
        i[0] = a;
        i[1] = b;
        i[2] = c;
        mesh.index_count += 3;
    }

    void quad(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
    {
        triangle(a, b, c);
        triangle(a, c, d);
    }

    void lathe(const float2 *profile, uint32_t count, uint32_t segments, const float4x4 &place, float3 color,
               bool faceted = false, float3 stretch = {1, 1, 1})
    {
        const float3 inverse = {1.0f / stretch.x, 1.0f / stretch.y, 1.0f / stretch.z};
        auto normal2 = [](float2 a, float2 b) {
            const float dx = b.x - a.x, dy = b.y - a.y, l = sqrt(dx * dx + dy * dy);
            return l > 0.0f ? float2{dy / l, -dx / l} : float2{0, 0};
        };
        auto blend = [](float2 a, float2 b) {
            if (a.x * b.x + a.y * b.y < 0.75f) return b;
            const float x = a.x + b.x, y = a.y + b.y, l = sqrt(x * x + y * y);
            return float2{x / l, y / l};
        };
        for (uint32_t k = 0; k + 1 < count; k++) {
            const float2 a = profile[k], b = profile[k + 1];
            if (a.x == 0.0f && b.x == 0.0f) continue;
            const float2 face = normal2(a, b);
            if (face.x == 0.0f && face.y == 0.0f) continue;
            const float2 na = k > 0 ? blend(normal2(profile[k - 1], a), face) : face;
            const float2 nb = k + 2 < count ? blend(normal2(b, profile[k + 2]), face) : face;
            const float v0 = static_cast<float>(k) / static_cast<float>(count - 1);
            const float v1 = static_cast<float>(k + 1) / static_cast<float>(count - 1);
            for (uint32_t s = 0; s < segments; s++) {
                const float t0 = static_cast<float>(s) / static_cast<float>(segments);
                const float t1 = static_cast<float>(s + 1) / static_cast<float>(segments);
                const float c0 = cos(t0 * 2 * pi), s0 = sin(t0 * 2 * pi), c1 = cos(t1 * 2 * pi), s1 = sin(t1 * 2 * pi);
                const float cm = cos((t0 + t1) * pi), sm_ = sin((t0 + t1) * pi);
                auto emit = [&](float2 p, float2 n, float c, float s, float u, float v) {
                    const float3 position = mul3({p.x * c, p.y, p.x * s}, stretch);
                    const float cc = faceted ? cm : c, ss = faceted ? sm_ : s;
                    const float3 normal = normalize(mul3({n.x * cc, n.y, n.x * ss}, inverse));
                    return vertex(transform_point(place, position), normalize(transform_vector(place, normal)), {u, v},
                                  color);
                };
                const uint32_t i0 = emit(a, na, c0, s0, t0, v0), i1 = emit(a, na, c1, s1, t1, v0);
                const uint32_t i2 = emit(b, nb, c1, s1, t1, v1), i3 = emit(b, nb, c0, s0, t0, v1);
                quad(i0, i1, i2, i3);
            }
        }
    }

    void cylinder(float3 from, float3 to, float r0, float r1, uint32_t segments, float3 color, bool caps = true,
                  bool faceted = false)
    {
        const float h = length(to - from);
        if (h <= 0.0f) return;
        const float4x4 place = along(from, to - from);
        const float2 capped[4] = {{0, 0}, {r0, 0}, {r1, h}, {0, h}};
        const float2 open[2] = {{r0, 0}, {r1, h}};
        if (caps) lathe(capped, 4, segments, place, color, faceted);
        else lathe(open, 2, segments, place, color, faceted);
    }

    void ellipsoid(float3 center, float3 radii, uint32_t slices, uint32_t stacks, float3 color)
    {
        float2 profile[65];
        if (stacks > 64) stacks = 64;
        for (uint32_t j = 0; j <= stacks; j++) {
            const float phi = static_cast<float>(j) / static_cast<float>(stacks) * pi;
            profile[j] = {sin(phi), -cos(phi)};
        }
        lathe(profile, stacks + 1, slices, translate(center), color, false, radii);
    }

    void box(const float4x4 &place, float3 size, float3 color)
    {
        Mesh cube = cube_mesh(1.0f);
        for (uint32_t f = 0; f < 6; f++) {
            uint32_t ids[4];
            for (uint32_t k = 0; k < 4; k++) {
                const Vertex &v = cube.vertices[f * 4 + k];
                ids[k] = vertex(transform_point(place, mul3(v.position, size)),
                                normalize(transform_vector(place, v.normal)), v.uv, color);
            }
            quad(ids[0], ids[1], ids[2], ids[3]);
        }
        cube.release();
    }

    void blade(float3 a, float3 b, float3 c, float3 n, float3 color)
    {
        triangle(vertex(a, n, {0, 1}, color), vertex(b, n, {1, 1}, color), vertex(c, n, {0.5f, 0}, color));
        const float3 m = -n;
        triangle(vertex(a, m, {0, 1}, color), vertex(b, m, {1, 1}, color), vertex(c, m, {0.5f, 0}, color));
    }
};

struct Frame {
    float4x4 world;
    float4x4 view_projection;
    float4   light;
    float4   eye;
    float4   params;
};

#define GEOMETRY_FRAME                                                                         \
    "cbuffer Frame : register(b0) {\n"                                                         \
    "    row_major float4x4 world; row_major float4x4 view_projection;\n"                     \
    "    float4 light; float4 eye; float4 params;\n"                                          \
    "};\n"                                                                                     \
    "struct In { float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD; "  \
    "float3 color : COLOR; };\n"

ID3D12RootSignature *frame_root(Gpu &gpu, bool texture, D3D12_FILTER filter = D3D12_FILTER_ANISOTROPIC)
{
    RootLayout layout;
    layout.cbv(0);
    if (texture) layout.srv_table(0, 1).sampler(0, filter);
    return gpu.root_signature(layout);
}

void draw_patches(ID3D12GraphicsCommandList *list, const DrawMesh &mesh)
{
    list->IASetVertexBuffers(0, 1, &mesh.vertex_view);
    list->IASetIndexBuffer(&mesh.index_view);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
    list->DrawIndexedInstanced(mesh.index_count, 1, 0, 0, 0);
}

Mesh head_mesh()
{
    MeshBuilder b;
    b.begin(8000, 24000);
    const float3 skin = {0.85f, 0.55f, 0.3f};
    b.ellipsoid({0, 0, 0}, {1.0f, 0.95f, 0.9f}, 32, 20, skin);
    b.ellipsoid({-1.05f, 0.2f, 0.1f}, {0.2f, 0.45f, 0.4f}, 16, 10, skin);
    b.ellipsoid({1.05f, 0.2f, 0.1f}, {0.2f, 0.45f, 0.4f}, 16, 10, skin);
    b.ellipsoid({0, -0.35f, -0.62f}, {0.62f, 0.4f, 0.4f}, 24, 12, {0.95f, 0.75f, 0.5f});
    b.ellipsoid({-0.35f, 0.25f, -0.72f}, {0.2f, 0.2f, 0.12f}, 12, 8, {0.1f, 0.1f, 0.1f});
    b.ellipsoid({0.35f, 0.25f, -0.72f}, {0.2f, 0.2f, 0.12f}, 12, 8, {0.1f, 0.1f, 0.1f});
    return b.mesh;
}

class GeometryShader final : public Sample {
public:
    const char *name() const override { return "geometryshader"; }
    bool init(Gpu &gpu) override
    {
        static const char *const source = GEOMETRY_FRAME R"(
struct Lit { float4 position : SV_Position; float3 normal : NORMAL; float3 color : COLOR; float3 world_position : POSITION1; };
Lit vs(In v) {
    Lit o;
    float4 w = mul(float4(v.position, 1), world);
    o.position = mul(w, view_projection);
    o.normal = mul(float4(v.normal, 0), world).xyz;
    o.color = v.color;
    o.world_position = w.xyz;
    return o;
}
float4 ps(Lit p) : SV_Target {
    float3 n = normalize(p.normal);
    float3 l = normalize(light.xyz - p.world_position);
    float3 v = normalize(eye.xyz - p.world_position);
    float3 r = reflect(-l, n);
    float diffuse = max(dot(n, l), 0.0);
    float specular = pow(max(dot(r, v), 0.0), 16.0) * 0.35;
    return float4(p.color * (0.2 + 0.6 * diffuse) + specular, 1);
}
struct Raw { float3 position : POSITION; float3 normal : NORMAL; };
Raw vs_normals(In v) { Raw r; r.position = v.position; r.normal = v.normal; return r; }
struct Line { float4 position : SV_Position; float3 color : COLOR; };
[maxvertexcount(6)]
void gs(triangle Raw input[3], inout LineStream<Line> lines) {
    for (int i = 0; i < 3; i++) {
        Line root;
        root.position = mul(mul(float4(input[i].position, 1), world), view_projection);
        root.color = float3(1, 0, 0);
        lines.Append(root);
        Line tip;
        tip.position = mul(mul(float4(input[i].position + input[i].normal * params.x, 1), world), view_projection);
        tip.color = float3(0, 0, 1);
        lines.Append(tip);
        lines.RestartStrip();
    }
}
float4 ps_line(Line p) : SV_Target { return float4(p.color, 1); }
)";
        root_ = frame_root(gpu, false);
        lit_ = gpu.pipeline(gpu.graphics(root_, gpu.vs("geometryshader_vs", source), gpu.ps("geometryshader_ps", source)));
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = gpu.graphics(root_, gpu.vs("geometryshader_normals_vs", source, "vs_normals"),
                                                               gpu.ps("geometryshader_line_ps", source, "ps_line"));
        desc.GS = gpu.gs("geometryshader_gs", source);
        normals_ = gpu.pipeline(desc);
        Mesh mesh = head_mesh();
        head_ = gpu.mesh(mesh);
        mesh.release();
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        gpu.begin_frame(0.0f, 0.0f, 0.0f);
        const float3 eye = {0, 0.3f, -3.1f};
        Frame frame = {};
        frame.world = rotate_y(radians(-25) + time * 0.5f);
        frame.view_projection = look_at(eye, {0, 0, 0}, {0, 1, 0}) * perspective(radians(60), gpu.aspect(), 0.1f, 100);
        frame.light = {3, 4, -5, 0};
        frame.eye = {eye.x, eye.y, eye.z, 0};
        frame.params = {0.12f, 0, 0, 0};
        auto *l = gpu.list;
        l->SetGraphicsRootSignature(root_);
        l->SetGraphicsRootConstantBufferView(0, gpu.constants(&frame, sizeof frame));
        l->SetPipelineState(lit_);
        head_.draw(l);
        l->SetPipelineState(normals_);
        head_.draw(l);
    }
    void release() override
    {
        safe_release(root_);
        safe_release(lit_);
        safe_release(normals_);
        head_.release();
    }

private:
    ID3D12RootSignature *root_ = nullptr;
    ID3D12PipelineState *lit_ = nullptr;
    ID3D12PipelineState *normals_ = nullptr;
    DrawMesh             head_;
};

class Tessellation final : public Sample {
public:
    const char *name() const override { return "tessellation"; }
    bool init(Gpu &gpu) override
    {
        static const char *const source = GEOMETRY_FRAME R"(
struct Point { float3 position : POSITION; float3 normal : NORMAL; float3 color : COLOR; };
Point vs(In v) { Point p; p.position = v.position; p.normal = normalize(v.normal); p.color = v.color; return p; }

struct Levels { float edges[3] : SV_TessFactor; float inside : SV_InsideTessFactor; };
Levels flat_levels(InputPatch<Point, 3> p) {
    Levels o;
    o.edges[0] = o.edges[1] = o.edges[2] = params.x;
    o.inside = params.x;
    return o;
}
[domain("tri")] [partitioning("fractional_odd")] [outputtopology("triangle_cw")]
[outputcontrolpoints(3)] [patchconstantfunc("flat_levels")]
Point hs_flat(InputPatch<Point, 3> p, uint i : SV_OutputControlPointID) { return p[i]; }

struct PnPatch {
    float edges[3] : SV_TessFactor; float inside : SV_InsideTessFactor;
    float3 b210 : B210; float3 b120 : B120; float3 b021 : B021; float3 b012 : B012;
    float3 b102 : B102; float3 b201 : B201; float3 b111 : B111;
    float3 n110 : N110; float3 n011 : N011; float3 n101 : N101;
};
float3 edge_point(float3 a, float3 b, float3 na) { return (2 * a + b - dot(b - a, na) * na) / 3; }
float3 edge_normal(float3 a, float3 b, float3 na, float3 nb) {
    float3 d = b - a;
    return normalize(na + nb - 2 * dot(d, na + nb) / dot(d, d) * d);
}
PnPatch pn_levels(InputPatch<Point, 3> p) {
    PnPatch o;
    o.edges[0] = o.edges[1] = o.edges[2] = params.x;
    o.inside = params.x;
    float3 p0 = p[0].position, p1 = p[1].position, p2 = p[2].position;
    float3 n0 = p[0].normal, n1 = p[1].normal, n2 = p[2].normal;
    o.b210 = edge_point(p0, p1, n0); o.b120 = edge_point(p1, p0, n1);
    o.b021 = edge_point(p1, p2, n1); o.b012 = edge_point(p2, p1, n2);
    o.b102 = edge_point(p2, p0, n2); o.b201 = edge_point(p0, p2, n0);
    float3 e = (o.b210 + o.b120 + o.b021 + o.b012 + o.b102 + o.b201) / 6;
    float3 v = (p0 + p1 + p2) / 3;
    o.b111 = e + (e - v) * 0.5;
    o.n110 = edge_normal(p0, p1, n0, n1);
    o.n011 = edge_normal(p1, p2, n1, n2);
    o.n101 = edge_normal(p2, p0, n2, n0);
    return o;
}
[domain("tri")] [partitioning("fractional_odd")] [outputtopology("triangle_cw")]
[outputcontrolpoints(3)] [patchconstantfunc("pn_levels")]
Point hs_pn(InputPatch<Point, 3> p, uint i : SV_OutputControlPointID) { return p[i]; }

struct Shaded { float4 position : SV_Position; float3 normal : NORMAL; float3 color : COLOR; float3 world_position : POSITION1; };
Shaded shade(float3 position, float3 normal, float3 color) {
    Shaded o;
    float4 w = mul(float4(position, 1), world);
    o.position = mul(w, view_projection);
    o.normal = mul(float4(normal, 0), world).xyz;
    o.color = color;
    o.world_position = w.xyz;
    return o;
}
[domain("tri")]
Shaded ds_flat(Levels c, float3 uvw : SV_DomainLocation, const OutputPatch<Point, 3> p) {
    return shade(uvw.x * p[0].position + uvw.y * p[1].position + uvw.z * p[2].position,
                 uvw.x * p[0].normal + uvw.y * p[1].normal + uvw.z * p[2].normal,
                 uvw.x * p[0].color + uvw.y * p[1].color + uvw.z * p[2].color);
}
[domain("tri")]
Shaded ds_pn(PnPatch c, float3 uvw : SV_DomainLocation, const OutputPatch<Point, 3> p) {
    float u = uvw.x, v = uvw.y, w = uvw.z;
    float3 flat_position = u * p[0].position + v * p[1].position + w * p[2].position;
    float3 flat_normal = u * p[0].normal + v * p[1].normal + w * p[2].normal;
    float3 pn_position = p[0].position * u * u * u + p[1].position * v * v * v + p[2].position * w * w * w
        + c.b210 * 3 * u * u * v + c.b120 * 3 * u * v * v + c.b201 * 3 * u * u * w
        + c.b102 * 3 * u * w * w + c.b021 * 3 * v * v * w + c.b012 * 3 * v * w * w
        + c.b111 * 6 * u * v * w;
    float3 pn_normal = p[0].normal * u * u + p[1].normal * v * v + p[2].normal * w * w
        + c.n110 * u * v + c.n011 * v * w + c.n101 * u * w;
    float alpha = params.y;
    return shade(lerp(flat_position, pn_position, alpha), lerp(flat_normal, pn_normal, alpha),
                 u * p[0].color + v * p[1].color + w * p[2].color);
}
float4 ps(Shaded p) : SV_Target {
    float3 n = normalize(p.normal);
    float3 l = normalize(light.xyz - p.world_position);
    float3 v = normalize(eye.xyz - p.world_position);
    float diffuse = max(dot(n, l), 0.0);
    float specular = pow(max(dot(reflect(-l, n), v), 0.0), 24.0) * 0.4;
    return float4(p.color * (0.15 + 0.85 * diffuse) + specular, 1);
}
float4 ps_wire(Shaded p) : SV_Target { return float4(0.05, 0.05, 0.08, 1); }
)";
        root_ = frame_root(gpu, false);
        const D3D12_SHADER_BYTECODE vs = gpu.vs("tessellation_vs", source);
        const D3D12_SHADER_BYTECODE hs[2] = {gpu.hs("tessellation_hs_flat", source, "hs_flat"),
                                             gpu.hs("tessellation_hs_pn", source, "hs_pn")};
        const D3D12_SHADER_BYTECODE ds[2] = {gpu.ds("tessellation_ds_flat", source, "ds_flat"),
                                             gpu.ds("tessellation_ds_pn", source, "ds_pn")};
        const D3D12_SHADER_BYTECODE ps = gpu.ps("tessellation_ps", source);
        const D3D12_SHADER_BYTECODE wire_ps = gpu.ps("tessellation_wire_ps", source, "ps_wire");
        for (int side = 0; side < 2; side++) {
            D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = gpu.graphics(root_, vs, ps);
            desc.HS = hs[side];
            desc.DS = ds[side];
            desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
            solid_[side] = gpu.pipeline(desc);
            desc.PS = wire_ps;
            desc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
            desc.RasterizerState.DepthBias = -200;
            desc.RasterizerState.SlopeScaledDepthBias = -1.0f;
            wire_[side] = gpu.pipeline(desc);
        }
        Mesh mesh = torus_mesh(1.0f, 0.5f, 10, 6);
        for (uint32_t i = 0; i < mesh.vertex_count; i++) {
            const float up = mesh.vertices[i].normal.y;
            mesh.vertices[i].color = {0.8f, 0.5f + 0.15f * up, 0.25f + 0.1f * up};
        }
        torus_ = gpu.mesh(mesh);
        mesh.release();
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        gpu.begin_frame(0.025f, 0.025f, 0.025f);
        const float half = static_cast<float>(gpu.width) * 0.5f;
        const float3 eye = {0, 2.4f, -3.6f};
        Frame frame = {};
        frame.world = rotate_y(time * 0.4f) * rotate_x(0.3f);
        frame.view_projection = look_at(eye, {0, 0, 0}, {0, 1, 0}) *
                                perspective(radians(45), half / static_cast<float>(gpu.height), 0.1f, 100);
        frame.light = {-4, 6, -4, 0};
        frame.eye = {eye.x, eye.y, eye.z, 0};
        frame.params = {3.0f, 1.0f, 0, 0};
        auto *l = gpu.list;
        l->SetGraphicsRootSignature(root_);
        l->SetGraphicsRootConstantBufferView(0, gpu.constants(&frame, sizeof frame));
        for (int side = 0; side < 2; side++) {
            gpu.viewport(half * static_cast<float>(side), 0, half, static_cast<float>(gpu.height));
            l->SetPipelineState(solid_[side]);
            draw_patches(l, torus_);
            l->SetPipelineState(wire_[side]);
            draw_patches(l, torus_);
        }
    }
    void release() override
    {
        safe_release(root_);
        for (int side = 0; side < 2; side++) {
            safe_release(solid_[side]);
            safe_release(wire_[side]);
        }
        torus_.release();
    }

private:
    ID3D12RootSignature *root_ = nullptr;
    ID3D12PipelineState *solid_[2] = {};
    ID3D12PipelineState *wire_[2] = {};
    DrawMesh             torus_;
};

uint32_t *brick_texels(uint32_t size)
{
    uint32_t *texels = static_cast<uint32_t *>(allocate(size * size * 4));
    const float brick_w = 64.0f, brick_h = 32.0f, mortar = 3.0f, bevel = 6.0f;
    for (uint32_t y = 0; y < size; y++)
        for (uint32_t x = 0; x < size; x++) {
            const float fy = static_cast<float>(y);
            const int row = static_cast<int>(fy / brick_h);
            const float fx = static_cast<float>(x) + ((row & 1) != 0 ? brick_w * 0.5f : 0.0f);
            const float lx = fx - floor(fx / brick_w) * brick_w, ly = fy - floor(fy / brick_h) * brick_h;
            const float edge = min(min(lx, brick_w - lx), min(ly, brick_h - ly));
            const float u = static_cast<float>(x) / static_cast<float>(size);
            const float v = static_cast<float>(y) / static_cast<float>(size);
            const float grain = fbm2(u, v, 16, 4, 11u);
            const float blotch = fbm2(u, v, 4, 3, 23u);
            const uint32_t brick_id = hash_u32(static_cast<uint32_t>(row) * 131u +
                                               static_cast<uint32_t>(floor(fx / brick_w)) % 4u);
            const float tone = 0.8f + 0.25f * static_cast<float>(brick_id & 255u) / 255.0f;
            float height = saturate((edge - mortar) / bevel);
            height = smoothstep01(height) * (0.75f + 0.25f * grain);
            float3 stone = float3{0.42f, 0.4f, 0.42f} * (tone * (0.7f + 0.5f * grain) * (0.85f + 0.3f * blotch));
            stone = stone + float3{0.06f, 0.03f, 0.0f} * blotch;
            const float3 joint = {0.08f, 0.08f, 0.09f};
            const float cover = saturate((edge - mortar + 1.5f) / 3.0f);
            const float3 color = joint + (stone - joint) * cover;
            const uint32_t alpha = static_cast<uint32_t>(saturate(height) * 255.0f + 0.5f);
            texels[y * size + x] = (rgba(color) & 0x00FFFFFFu) | (alpha << 24u);
        }
    return texels;
}

class Displacement final : public Sample {
public:
    const char *name() const override { return "displacement"; }
    bool init(Gpu &gpu) override
    {
        static const char *const source = GEOMETRY_FRAME R"(
Texture2D bricks : register(t0);
SamplerState filter : register(s0);
struct Point { float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD; };
Point vs(In v) { Point p; p.position = v.position; p.normal = v.normal; p.uv = v.uv; return p; }
struct Levels { float edges[3] : SV_TessFactor; float inside : SV_InsideTessFactor; };
Levels levels(InputPatch<Point, 3> p) {
    Levels o;
    o.edges[0] = o.edges[1] = o.edges[2] = params.x;
    o.inside = params.x;
    return o;
}
[domain("tri")] [partitioning("integer")] [outputtopology("triangle_cw")]
[outputcontrolpoints(3)] [patchconstantfunc("levels")]
Point hs(InputPatch<Point, 3> p, uint i : SV_OutputControlPointID) { return p[i]; }
struct Shaded { float4 position : SV_Position; float3 normal : NORMAL; float2 uv : TEXCOORD; float3 world_position : POSITION1; };
Shaded evaluate(float3 uvw, const OutputPatch<Point, 3> p, float strength) {
    Shaded o;
    float3 position = uvw.x * p[0].position + uvw.y * p[1].position + uvw.z * p[2].position;
    float3 normal = normalize(uvw.x * p[0].normal + uvw.y * p[1].normal + uvw.z * p[2].normal);
    o.uv = uvw.x * p[0].uv + uvw.y * p[1].uv + uvw.z * p[2].uv;
    position += normal * (max(bricks.SampleLevel(filter, o.uv, 0).a, 0.2) * strength);
    float4 w = mul(float4(position, 1), world);
    o.position = mul(w, view_projection);
    o.normal = mul(float4(normal, 0), world).xyz;
    o.world_position = w.xyz;
    return o;
}
[domain("tri")]
Shaded ds_flat(Levels c, float3 uvw : SV_DomainLocation, const OutputPatch<Point, 3> p) { return evaluate(uvw, p, 0); }
[domain("tri")]
Shaded ds_displaced(Levels c, float3 uvw : SV_DomainLocation, const OutputPatch<Point, 3> p) {
    return evaluate(uvw, p, params.y);
}
float4 ps(Shaded p) : SV_Target {
    float3 n = normalize(p.normal);
    if (params.z > 0) {
        // The displaced surface's own facet normal, so the raised stones shade.
        float3 facet = normalize(cross(ddy(p.world_position), ddx(p.world_position)));
        n = normalize(dot(facet, n) < 0 ? -facet : facet);
    }
    float3 to_light = light.xyz - p.world_position;
    float3 l = normalize(to_light);
    float falloff = saturate(1.6 - length(to_light) * 0.16);
    float diffuse = max(dot(n, l), 0.0) * falloff;
    return float4(bricks.Sample(filter, p.uv).rgb * (0.03 + 1.3 * diffuse), 1);
}
)";
        root_ = frame_root(gpu, true, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc =
            gpu.graphics(root_, gpu.vs("displacement_vs", source), gpu.ps("displacement_ps", source));
        desc.HS = gpu.hs("displacement_hs", source);
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
        desc.DS = gpu.ds("displacement_ds_flat", source, "ds_flat");
        flat_ = gpu.pipeline(desc);
        desc.DS = gpu.ds("displacement_ds_displaced", source, "ds_displaced");
        displaced_ = gpu.pipeline(desc);
        Mesh mesh = torus_mesh(1.0f, 0.5f, 48, 24);
        torus_ = gpu.mesh(mesh);
        mesh.release();
        const uint32_t size = 256;
        uint32_t *texels = brick_texels(size);
        bricks_ = gpu.texture(texels, size, size);
        release_memory(texels);
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        gpu.begin_frame(0.0f, 0.0f, 0.0f);
        const float half = static_cast<float>(gpu.width) * 0.5f;
        const float3 eye = {0, 1.75f, -2.45f};
        Frame frame = {};
        frame.world = rotate_y(time * 0.15f);
        frame.view_projection = look_at(eye, {0, -0.1f, -0.2f}, {0, 1, 0}) *
                                perspective(radians(45), half / static_cast<float>(gpu.height), 0.1f, 100);
        frame.light = {0, 3.0f, 0.6f, 0};
        frame.eye = {eye.x, eye.y, eye.z, 0};
        auto *l = gpu.list;
        l->SetGraphicsRootSignature(root_);
        l->SetGraphicsRootDescriptorTable(1, bricks_.srv.gpu);
        for (int side = 0; side < 2; side++) {
            frame.params = {8.0f, 0.1f, static_cast<float>(side), 0};
            l->SetGraphicsRootConstantBufferView(0, gpu.constants(&frame, sizeof frame));
            gpu.viewport(half * static_cast<float>(side), 0, half, static_cast<float>(gpu.height));
            l->SetPipelineState(side == 0 ? flat_ : displaced_);
            draw_patches(l, torus_);
        }
    }
    void release() override
    {
        safe_release(root_);
        safe_release(flat_);
        safe_release(displaced_);
        torus_.release();
        bricks_.release();
    }

private:
    ID3D12RootSignature *root_ = nullptr;
    ID3D12PipelineState *flat_ = nullptr;
    ID3D12PipelineState *displaced_ = nullptr;
    DrawMesh             torus_;
    Texture              bricks_;
};

Mesh rock_mesh(uint32_t seed)
{
    Mesh mesh = sphere_mesh(1.0f, 20, 14);
    for (uint32_t i = 0; i < mesh.vertex_count; i++) {
        Vertex &v = mesh.vertices[i];
        const float3 n = v.normal;
        const float bump = noise3(n * 1.6f, seed) * 0.5f + noise3(n * 3.5f, seed + 7) * 0.25f;
        v.position = mul3(n, {1.0f, 0.8f, 0.9f}) * (0.75f + 0.5f * bump);
        v.normal = {0, 0, 0};
        v.color = {1, 1, 1};
    }
    for (uint32_t t = 0; t + 2 < mesh.index_count; t += 3) {
        Vertex &a = mesh.vertices[mesh.indices[t]], &b = mesh.vertices[mesh.indices[t + 1]],
               &c = mesh.vertices[mesh.indices[t + 2]];
        const float3 face = cross(b.position - a.position, c.position - a.position);
        a.normal = a.normal + face;
        b.normal = b.normal + face;
        c.normal = c.normal + face;
    }
    for (uint32_t i = 0; i < mesh.vertex_count; i++) {
        float3 sum = mesh.vertices[i].normal;
        for (uint32_t j = 0; j < mesh.vertex_count; j++)
            if (j != i && length(mesh.vertices[j].position - mesh.vertices[i].position) < 1e-5f)
                sum = sum + mesh.vertices[j].normal;
        mesh.vertices[i].uv = {mesh.vertices[i].uv.x * 2.0f, mesh.vertices[i].uv.y};
        mesh.vertices[i].color = normalize(sum);
    }
    for (uint32_t i = 0; i < mesh.vertex_count; i++) {
        mesh.vertices[i].normal = mesh.vertices[i].color;
        mesh.vertices[i].color = {1, 1, 1};
    }
    const Vertex &top = mesh.vertices[mesh.vertex_count / 2];
    if (dot(top.normal, top.position) < 0.0f)
        for (uint32_t i = 0; i < mesh.vertex_count; i++) mesh.vertices[i].normal = -mesh.vertices[i].normal;
    return mesh;
}

uint32_t *rock_texels(uint32_t size, uint32_t layers)
{
    uint32_t *texels = static_cast<uint32_t *>(allocate(size * size * layers * 4));
    const float3 tints[5] = {{0.55f, 0.36f, 0.24f}, {0.2f, 0.19f, 0.18f}, {0.5f, 0.52f, 0.55f},
                             {0.88f, 0.87f, 0.85f}, {0.36f, 0.42f, 0.5f}};
    for (uint32_t layer = 0; layer < layers; layer++) {
        const float3 tint = tints[layer % 5];
        for (uint32_t y = 0; y < size; y++)
            for (uint32_t x = 0; x < size; x++) {
                const float u = static_cast<float>(x) / static_cast<float>(size);
                const float v = static_cast<float>(y) / static_cast<float>(size);
                const float broad = fbm2(u, v, 4, 4, 100u + layer * 17u);
                const float grain = fbm2(u, v, 32, 2, 200u + layer * 17u);
                const float speck = static_cast<float>(hash_u32(x * 7919u + y * 104729u + layer) & 255u) / 255.0f;
                const float vein = abs(fbm2(u, v, 6, 3, 300u + layer) - 0.5f) < 0.02f ? 0.55f : 1.0f;
                const float shade = (0.55f + 0.6f * broad) * (0.8f + 0.3f * grain) * (0.85f + 0.3f * speck) * vein;
                texels[(layer * size + y) * size + x] = rgba(tint * shade);
            }
    }
    return texels;
}

struct RockInstance {
    float3   position;
    float3   rotation;
    float    scale;
    uint32_t layer;
};

class Instancing final : public Sample {
public:
    const char *name() const override { return "instancing"; }
    bool init(Gpu &gpu) override
    {
        static const char *const source = R"(
cbuffer Frame : register(b0) { row_major float4x4 view_projection; float4 eye; float4 params; };
Texture2DArray rocks : register(t0);
SamplerState filter : register(s0);
struct In {
    float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD; float3 color : COLOR;
    float3 instance_position : INSTPOS; float3 instance_rotation : INSTROT;
    float instance_scale : INSTSCALE; uint instance_layer : INSTLAYER;
};
struct Out { float4 position : SV_Position; float3 normal : NORMAL; float3 uv : TEXCOORD; float3 world_position : POSITION1; };
float3x3 rotation(float3 a) {
    float sx = sin(a.x), cx = cos(a.x), sy = sin(a.y), cy = cos(a.y), sz = sin(a.z), cz = cos(a.z);
    float3x3 rx = float3x3(1, 0, 0, 0, cx, sx, 0, -sx, cx);
    float3x3 ry = float3x3(cy, 0, -sy, 0, 1, 0, sy, 0, cy);
    float3x3 rz = float3x3(cz, sz, 0, -sz, cz, 0, 0, 0, 1);
    return mul(mul(rx, ry), rz);
}
Out vs(In v) {
    // The whole field turns: each rock's offset is rotated with it.
    float3x3 r = rotation(v.instance_rotation + float3(0, params.x, 0));
    float3 w = mul(v.position * v.instance_scale + v.instance_position, r);
    Out o;
    o.position = mul(float4(w, 1), view_projection);
    o.normal = mul(v.normal, r);
    o.uv = float3(v.uv, v.instance_layer);
    o.world_position = w;
    return o;
}
float4 ps(Out p) : SV_Target {
    float4 color = rocks.Sample(filter, p.uv);
    float3 n = normalize(p.normal);
    float3 l = normalize(eye.xyz - p.world_position);
    float3 r = reflect(-l, n);
    float diffuse = max(dot(n, l), 0.0);
    float specular = pow(max(dot(r, l), 0.0), 32.0) * 0.375;
    return float4(color.rgb * (0.1 + diffuse) + specular, 1);
}
)";
        static const D3D12_INPUT_ELEMENT_DESC layout[8] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"INSTPOS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
            {"INSTROT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 12, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
            {"INSTSCALE", 0, DXGI_FORMAT_R32_FLOAT, 1, 24, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
            {"INSTLAYER", 0, DXGI_FORMAT_R32_UINT, 1, 28, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        };
        root_ = frame_root(gpu, true);
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc =
            gpu.graphics(root_, gpu.vs("instancing_vs", source), gpu.ps("instancing_ps", source));
        desc.InputLayout = {layout, 8};
        pipeline_ = gpu.pipeline(desc);
        Mesh mesh = rock_mesh(5u);
        rock_ = gpu.mesh(mesh);
        mesh.release();
        RockInstance *instances = static_cast<RockInstance *>(allocate(sizeof(RockInstance) * instance_count));
        Random random(2048u);
        for (uint32_t i = 0; i < instance_count; i++) {
            RockInstance &rock = instances[i];
            rock.rotation = {pi * random.unit(), pi * random.unit(), pi * random.unit()};
            const float theta = 2.0f * pi * random.unit();
            const float radius = 1.5f + 6.0f * sqrt(random.unit());
            rock.position = {cos(theta) * radius, random.range(-0.3f, 0.3f), sin(theta) * radius};
            rock.scale = (1.0f + random.unit() * 2.0f) * 0.09f;
            rock.layer = random.next() % layers;
        }
        const uint32_t bytes = sizeof(RockInstance) * instance_count;
        instances_ = gpu.buffer(instances, bytes, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        release_memory(instances);
        if (instances_ != nullptr) instance_view_ = {instances_->GetGPUVirtualAddress(), bytes, sizeof(RockInstance)};
        const uint32_t size = 256;
        uint32_t *texels = rock_texels(size, layers);
        rocks_ = gpu.texture(texels, size, size, layers);
        release_memory(texels);
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        gpu.begin_frame(0.0f, 0.0f, 0.0f);
        const float3 eye = {0, 0.4f, -8.5f};
        Constants frame = {};
        frame.view_projection = look_at(eye, {0, 0, 0}, {0, 1, 0}) * perspective(radians(60), gpu.aspect(), 0.1f, 100);
        frame.eye = {eye.x, eye.y, eye.z, 0};
        frame.params = {time * 0.1f, 0, 0, 0};
        auto *l = gpu.list;
        l->SetGraphicsRootSignature(root_);
        l->SetPipelineState(pipeline_);
        l->SetGraphicsRootConstantBufferView(0, gpu.constants(&frame, sizeof frame));
        l->SetGraphicsRootDescriptorTable(1, rocks_.srv.gpu);
        const D3D12_VERTEX_BUFFER_VIEW views[2] = {rock_.vertex_view, instance_view_};
        l->IASetVertexBuffers(0, 2, views);
        l->IASetIndexBuffer(&rock_.index_view);
        l->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        l->DrawIndexedInstanced(rock_.index_count, instance_count, 0, 0, 0);
    }
    void release() override
    {
        safe_release(root_);
        safe_release(pipeline_);
        rock_.release();
        safe_release(instances_);
        rocks_.release();
    }

private:
    struct Constants {
        float4x4 view_projection;
        float4   eye;
        float4   params;
    };
    static constexpr uint32_t instance_count = 2048;
    static constexpr uint32_t layers = 5;
    ID3D12RootSignature     *root_ = nullptr;
    ID3D12PipelineState     *pipeline_ = nullptr;
    DrawMesh                 rock_;
    ID3D12Resource          *instances_ = nullptr;
    D3D12_VERTEX_BUFFER_VIEW instance_view_ = {};
    Texture                  rocks_;
};

void truss(MeshBuilder &b, float3 from, float3 to, float width, uint32_t bays, float3 color)
{
    const float4x4 frame = along(from, to - from);
    const float3 x = {frame.m[0][0], frame.m[0][1], frame.m[0][2]};
    const float3 z = {frame.m[2][0], frame.m[2][1], frame.m[2][2]};
    float3 corner[3];
    for (int k = 0; k < 3; k++) {
        const float a = radians(90.0f + 120.0f * static_cast<float>(k));
        corner[k] = (x * cos(a) + z * sin(a)) * (width * 0.577f);
    }
    const float thick = width * 0.08f, thin = width * 0.045f;
    for (int k = 0; k < 3; k++) b.cylinder(from + corner[k], to + corner[k], thick, thick, 4, color, false);
    const float3 step = (to - from) * (1.0f / static_cast<float>(bays));
    for (uint32_t i = 0; i <= bays; i++) {
        const float3 at = from + step * static_cast<float>(i);
        for (int k = 0; k < 3; k++) {
            b.cylinder(at + corner[k], at + corner[(k + 1) % 3], thin, thin, 3, color, false);
            if (i < bays) b.cylinder(at + corner[k], at + step + corner[(k + 1) % 3], thin, thin, 3, color, false);
        }
    }
}

Mesh probe_mesh()
{
    MeshBuilder b;
    b.begin(160000, 240000);
    const float3 white = {0.92f, 0.92f, 0.9f}, dark = {0.22f, 0.22f, 0.24f}, metal = {0.6f, 0.6f, 0.62f};
    const float3 gold = {0.85f, 0.65f, 0.25f}, brown = {0.55f, 0.4f, 0.2f};

    const float radius = 1.8f, depth = 0.35f, shell = 0.04f, k = depth / (radius * radius);
    float2 dish[27];
    for (int i = 0; i <= 12; i++) {
        const float r = radius * static_cast<float>(i) / 12.0f;
        dish[i] = {r, k * r * r};
    }
    dish[13] = {radius, k * radius * radius + shell};
    for (int i = 12; i >= 0; i--) {
        const float r = radius * static_cast<float>(i) / 12.0f;
        dish[26 - i] = {r, k * r * r + shell};
    }
    b.lathe(dish, 27, 64, translate({0, 0.08f, 0}), white);
    const float3 apex = {0, 1.25f, 0};
    for (int i = 0; i < 4; i++) {
        const float a = radians(45.0f + 90.0f * static_cast<float>(i));
        const float r = radius * 0.85f;
        b.cylinder({cos(a) * r, 0.1f + k * r * r, sin(a) * r}, apex, 0.015f, 0.015f, 4, metal, false);
    }
    b.cylinder(apex - float3{0, 0.05f, 0}, apex + float3{0, 0.08f, 0}, 0.16f, 0.12f, 16, gold);
    b.cylinder(apex, apex + float3{0, 0.3f, 0}, 0.03f, 0.02f, 8, metal);

    const float2 bus[4] = {{0, -0.65f}, {0.9f, -0.65f}, {0.9f, -0.15f}, {0, -0.15f}};
    b.lathe(bus, 4, 10, float4x4::identity(), dark, true);
    const float2 collar[4] = {{0, -0.15f}, {0.62f, -0.15f}, {0.62f, 0.1f}, {0, 0.1f}};
    b.lathe(collar, 4, 32, float4x4::identity(), white);
    b.cylinder({0, -0.4f, -0.84f}, {0, -0.4f, -0.9f}, 0.16f, 0.16f, 24, gold);
    for (int i = 0; i < 10; i += 3) {
        const float a = radians(36.0f * static_cast<float>(i) + 18.0f);
        b.box(rotate_y(-a) * translate({cos(a) * 0.87f, -0.4f, sin(a) * 0.87f}), {0.06f, 0.35f, 0.4f}, metal);
    }

    for (int i = 0; i < 6; i++) {
        const float a = radians(60.0f * static_cast<float>(i));
        const float a2 = radians(60.0f * static_cast<float>(i) + 60.0f);
        const float3 top = {cos(a) * 0.8f, -0.65f, sin(a) * 0.8f};
        const float3 foot = {cos(a + 0.5f) * 0.5f, -1.25f, sin(a + 0.5f) * 0.5f};
        b.cylinder(top, foot, 0.012f, 0.012f, 4, metal, false);
        b.cylinder({cos(a2) * 0.8f, -0.65f, sin(a2) * 0.8f}, foot, 0.012f, 0.012f, 4, metal, false);
    }

    const float3 science_root = {0.85f, -0.35f, 0.1f}, science_tip = {4.4f, -0.1f, 0.3f};
    truss(b, science_root, science_tip, 0.2f, 16, metal);
    b.box(translate(science_tip + float3{0.25f, -0.1f, 0}), {0.5f, 0.35f, 0.4f}, dark);
    b.cylinder(science_tip + float3{0.2f, -0.35f, -0.2f}, science_tip + float3{0.2f, -0.35f, 0.5f}, 0.2f, 0.2f, 20,
               dark);
    b.cylinder(science_tip + float3{0.2f, -0.35f, -0.25f}, science_tip + float3{0.2f, -0.35f, -0.2f}, 0.21f, 0.21f,
               20, brown);
    b.cylinder(science_tip + float3{0.55f, 0.1f, 0}, science_tip + float3{0.55f, 0.6f, 0}, 0.03f, 0.03f, 6, dark);

    const float3 rtg_root = {-0.85f, -0.4f, -0.1f}, rtg_tip = {-2.9f, -0.55f, -0.6f};
    truss(b, rtg_root, rtg_tip, 0.16f, 8, metal);
    const float3 rtg_axis = normalize(rtg_tip - rtg_root);
    for (int i = 0; i < 3; i++) {
        const float3 start = rtg_tip - rtg_axis * (0.95f - 0.4f * static_cast<float>(i));
        b.cylinder(start, start + rtg_axis * 0.36f, 0.13f, 0.13f, 16, dark);
        const float4x4 frame = along(start, rtg_axis);
        for (int f = 0; f < 6; f++)
            b.box(translate({0.17f, 0.18f, 0}) * rotate_y(radians(60.0f * static_cast<float>(f))) * frame,
                  {0.14f, 0.34f, 0.008f}, dark);
    }
    b.cylinder(rtg_tip + rtg_axis * 0.5f, rtg_tip - rtg_axis * 1.0f, 0.02f, 0.02f, 6, metal);

    b.cylinder({0.2f, -0.45f, 0.6f}, {-4.6f, 4.9f, 3.6f}, 0.02f, 0.012f, 6, brown, false);
    b.cylinder({-0.4f, -0.6f, 0.4f}, {-3.5f, -5.5f, 1.4f}, 0.01f, 0.008f, 5, brown, false);
    b.cylinder({-0.4f, -0.6f, 0.4f}, {-4.4f, -3.8f, 3.2f}, 0.01f, 0.008f, 5, brown, false);
    return b.mesh;
}

uint32_t *panel_texels(uint32_t size)
{
    uint32_t *texels = static_cast<uint32_t *>(allocate(size * size * 4));
    for (uint32_t y = 0; y < size; y++)
        for (uint32_t x = 0; x < size; x++) {
            const float u = static_cast<float>(x) / static_cast<float>(size);
            const float v = static_cast<float>(y) / static_cast<float>(size);
            const bool seam = (x % 32u) == 0u || (y % 64u) == 0u;
            const float shade = (0.82f + 0.18f * fbm2(u, v, 8, 4, 77u)) * (seam ? 0.6f : 1.0f);
            texels[y * size + x] = rgba({shade, shade, shade});
        }
    return texels;
}

const char *const probe_source = GEOMETRY_FRAME R"(
Texture2D panel : register(t0);
SamplerState filter : register(s0);
struct Out { float4 position : SV_Position; float3 normal : NORMAL; float3 color : COLOR; float2 uv : TEXCOORD; float3 world_position : POSITION1; };
Out vs(In v) {
    Out o;
    float4 w = mul(float4(v.position, 1), world);
    o.position = mul(w, view_projection);
    o.normal = mul(float4(v.normal, 0), world).xyz;
    o.color = v.color;
    o.uv = v.uv * float2(4, 1);
    o.world_position = w.xyz;
    return o;
}
float4 ps(Out p) : SV_Target {
    float3 color = panel.Sample(filter, p.uv).rgb * p.color;
    float3 n = normalize(p.normal);
    float3 l = normalize(light.xyz - p.world_position);
    float3 v = normalize(eye.xyz - p.world_position);
    float3 r = reflect(-l, n);
    float diffuse = max(dot(n, l), 0.0);
    float specular = pow(max(dot(r, v), 0.0), 16.0) * 0.75;
    return float4(color * (params.x + diffuse) + specular * params.y, 1);
}
)";

struct Probe {
    ID3D12RootSignature *root = nullptr;
    ID3D12PipelineState *pipeline = nullptr;
    DrawMesh             mesh;
    Texture              panel;

    void init(Gpu &gpu, const char *vs_name, const char *ps_name, uint32_t samples = 1)
    {
        root = frame_root(gpu, true);
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc =
            gpu.graphics(root, gpu.vs(vs_name, probe_source), gpu.ps(ps_name, probe_source));
        desc.SampleDesc.Count = samples;
        desc.RasterizerState.MultisampleEnable = samples > 1 ? TRUE : FALSE;
        pipeline = gpu.pipeline(desc);
        Mesh model = probe_mesh();
        mesh = gpu.mesh(model);
        model.release();
        const uint32_t size = 256;
        uint32_t *texels = panel_texels(size);
        panel = gpu.texture(texels, size, size);
        release_memory(texels);
    }
    void draw(Gpu &gpu, const Frame &frame)
    {
        auto *l = gpu.list;
        l->SetGraphicsRootSignature(root);
        l->SetPipelineState(pipeline);
        l->SetGraphicsRootConstantBufferView(0, gpu.constants(&frame, sizeof frame));
        l->SetGraphicsRootDescriptorTable(1, panel.srv.gpu);
        mesh.draw(l);
    }
    void release()
    {
        safe_release(root);
        safe_release(pipeline);
        mesh.release();
        panel.release();
    }
};

Frame probe_frame(const Gpu &gpu, float time, float3 eye, float3 at, float ambient)
{
    Frame frame = {};
    frame.world = rotate_y(radians(-30.0f) + time * 0.25f) * rotate_x(radians(-0.5f));
    frame.view_projection = look_at(eye, at, {0, 1, 0}) * perspective(radians(60), gpu.aspect(), 0.1f, 100);
    frame.light = {25.0f, 5.0f, -5.0f, 0};
    frame.eye = {eye.x, eye.y, eye.z, 0};
    frame.params = {ambient, 1.0f, 0, 0};
    return frame;
}

class MeshSample final : public Sample {
public:
    const char *name() const override { return "mesh"; }
    bool init(Gpu &gpu) override
    {
        probe_.init(gpu, "mesh_vs", "mesh_ps");
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        gpu.begin_frame(0.025f, 0.025f, 0.025f);
        probe_.draw(gpu, probe_frame(gpu, time, {0.6f, 1.2f, -6.2f}, {0.6f, 0.2f, 0}, 0.08f));
    }
    void release() override { probe_.release(); }

private:
    Probe probe_;
};

class Multisampling final : public Sample {
public:
    const char *name() const override { return "multisampling"; }
    bool init(Gpu &gpu) override
    {
        probe_.init(gpu, "multisampling_vs", "multisampling_ps", sample_count);
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS levels = {};
        levels.Format = Gpu::color_format;
        levels.SampleCount = sample_count;
        if (FAILED(gpu.device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &levels, sizeof levels)) ||
            levels.NumQualityLevels == 0)
            return false;
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = gpu.width;
        desc.Height = gpu.height;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = Gpu::color_format;
        desc.SampleDesc.Count = sample_count;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        D3D12_CLEAR_VALUE clear = {};
        clear.Format = Gpu::color_format;
        for (float &channel : clear.Color) channel = 1.0f;
        color_ = gpu.resource(desc, D3D12_RESOURCE_STATE_RENDER_TARGET, &clear);
        desc.Format = Gpu::depth_format;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        clear.Format = Gpu::depth_format;
        clear.DepthStencil = {1.0f, 0};
        depth_ = gpu.resource(desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear);
        if (color_ == nullptr || depth_ == nullptr) return false;
        color_view_ = gpu.rtvs(1).cpu;
        depth_view_ = gpu.dsvs(1).cpu;
        gpu.device->CreateRenderTargetView(color_, nullptr, color_view_);
        gpu.device->CreateDepthStencilView(depth_, nullptr, depth_view_);
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        auto *l = gpu.list;
        const float clear[4] = {1, 1, 1, 1};
        l->OMSetRenderTargets(1, &color_view_, FALSE, &depth_view_);
        l->ClearRenderTargetView(color_view_, clear, 0, nullptr);
        l->ClearDepthStencilView(depth_view_, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
        gpu.viewport(0, 0, static_cast<float>(gpu.width), static_cast<float>(gpu.height));
        probe_.draw(gpu, probe_frame(gpu, time, {1.2f, 0.9f, -5.0f}, {0.2f, 0.9f, 0}, 0.25f));
        D3D12_RESOURCE_BARRIER to_resolve[2] = {};
        for (D3D12_RESOURCE_BARRIER &b : to_resolve) {
            b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        }
        to_resolve[0].Transition = {color_, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                    D3D12_RESOURCE_STATE_RESOLVE_SOURCE};
        to_resolve[1].Transition = {gpu.color, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                                    D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_DEST};
        l->ResourceBarrier(2, to_resolve);
        l->ResolveSubresource(gpu.color, 0, color_, 0, Gpu::color_format);
        for (D3D12_RESOURCE_BARRIER &b : to_resolve) {
            const D3D12_RESOURCE_STATES before = b.Transition.StateBefore;
            b.Transition.StateBefore = b.Transition.StateAfter;
            b.Transition.StateAfter = before;
        }
        l->ResourceBarrier(2, to_resolve);
        gpu.bind_frame();
    }
    void release() override
    {
        probe_.release();
        safe_release(color_);
        safe_release(depth_);
    }

private:
    static constexpr UINT       sample_count = 4;
    Probe                       probe_;
    ID3D12Resource             *color_ = nullptr;
    ID3D12Resource             *depth_ = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE color_view_ = {};
    D3D12_CPU_DESCRIPTOR_HANDLE depth_view_ = {};
};

float ground_height(float x, float z)
{
    const float r2 = (x * x + z * z) / 9.0f;
    const float mound = r2 < 1.0f ? (1.0f - r2) * (1.0f - r2) * 0.9f : 0.0f;
    return mound + (noise3({x * 0.8f, z * 0.8f, 0.5f}, 9u) - 0.5f) * 0.3f;
}

Mesh glade_mesh()
{
    MeshBuilder b;
    b.begin(60000, 90000);
    const float3 grass = {0.08f, 0.12f, 0.05f}, bark = {0.1f, 0.05f, 0.035f}, wood = {0.12f, 0.06f, 0.03f};
    const float3 gold = {0.35f, 0.27f, 0.12f}, cap = {0.17f, 0.12f, 0.33f}, stem = {0.3f, 0.28f, 0.26f};

    const int cells = 48;
    const float extent = 14.0f, step = extent / static_cast<float>(cells);
    const uint32_t first = b.mesh.vertex_count;
    for (int j = 0; j <= cells; j++)
        for (int i = 0; i <= cells; i++) {
            const float x = -extent * 0.5f + step * static_cast<float>(i);
            const float z = -extent * 0.5f + step * static_cast<float>(j);
            const float e = 0.05f;
            const float3 n = normalize({ground_height(x - e, z) - ground_height(x + e, z), 2 * e,
                                        ground_height(x, z - e) - ground_height(x, z + e)});
            const float tone = 0.8f + 0.4f * noise3({x * 1.3f, z * 1.3f, 2.0f}, 3u);
            b.vertex({x, ground_height(x, z), z}, n, {static_cast<float>(i), static_cast<float>(j)}, grass * tone);
        }
    for (int j = 0; j < cells; j++)
        for (int i = 0; i < cells; i++) {
            const uint32_t a = first + static_cast<uint32_t>(j * (cells + 1) + i), c = a + cells + 1;
            b.quad(a, a + 1, c + 1, c);
        }

    const float3 trunk_base = {0.3f, ground_height(0.3f, 2.6f) - 0.2f, 2.6f};
    const float2 trunk[6] = {{0, 0}, {1.3f, 0}, {0.8f, 0.6f}, {0.6f, 1.6f}, {0.55f, 9.0f}, {0, 9.0f}};
    b.lathe(trunk, 6, 24, translate(trunk_base), bark);
    for (int i = 0; i < 6; i++) {
        const float a = radians(200.0f + 28.0f * static_cast<float>(i));
        const float reach = 2.2f + 0.6f * static_cast<float>(i % 2);
        const float3 end = {trunk_base.x + cos(a) * reach, 0, trunk_base.z + sin(a) * reach};
        const float3 tip = {end.x, ground_height(end.x, end.z) - 0.1f, end.z};
        b.cylinder(trunk_base + float3{cos(a) * 0.6f, 0.9f, sin(a) * 0.6f}, tip, 0.35f, 0.06f, 10, bark);
    }

    const float chest_y = ground_height(0, 0) + 0.3f;
    b.box(translate({0, chest_y, 0}), {1.4f, 0.6f, 0.8f}, wood);
    b.cylinder({-0.7f, chest_y + 0.3f, 0}, {0.7f, chest_y + 0.3f, 0}, 0.4f, 0.4f, 20, wood);
    for (int i = 0; i < 2; i++) {
        const float x = i == 0 ? -0.62f : 0.62f;
        b.box(translate({x, chest_y, 0}), {0.1f, 0.64f, 0.84f}, gold);
        b.cylinder({x - 0.05f, chest_y + 0.3f, 0}, {x + 0.05f, chest_y + 0.3f, 0}, 0.43f, 0.43f, 20, gold);
    }
    b.box(translate({0, chest_y + 0.02f, -0.41f}), {1.42f, 0.06f, 0.04f}, gold);
    b.box(translate({0, chest_y + 0.1f, -0.44f}), {0.16f, 0.2f, 0.06f}, {0.33f, 0.04f, 0.04f});
    b.ellipsoid({-0.3f, chest_y + 0.9f, 0.6f}, {0.2f, 0.4f, 0.2f}, 8, 6, {0.02f, 0.06f, 0.36f});

    Random random(77u);
    for (int i = 0; i < 14; i++) {
        const float a = random.range(0, 2 * pi), r = random.range(1.2f, 4.0f);
        const float x = cos(a) * r, z = sin(a) * r - 0.5f;
        const float y = ground_height(x, z), h = random.range(0.15f, 0.35f), w = random.range(0.12f, 0.22f);
        b.cylinder({x, y - 0.05f, z}, {x, y + h, z}, w * 0.25f, w * 0.2f, 8, stem);
        const float2 dome[6] = {{0, -0.1f}, {1.0f, -0.1f}, {0.95f, 0.15f}, {0.75f, 0.4f}, {0.4f, 0.55f}, {0, 0.6f}};
        b.lathe(dome, 6, 12, translate({x, y + h, z}), cap, false, {w, w, w});
    }
    for (int i = 0; i < 500; i++) {
        const float a = random.range(0, 2 * pi), r = 1.0f + 5.0f * sqrt(random.unit());
        const float x = cos(a) * r, z = sin(a) * r;
        if (z > 1.5f && x > -1.8f && x < 2.4f) continue;
        const float y = ground_height(x, z) - 0.02f;
        const float facing = random.range(0, 2 * pi), h = random.range(0.25f, 0.6f), w = 0.06f;
        const float3 side = {cos(facing) * w, 0, sin(facing) * w};
        const float3 lean = {random.range(-0.15f, 0.15f), h, random.range(-0.15f, 0.15f)};
        const float3 base = {x, y, z};
        const float3 n = normalize({-side.z, 0.3f, side.x});
        const float tone = random.range(0.8f, 1.4f);
        b.blade(base - side, base + side, base + lean, n, float3{0.1f, 0.17f, 0.06f} * tone);
    }
    return b.mesh;
}

class Pipelines final : public Sample {
public:
    const char *name() const override { return "pipelines"; }
    bool init(Gpu &gpu) override
    {
        static const char *const source = GEOMETRY_FRAME R"(
struct Out { float4 position : SV_Position; float3 normal : NORMAL; float3 color : COLOR; float3 view : TEXCOORD0; float3 to_light : TEXCOORD1; };
Out vs(In v) {
    Out o;
    float4 w = mul(float4(v.position, 1), world);
    o.position = mul(w, view_projection);
    o.normal = mul(float4(v.normal, 0), world).xyz;
    o.color = v.color;
    o.view = eye.xyz - w.xyz;
    o.to_light = light.xyz - w.xyz;
    return o;
}
float4 ps_phong(Out p) : SV_Target {
    float3 color = lerp(p.color, dot(float3(0.2126, 0.7152, 0.0722), p.color).xxx, 0.65);
    float3 n = normalize(p.normal), l = normalize(p.to_light), v = normalize(p.view);
    float3 r = reflect(-l, n);
    float3 diffuse = max(dot(n, l), 0.0) * color;
    float3 specular = pow(max(dot(r, v), 0.0), 32.0) * 0.35;
    return float4(color + diffuse * 1.75 + specular, 1);
}
float4 ps_toon(Out p) : SV_Target {
    float3 n = normalize(p.normal), l = normalize(p.to_light);
    float intensity = dot(n, l);
    float shade = 1.0;
    shade = intensity < 0.5 ? 0.75 : shade;
    shade = intensity < 0.35 ? 0.6 : shade;
    shade = intensity < 0.25 ? 0.5 : shade;
    shade = intensity < 0.1 ? 0.25 : shade;
    return float4(p.color * 3.0 * shade, 1);
}
float4 ps_wire(Out p) : SV_Target { return float4(p.color * 1.5, 1); }
)";
        root_ = frame_root(gpu, false);
        const D3D12_SHADER_BYTECODE vs = gpu.vs("pipelines_vs", source);
        pipelines_[0] = gpu.pipeline(gpu.graphics(root_, vs, gpu.ps("pipelines_phong_ps", source, "ps_phong")));
        pipelines_[1] = gpu.pipeline(gpu.graphics(root_, vs, gpu.ps("pipelines_toon_ps", source, "ps_toon")));
        D3D12_GRAPHICS_PIPELINE_STATE_DESC wire = gpu.graphics(root_, vs, gpu.ps("pipelines_wire_ps", source, "ps_wire"));
        wire.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
        pipelines_[2] = gpu.pipeline(wire);
        Mesh mesh = glade_mesh();
        glade_ = gpu.mesh(mesh);
        mesh.release();
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        gpu.begin_frame(0.025f, 0.025f, 0.025f);
        const float third = static_cast<float>(gpu.width) / 3.0f;
        const float yaw = radians(15.0f) + time * 0.2f, pitch = radians(25.0f), distance = 9.0f;
        const float3 at = {0, 1.0f, 0};
        const float3 eye = at + float3{-sin(yaw) * cos(pitch), sin(pitch), -cos(yaw) * cos(pitch)} * distance;
        Frame frame = {};
        frame.world = float4x4::identity();
        frame.view_projection = look_at(eye, at, {0, 1, 0}) *
                                perspective(radians(60), third / static_cast<float>(gpu.height), 0.1f, 100);
        frame.light = {2.0f, 8.0f, -4.0f, 0};
        frame.eye = {eye.x, eye.y, eye.z, 0};
        auto *l = gpu.list;
        l->SetGraphicsRootSignature(root_);
        l->SetGraphicsRootConstantBufferView(0, gpu.constants(&frame, sizeof frame));
        for (int i = 0; i < 3; i++) {
            gpu.viewport(third * static_cast<float>(i), 0, third, static_cast<float>(gpu.height));
            l->SetPipelineState(pipelines_[i]);
            glade_.draw(l);
        }
    }
    void release() override
    {
        safe_release(root_);
        for (ID3D12PipelineState *&p : pipelines_) safe_release(p);
        glade_.release();
    }

private:
    ID3D12RootSignature *root_ = nullptr;
    ID3D12PipelineState *pipelines_[3] = {};
    DrawMesh             glade_;
};

}

void register_geometry(Sample **all, uint32_t &made)
{
    all[made++] = new GeometryShader();
    all[made++] = new Tessellation();
    all[made++] = new Displacement();
    all[made++] = new Instancing();
    all[made++] = new MeshSample();
    all[made++] = new Pipelines();
    all[made++] = new Multisampling();
}

}
