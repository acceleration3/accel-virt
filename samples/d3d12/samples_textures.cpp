#include "framework.h"


namespace samples {

namespace {

float saturate(float x) { return clamp(x, 0.0f, 1.0f); }
float smooth01(float t) { return t * t * (3.0f - 2.0f * t); }
float3 mix3(float3 a, float3 b, float t) { return a + (b - a) * t; }

float atan2_approx(float y, float x)
{
    const float ax = abs(x), ay = abs(y);
    if (ax < 1e-12f && ay < 1e-12f) return 0.0f;
    const float a = min(ax, ay) / max(ax, ay);
    const float s = a * a;
    float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax) r = 0.5f * pi - r;
    if (x < 0) r = pi - r;
    if (y < 0) r = -r;
    return r;
}

uint32_t hash_u32(uint32_t x)
{
    x ^= x >> 16u;
    x *= 0x7feb352du;
    x ^= x >> 15u;
    x *= 0x846ca68bu;
    x ^= x >> 16u;
    return x;
}

float hash_unit(int x, int y, uint32_t seed)
{
    const uint32_t h = hash_u32(static_cast<uint32_t>(x) * 0x8da6b343u ^ static_cast<uint32_t>(y) * 0xd8163841u ^
                                hash_u32(seed + 0x9e3779b9u));
    return static_cast<float>(h & 0xFFFFFFu) / 16777216.0f;
}

float value_noise(float x, float y, uint32_t seed)
{
    const float fx0 = floor(x), fy0 = floor(y);
    const int ix = static_cast<int>(fx0), iy = static_cast<int>(fy0);
    const float tx = smooth01(x - fx0), ty = smooth01(y - fy0);
    const float a = hash_unit(ix, iy, seed), b = hash_unit(ix + 1, iy, seed);
    const float c = hash_unit(ix, iy + 1, seed), d = hash_unit(ix + 1, iy + 1, seed);
    return mix(mix(a, b, tx), mix(c, d, tx), ty);
}

float fbm(float x, float y, uint32_t seed, int octaves)
{
    float sum = 0.0f, amplitude = 0.5f, total = 0.0f;
    for (int i = 0; i < octaves; i++) {
        sum += value_noise(x, y, seed + static_cast<uint32_t>(i) * 101u) * amplitude;
        total += amplitude;
        amplitude *= 0.5f;
        x *= 2.03f;
        y *= 2.03f;
    }
    return sum / total;
}

uint32_t rgba(float3 c, float a = 1.0f)
{
    const uint32_t r = static_cast<uint32_t>(saturate(c.x) * 255.0f + 0.5f);
    const uint32_t g = static_cast<uint32_t>(saturate(c.y) * 255.0f + 0.5f);
    const uint32_t b = static_cast<uint32_t>(saturate(c.z) * 255.0f + 0.5f);
    const uint32_t w = static_cast<uint32_t>(saturate(a) * 255.0f + 0.5f);
    return (w << 24u) | (b << 16u) | (g << 8u) | r;
}

float3 transform_point(const float4x4 &m, float3 p)
{
    return {p.x * m.m[0][0] + p.y * m.m[1][0] + p.z * m.m[2][0] + m.m[3][0],
            p.x * m.m[0][1] + p.y * m.m[1][1] + p.z * m.m[2][1] + m.m[3][1],
            p.x * m.m[0][2] + p.y * m.m[1][2] + p.z * m.m[2][2] + m.m[3][2]};
}

float3 transform_direction(const float4x4 &m, float3 d)
{
    return {d.x * m.m[0][0] + d.y * m.m[1][0] + d.z * m.m[2][0], d.x * m.m[0][1] + d.y * m.m[1][1] + d.z * m.m[2][1],
            d.x * m.m[0][2] + d.y * m.m[1][2] + d.z * m.m[2][2]};
}

float4 transform4(const float4x4 &m, float3 p)
{
    float4 r;
    r.x = p.x * m.m[0][0] + p.y * m.m[1][0] + p.z * m.m[2][0] + m.m[3][0];
    r.y = p.x * m.m[0][1] + p.y * m.m[1][1] + p.z * m.m[2][1] + m.m[3][1];
    r.z = p.x * m.m[0][2] + p.y * m.m[1][2] + p.z * m.m[2][2] + m.m[3][2];
    r.w = p.x * m.m[0][3] + p.y * m.m[1][3] + p.z * m.m[2][3] + m.m[3][3];
    return r;
}

enum class Blend { alpha, additive, no_color };

void set_blend(D3D12_GRAPHICS_PIPELINE_STATE_DESC &desc, Blend mode)
{
    D3D12_RENDER_TARGET_BLEND_DESC &target = desc.BlendState.RenderTarget[0];
    target.BlendEnable = mode == Blend::no_color ? FALSE : TRUE;
    target.SrcBlend = mode == Blend::alpha ? D3D12_BLEND_SRC_ALPHA : D3D12_BLEND_ONE;
    target.DestBlend = mode == Blend::alpha ? D3D12_BLEND_INV_SRC_ALPHA : D3D12_BLEND_ONE;
    target.BlendOp = D3D12_BLEND_OP_ADD;
    target.SrcBlendAlpha = D3D12_BLEND_ONE;
    target.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    target.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    target.RenderTargetWriteMask = mode == Blend::no_color ? 0 : D3D12_COLOR_WRITE_ENABLE_ALL;
}

void set_depth(D3D12_GRAPHICS_PIPELINE_STATE_DESC &desc, bool test, bool write,
               D3D12_COMPARISON_FUNC func = D3D12_COMPARISON_FUNC_LESS)
{
    desc.DepthStencilState.DepthEnable = test ? TRUE : FALSE;
    desc.DepthStencilState.DepthWriteMask = write ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthStencilState.DepthFunc = func;
}

struct RenderTexture {
    ID3D12Resource             *texture = nullptr;
    ID3D12Resource             *depth_texture = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE target = {};
    D3D12_CPU_DESCRIPTOR_HANDLE depth = {};
    Descriptors                 view;
    uint32_t                    width = 0, height = 0;
    static constexpr DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;

    void create(Gpu &gpu, uint32_t w, uint32_t h)
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
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        D3D12_CLEAR_VALUE clear = {};
        clear.Format = format;
        clear.Color[3] = 1.0f;
        texture = gpu.resource(desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear);
        desc.Format = Gpu::depth_format;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        clear.Format = Gpu::depth_format;
        clear.DepthStencil = {1.0f, 0};
        depth_texture = gpu.resource(desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear);
        if (texture == nullptr || depth_texture == nullptr) return;
        target = gpu.rtvs(1).cpu;
        depth = gpu.dsvs(1).cpu;
        gpu.device->CreateRenderTargetView(texture, nullptr, target);
        gpu.device->CreateDepthStencilView(depth_texture, nullptr, depth);
        view = gpu.views(1);
        gpu.texture_view(texture, view.cpu);
    }
    void begin(Gpu &gpu) { gpu.barrier(texture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET); }
    void end(Gpu &gpu) { gpu.barrier(texture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE); }
    void release()
    {
        safe_release(texture);
        safe_release(depth_texture);
    }
};

void paint(Mesh &mesh, float3 color, float mask)
{
    for (uint32_t i = 0; i < mesh.vertex_count; i++) {
        mesh.vertices[i].color = color;
        mesh.vertices[i].uv = {mask, mesh.vertices[i].uv.y};
    }
}

struct MeshBuilder {
    Mesh     mesh;
    uint32_t vertex_capacity = 0, index_capacity = 0;

    void grow(uint32_t vertices, uint32_t indices)
    {
        if (mesh.vertex_count + vertices > vertex_capacity) {
            uint32_t capacity = vertex_capacity * 2u + vertices + 256u;
            Vertex *moved = static_cast<Vertex *>(allocate(sizeof(Vertex) * capacity));
            for (uint32_t i = 0; i < mesh.vertex_count; i++) moved[i] = mesh.vertices[i];
            release_memory(mesh.vertices);
            mesh.vertices = moved;
            vertex_capacity = capacity;
        }
        if (mesh.index_count + indices > index_capacity) {
            uint32_t capacity = index_capacity * 2u + indices + 256u;
            uint32_t *moved = static_cast<uint32_t *>(allocate(sizeof(uint32_t) * capacity));
            for (uint32_t i = 0; i < mesh.index_count; i++) moved[i] = mesh.indices[i];
            release_memory(mesh.indices);
            mesh.indices = moved;
            index_capacity = capacity;
        }
    }
    void add(const Mesh &part, const float4x4 &transform)
    {
        grow(part.vertex_count, part.index_count);
        const uint32_t base = mesh.vertex_count;
        for (uint32_t i = 0; i < part.vertex_count; i++) {
            Vertex v = part.vertices[i];
            v.position = transform_point(transform, v.position);
            v.normal = normalize(transform_direction(transform, v.normal));
            mesh.vertices[mesh.vertex_count++] = v;
        }
        for (uint32_t i = 0; i < part.index_count; i++) mesh.indices[mesh.index_count++] = base + part.indices[i];
    }
    void take(Mesh part, const float4x4 &transform)
    {
        add(part, transform);
        part.release();
    }
    Mesh finish()
    {
        const Mesh made = mesh;
        mesh = Mesh();
        vertex_capacity = index_capacity = 0;
        return made;
    }
};

Mesh lathe_mesh(const float2 *profile, uint32_t count, uint32_t sides, float3 color)
{
    Mesh mesh;
    mesh.vertices = static_cast<Vertex *>(allocate(sizeof(Vertex) * count * (sides + 1)));
    mesh.indices = static_cast<uint32_t *>(allocate(sizeof(uint32_t) * (count - 1) * sides * 6));
    auto segment_normal = [&](uint32_t i) -> float2 {
        const float dr = profile[i + 1].x - profile[i].x, dy = profile[i + 1].y - profile[i].y;
        const float l = sqrt(dr * dr + dy * dy);
        return l > 1e-6f ? float2{dy / l, -dr / l} : float2{0, 0};
    };
    for (uint32_t i = 0; i < count; i++) {
        const float2 before = i > 0 ? segment_normal(i - 1) : float2{0, 0};
        const float2 after = i + 1 < count ? segment_normal(i) : float2{0, 0};
        const bool hard_before = i > 0 && before.x == 0 && before.y == 0;
        const bool hard_after = i + 1 < count && after.x == 0 && after.y == 0;
        float2 n = hard_before ? after : hard_after ? before : float2{before.x + after.x, before.y + after.y};
        const float nl = sqrt(n.x * n.x + n.y * n.y);
        if (nl > 0) n = {n.x / nl, n.y / nl};
        for (uint32_t s = 0; s <= sides; s++) {
            const float a = static_cast<float>(s) / static_cast<float>(sides) * 2.0f * pi;
            const float c = cos(a), sn = sin(a);
            Vertex v;
            v.position = {profile[i].x * c, profile[i].y, profile[i].x * sn};
            v.normal = {n.x * c, n.y, n.x * sn};
            v.uv = {static_cast<float>(s) / static_cast<float>(sides), static_cast<float>(i) / static_cast<float>(count - 1)};
            v.color = color;
            mesh.vertices[mesh.vertex_count++] = v;
        }
    }
    for (uint32_t i = 0; i + 1 < count; i++)
        for (uint32_t s = 0; s < sides; s++) {
            const uint32_t a = i * (sides + 1) + s, b = a + sides + 1;
            uint32_t *out = mesh.indices + mesh.index_count;
            out[0] = a; out[1] = b; out[2] = b + 1;
            out[3] = a; out[4] = b + 1; out[5] = a + 1;
            mesh.index_count += 6;
        }
    return mesh;
}

float3 bezier(float3 p0, float3 p1, float3 p2, float3 p3, float t)
{
    const float u = 1.0f - t;
    return p0 * (u * u * u) + p1 * (3 * u * u * t) + p2 * (3 * u * t * t) + p3 * (t * t * t);
}

Mesh tube_mesh(float3 p0, float3 p1, float3 p2, float3 p3, float r0, float r1, uint32_t segments, uint32_t sides,
               float3 color)
{
    Mesh mesh;
    mesh.vertices = static_cast<Vertex *>(allocate(sizeof(Vertex) * (segments + 1) * (sides + 1)));
    mesh.indices = static_cast<uint32_t *>(allocate(sizeof(uint32_t) * segments * sides * 6));
    for (uint32_t i = 0; i <= segments; i++) {
        const float t = static_cast<float>(i) / static_cast<float>(segments);
        const float3 c = bezier(p0, p1, p2, p3, t);
        const float3 ahead = bezier(p0, p1, p2, p3, min(t + 0.01f, 1.0f)) - bezier(p0, p1, p2, p3, max(t - 0.01f, 0.0f));
        const float3 side = normalize(cross(normalize(ahead), {0, 0, 1}));
        const float3 other = {0, 0, 1};
        const float r = mix(r0, r1, t);
        for (uint32_t s = 0; s <= sides; s++) {
            const float a = static_cast<float>(s) / static_cast<float>(sides) * 2.0f * pi;
            const float3 n = side * cos(a) + other * sin(a);
            Vertex v;
            v.position = c + n * r;
            v.normal = n;
            v.uv = {static_cast<float>(s) / static_cast<float>(sides), t};
            v.color = color;
            mesh.vertices[mesh.vertex_count++] = v;
        }
    }
    for (uint32_t i = 0; i < segments; i++)
        for (uint32_t s = 0; s < sides; s++) {
            const uint32_t a = i * (sides + 1) + s, b = a + sides + 1;
            uint32_t *out = mesh.indices + mesh.index_count;
            out[0] = a; out[1] = b; out[2] = b + 1;
            out[3] = a; out[4] = b + 1; out[5] = a + 1;
            mesh.index_count += 6;
        }
    return mesh;
}

const uint8_t font5x7[95][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x5F, 0x00, 0x00}, {0x00, 0x07, 0x00, 0x07, 0x00},
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, {0x24, 0x2A, 0x7F, 0x2A, 0x12}, {0x23, 0x13, 0x08, 0x64, 0x62},
    {0x36, 0x49, 0x55, 0x22, 0x50}, {0x00, 0x05, 0x03, 0x00, 0x00}, {0x00, 0x1C, 0x22, 0x41, 0x00},
    {0x00, 0x41, 0x22, 0x1C, 0x00}, {0x08, 0x2A, 0x1C, 0x2A, 0x08}, {0x08, 0x08, 0x3E, 0x08, 0x08},
    {0x00, 0x50, 0x30, 0x00, 0x00}, {0x08, 0x08, 0x08, 0x08, 0x08}, {0x00, 0x60, 0x60, 0x00, 0x00},
    {0x20, 0x10, 0x08, 0x04, 0x02}, {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4B, 0x31}, {0x18, 0x14, 0x12, 0x7F, 0x10},
    {0x27, 0x45, 0x45, 0x45, 0x39}, {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E}, {0x00, 0x36, 0x36, 0x00, 0x00},
    {0x00, 0x56, 0x36, 0x00, 0x00}, {0x08, 0x14, 0x22, 0x41, 0x00}, {0x14, 0x14, 0x14, 0x14, 0x14},
    {0x00, 0x41, 0x22, 0x14, 0x08}, {0x02, 0x01, 0x51, 0x09, 0x06}, {0x32, 0x49, 0x79, 0x41, 0x3E},
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, {0x7F, 0x49, 0x49, 0x49, 0x36}, {0x3E, 0x41, 0x41, 0x41, 0x22},
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, {0x7F, 0x49, 0x49, 0x49, 0x41}, {0x7F, 0x09, 0x09, 0x09, 0x01},
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, {0x7F, 0x08, 0x08, 0x08, 0x7F}, {0x00, 0x41, 0x7F, 0x41, 0x00},
    {0x20, 0x40, 0x41, 0x3F, 0x01}, {0x7F, 0x08, 0x14, 0x22, 0x41}, {0x7F, 0x40, 0x40, 0x40, 0x40},
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, {0x7F, 0x04, 0x08, 0x10, 0x7F}, {0x3E, 0x41, 0x41, 0x41, 0x3E},
    {0x7F, 0x09, 0x09, 0x09, 0x06}, {0x3E, 0x41, 0x51, 0x21, 0x5E}, {0x7F, 0x09, 0x19, 0x29, 0x46},
    {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7F, 0x01, 0x01}, {0x3F, 0x40, 0x40, 0x40, 0x3F},
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, {0x3F, 0x40, 0x38, 0x40, 0x3F}, {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43}, {0x00, 0x7F, 0x41, 0x41, 0x00},
    {0x02, 0x04, 0x08, 0x10, 0x20}, {0x00, 0x41, 0x41, 0x7F, 0x00}, {0x04, 0x02, 0x01, 0x02, 0x04},
    {0x40, 0x40, 0x40, 0x40, 0x40}, {0x00, 0x01, 0x02, 0x04, 0x00}, {0x20, 0x54, 0x54, 0x54, 0x78},
    {0x7F, 0x48, 0x44, 0x44, 0x38}, {0x38, 0x44, 0x44, 0x44, 0x20}, {0x38, 0x44, 0x44, 0x48, 0x7F},
    {0x38, 0x54, 0x54, 0x54, 0x18}, {0x08, 0x7E, 0x09, 0x01, 0x02}, {0x0C, 0x52, 0x52, 0x52, 0x3E},
    {0x7F, 0x08, 0x04, 0x04, 0x78}, {0x00, 0x44, 0x7D, 0x40, 0x00}, {0x20, 0x40, 0x44, 0x3D, 0x00},
    {0x7F, 0x10, 0x28, 0x44, 0x00}, {0x00, 0x41, 0x7F, 0x40, 0x00}, {0x7C, 0x04, 0x18, 0x04, 0x78},
    {0x7C, 0x08, 0x04, 0x04, 0x78}, {0x38, 0x44, 0x44, 0x44, 0x38}, {0x7C, 0x14, 0x14, 0x14, 0x08},
    {0x08, 0x14, 0x14, 0x18, 0x7C}, {0x7C, 0x08, 0x04, 0x04, 0x08}, {0x48, 0x54, 0x54, 0x54, 0x20},
    {0x04, 0x3F, 0x44, 0x40, 0x20}, {0x3C, 0x40, 0x40, 0x20, 0x7C}, {0x1C, 0x20, 0x40, 0x20, 0x1C},
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, {0x44, 0x28, 0x10, 0x28, 0x44}, {0x0C, 0x50, 0x50, 0x50, 0x3C},
    {0x44, 0x64, 0x54, 0x4C, 0x44}, {0x00, 0x08, 0x36, 0x41, 0x00}, {0x00, 0x00, 0x7F, 0x00, 0x00},
    {0x00, 0x41, 0x36, 0x08, 0x00}, {0x08, 0x04, 0x08, 0x10, 0x08},
};

bool font_pixel(char c, int x, int y)
{
    if (c < 32 || c > 126 || x < 0 || x > 4 || y < 0 || y > 6) return false;
    return ((font5x7[c - 32][x] >> y) & 1u) != 0;
}

uint32_t text_length(const char *text)
{
    uint32_t n = 0;
    while (text[n] != '\0') n++;
    return n;
}

void append_signed(char *to, size_t capacity, float value)
{
    append_text(to, capacity, value <= -0.005f ? "-" : "+");
    append_number(to, capacity, abs(value), 2);
}

const char *const bitmap_text_source = R"(
cbuffer Screen : register(b0) { float4 screen; float4 color; };
Texture2D atlas : register(t0);
SamplerState point_filter : register(s0);
struct Out { float4 position : SV_Position; float2 uv : TEXCOORD; };
Out vs(float2 position : POSITION, float2 uv : TEXCOORD) {
    Out o;
    o.position = float4(position.x / screen.x * 2 - 1, 1 - position.y / screen.y * 2, 0, 1);
    o.uv = uv;
    return o;
}
float4 ps(Out p) : SV_Target {
    float a = atlas.SampleLevel(point_filter, p.uv, 0).a;
    return float4(color.rgb, a * color.a);
}
)";

class BitmapText {
public:
    enum Align { left, center, right };
    static constexpr uint32_t max_chars = 2048;

    bool init(Gpu &gpu)
    {
        static const D3D12_INPUT_ELEMENT_DESC layout[2] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
        root_ = gpu.root_signature(RootLayout().cbv(0).srv_table(0, 1, D3D12_SHADER_VISIBILITY_PIXEL)
                                       .sampler(0, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP));
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc =
            gpu.graphics(root_, gpu.vs("bitmaptext_vs", bitmap_text_source), gpu.ps("bitmaptext_ps", bitmap_text_source));
        desc.InputLayout = {layout, 2};
        set_blend(desc, Blend::alpha);
        set_depth(desc, false, false);
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        pipeline_ = gpu.pipeline(desc);
        uint32_t *texels = static_cast<uint32_t *>(allocate(96 * 48 * 4));
        for (int g = 0; g < 95; g++) {
            const int cx = (g % 16) * 6, cy = (g / 16) * 8;
            for (int y = 0; y < 7; y++)
                for (int x = 0; x < 5; x++)
                    if (font_pixel(static_cast<char>(32 + g), x, y)) texels[(cy + y) * 96 + cx + x] = 0xFFFFFFFFu;
        }
        for (int i = 0; i < 96 * 48; i++) texels[i] |= 0x00FFFFFFu;
        atlas_ = gpu.texture(texels, 96, 48);
        release_memory(texels);
        staging_ = static_cast<float *>(allocate(max_chars * 6 * 16));
        return !gpu.failed;
    }
    void clear() { count_ = 0; }
    void add(const char *text, float x, float y, Align align = left, float size = 2.0f)
    {
        const float width = static_cast<float>(text_length(text)) * 6.0f * size - size;
        if (align == center) x -= width * 0.5f;
        if (align == right) x -= width;
        x = floor(x + 0.5f);
        y = floor(y + 0.5f);
        for (; *text != '\0' && count_ < max_chars; text++, x += 6.0f * size) {
            const char c = *text;
            if (c <= 32 || c > 126) continue;
            const int g = c - 32;
            const float u0 = static_cast<float>((g % 16) * 6) / 96.0f, v0 = static_cast<float>((g / 16) * 8) / 48.0f;
            const float u1 = u0 + 5.0f / 96.0f, v1 = v0 + 7.0f / 48.0f;
            const float x1 = x + 5.0f * size, y1 = y + 7.0f * size;
            const float quad[24] = {x, y, u0, v0, x1, y, u1, v0, x1, y1, u1, v1,
                                    x, y, u0, v0, x1, y1, u1, v1, x, y1, u0, v1};
            float *to = staging_ + count_ * 24;
            for (int i = 0; i < 24; i++) to[i] = quad[i];
            count_++;
        }
    }
    void draw(Gpu &gpu, float4 color = {1, 1, 1, 1})
    {
        if (count_ == 0) return;
        auto *l = gpu.list;
        const float constants[8] = {static_cast<float>(gpu.width), static_cast<float>(gpu.height), 0, 0,
                                    color.x, color.y, color.z, color.w};
        const uint32_t bytes = count_ * 24 * 4;
        const Upload vertices = gpu.upload(bytes, 16);
        if (vertices.cpu == nullptr) return;
        copy_bytes(vertices.cpu, staging_, bytes);
        const D3D12_VERTEX_BUFFER_VIEW view = {vertices.gpu, bytes, 16};
        l->SetGraphicsRootSignature(root_);
        l->SetPipelineState(pipeline_);
        l->SetGraphicsRootConstantBufferView(0, gpu.constants(constants, sizeof constants));
        l->SetGraphicsRootDescriptorTable(1, atlas_.srv.gpu);
        l->IASetVertexBuffers(0, 1, &view);
        l->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        l->DrawInstanced(count_ * 6, 1, 0, 0);
    }
    void release()
    {
        safe_release(root_);
        safe_release(pipeline_);
        atlas_.release();
        release_memory(staging_);
        staging_ = nullptr;
        count_ = 0;
    }

private:
    ID3D12RootSignature *root_ = nullptr;
    ID3D12PipelineState *pipeline_ = nullptr;
    Texture              atlas_;
    float               *staging_ = nullptr;
    uint32_t             count_ = 0;
};

float2 project(const float4x4 &world_view_projection, float3 p, const Gpu &gpu)
{
    const float4 clip = transform4(world_view_projection, p);
    return {(clip.x / clip.w * 0.5f + 0.5f) * static_cast<float>(gpu.width),
            (0.5f - clip.y / clip.w * 0.5f) * static_cast<float>(gpu.height)};
}

const char *const texturearray_source = R"(
cbuffer Frame : register(b0) { row_major float4x4 view_projection; row_major float4x4 world[7]; float4 layer[7]; };
Texture2DArray layers : register(t0);
SamplerState filter : register(s0);
struct In { float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD; float3 color : COLOR; };
struct Out { float4 position : SV_Position; float3 uv : TEXCOORD; };
Out vs(In v, uint instance : SV_InstanceID) {
    Out o;
    o.position = mul(mul(float4(v.position, 1), world[instance]), view_projection);
    o.uv = float3(v.uv, layer[instance].x);
    return o;
}
float4 ps(Out p) : SV_Target { return layers.Sample(filter, p.uv); }
)";

uint32_t array_texel(int layer, int x, int y, int size)
{
    const float u = static_cast<float>(x) / static_cast<float>(size), v = static_cast<float>(y) / static_cast<float>(size);
    const uint32_t seed = static_cast<uint32_t>(layer) * 977u + 13u;
    switch (layer) {
    case 0: {
        const float n = fbm(u * 8, v * 8, seed, 5), fine = hash_unit(x, y, seed);
        const float c = 0.8f + 0.14f * n + 0.05f * fine;
        return rgba({c, c, c * 1.01f});
    }
    case 1: {
        const float n = fbm(u * 10, v * 10, seed, 5), patch = fbm(u * 5 + 3, v * 5, seed + 7, 4);
        const float3 grass = float3{0.32f, 0.36f, 0.09f} * (0.55f + 0.8f * n);
        const float3 dirt = float3{0.62f, 0.55f, 0.4f} * (0.7f + 0.5f * n);
        return rgba(mix3(grass, dirt, smooth01(saturate((patch - 0.6f) * 8.0f))));
    }
    case 2: {
        const float n = fbm(u * 12, v * 12, seed, 4);
        const float ripple = 0.5f + 0.5f * sin((u * 3.0f + v * 2.0f + n * 1.6f) * 2.0f * pi * 6.0f);
        return rgba(float3{0.2f, 0.55f, 0.82f} * (0.78f + 0.2f * n + 0.1f * ripple));
    }
    case 3: {
        const float n = fbm(u * 8, v * 8, seed, 5), m = fbm(u * 18, v * 18, seed + 3, 3);
        return rgba(mix3({0.3f, 0.31f, 0.28f}, {0.42f, 0.5f, 0.3f}, m) * (0.5f + 0.8f * n));
    }
    case 4: {
        const float n = fbm(u * 3, v * 3, seed, 5);
        const float band = 0.5f + 0.5f * sin((u * 1.5f + n * 3.0f) * 2.0f * pi);
        float3 c = mix3({0.42f, 0.38f, 0.36f}, {0.66f, 0.62f, 0.56f}, band);
        const float rust = smooth01(saturate((fbm(u * 4, v * 4, seed + 5, 3) - 0.62f) * 6.0f));
        return rgba(mix3(c, {0.55f, 0.3f, 0.18f}, rust * 0.8f));
    }
    case 5: {
        const float h = hash_unit(x / 2, y / 2, seed), cell = value_noise(u * 48, v * 48, seed + 1);
        return rgba(mix3({0.25f, 0.23f, 0.3f}, {0.55f, 0.5f, 0.62f}, h) * (0.5f + 0.7f * cell));
    }
    default: {
        const float n = fbm(u * 4, v * 4, seed, 5);
        const float ridge = 1.0f - saturate(abs(n - 0.5f) * 9.0f);
        float crack = ridge * ridge;
        crack *= crack;
        const float3 rock = float3{0.35f, 0.03f, 0.0f} * (0.5f + fbm(u * 16, v * 16, seed + 2, 3));
        return rgba(rock + float3{1.0f, 0.75f, 0.2f} * crack + float3{0.8f, 0.15f, 0.0f} * ridge * 0.6f);
    }
    }
}

struct TextureArrayFrame {
    float4x4 view_projection;
    float4x4 world[7];
    float4   layer[7];
};

class TextureArray final : public Sample {
public:
    const char *name() const override { return "texturearray"; }
    bool init(Gpu &gpu) override
    {
        root_ = gpu.root_signature(RootLayout().cbv(0).srv_table(0, 1, D3D12_SHADER_VISIBILITY_PIXEL).sampler(0));
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc =
            gpu.graphics(root_, gpu.vs("texturearray_vs", texturearray_source), gpu.ps("texturearray_ps", texturearray_source));
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        pipeline_ = gpu.pipeline(desc);
        Mesh mesh = plane_mesh(5.0f, 1, 1.0f);
        quad_ = gpu.mesh(mesh);
        mesh.release();
        const int size = 256;
        uint32_t *texels = static_cast<uint32_t *>(allocate(size * size * 4 * layers));
        for (int l = 0; l < static_cast<int>(layers); l++)
            for (int y = 0; y < size; y++)
                for (int x = 0; x < size; x++) texels[(l * size + y) * size + x] = array_texel(l, x, y, size);
        array_ = gpu.texture(texels, size, size, layers);
        release_memory(texels);
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        auto *l = gpu.list;
        gpu.begin_frame(0.02f, 0.02f, 0.02f);
        TextureArrayFrame frame = {};
        const float4x4 view = rotate_y(radians(35.0f) + time * 0.25f) * rotate_x(radians(15.0f)) * translate({0, 0.3f, 15.0f});
        frame.view_projection = view * perspective(radians(60), gpu.aspect(), 0.1f, 256);
        for (uint32_t i = 0; i < layers; i++) {
            frame.world[i] = rotate_x(radians(24.0f)) * translate({0, (3.0f - static_cast<float>(i)) * 1.5f, 0});
            frame.layer[i] = {static_cast<float>(i), 0, 0, 0};
        }
        l->SetGraphicsRootSignature(root_);
        l->SetPipelineState(pipeline_);
        l->SetGraphicsRootConstantBufferView(0, gpu.constants(&frame, sizeof frame));
        l->SetGraphicsRootDescriptorTable(1, array_.srv.gpu);
        quad_.draw(l, layers);
    }
    void release() override
    {
        safe_release(root_);
        safe_release(pipeline_);
        quad_.release();
        array_.release();
    }

private:
    static constexpr uint32_t layers = 7;
    ID3D12RootSignature *root_ = nullptr;
    ID3D12PipelineState *pipeline_ = nullptr;
    DrawMesh             quad_;
    Texture              array_;
};

float3 city_environment(float3 d)
{
    const float horizontal = sqrt(d.x * d.x + d.z * d.z);
    const float elevation = d.y / max(horizontal, 1e-4f);
    const float azimuth = (atan2_approx(d.z, d.x) + pi) / (2.0f * pi);
    const float lamps = 16.0f;
    const float lamp_u = fract(azimuth * lamps) - 0.5f;
    if (elevation < -0.03f) {
        const float t = 1.6f / -d.y;
        const float gx = d.x * t, gz = d.z * t;
        const float tx = gx * 0.7f, tz = gz * 0.7f;
        const float fx = fract(tx), fz = fract(tz);
        const bool grout = fx < 0.05f || fz < 0.05f;
        const float variation = hash_unit(static_cast<int>(floor(tx)), static_cast<int>(floor(tz)), 5);
        float3 c = grout ? float3{0.1f, 0.07f, 0.06f} : float3{0.36f, 0.24f, 0.2f} * (0.75f + 0.35f * variation);
        const float glow = 1.0f / (1.0f + abs(lamp_u) * abs(lamp_u) * 60.0f);
        const float distance = sqrt(gx * gx + gz * gz);
        c = c * (0.45f + 0.9f * glow * saturate(distance / 30.0f));
        const float haze = saturate(1.0f + elevation * 4.0f);
        return mix3(c, {0.45f, 0.28f, 0.22f}, haze * haze);
    }
    if (elevation < 0.06f) {
        const float rail = abs(elevation - 0.045f) < 0.005f ? 1.0f : 0.0f;
        float3 c = float3{0.42f, 0.3f, 0.28f} * (0.7f + 0.3f * hash_unit(static_cast<int>(azimuth * 400), 0, 9));
        c = mix3(c, {0.75f, 0.7f, 0.65f}, rail);
        const float dy = (elevation - 0.0f) * 40.0f, dx = lamp_u * 12.0f;
        const float spot = 1.0f / (1.0f + (dx * dx + dy * dy) * 3.0f);
        return c + float3{1.0f, 0.75f, 0.5f} * spot * 1.5f;
    }
    const float blocks = 36.0f;
    const int block = static_cast<int>(azimuth * blocks);
    const float h = hash_unit(block, 1, 17);
    const float height = hash_unit(block, 2, 17) < 0.25f ? 0.12f + 0.1f * h : 0.2f + 0.85f * h * h;
    if (elevation < height) {
        const float columns = fract(azimuth * blocks * 7.0f), rows = fract(elevation * 45.0f);
        const bool window = columns > 0.25f && columns < 0.75f && rows > 0.3f && rows < 0.8f;
        const bool lit = hash_unit(static_cast<int>(azimuth * blocks * 7.0f), static_cast<int>(elevation * 45.0f), 3) >
                         0.45f;
        const float wall = 0.05f + 0.04f * hash_unit(block, 3, 17);
        if (window && lit) return float3{1.0f, 0.93f, 0.78f} * (0.75f + 0.25f * h);
        return {wall, wall, wall * 1.1f};
    }
    const float star = hash_unit(static_cast<int>(d.x * 500), static_cast<int>(d.y * 500 + d.z * 700), 23);
    const float sky = saturate(1.0f - elevation * 1.5f);
    return float3{0.02f, 0.025f, 0.05f} + float3{0.05f, 0.05f, 0.07f} * sky + float3{1, 1, 1} * (star > 0.9993f ? 0.8f : 0.0f);
}

float3 cube_direction(int face, float u, float v)
{
    switch (face) {
    case 0: return {1, -v, -u};
    case 1: return {-1, -v, u};
    case 2: return {u, 1, v};
    case 3: return {u, -1, -v};
    case 4: return {u, -v, 1};
    default: return {-u, -v, -1};
    }
}

const char *const texturecubemap_source = R"(
cbuffer Frame : register(b0) {
    row_major float4x4 world; row_major float4x4 view_projection;
    float4 eye; float4 right; float4 up; float4 forward;   // w of right/up: the tangents of the half fovs
};
TextureCube environment : register(t0);
SamplerState filter : register(s0);
struct Screen { float4 position : SV_Position; float2 uv : TEXCOORD; };
Screen sky_vs(uint id : SV_VertexID) {
    Screen o;
    float2 uv = float2((id << 1) & 2, id & 2);
    o.position = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
    o.uv = uv;
    return o;
}
float4 sky_ps(Screen p) : SV_Target {
    float2 ndc = float2(p.uv.x * 2 - 1, 1 - p.uv.y * 2);
    float3 d = forward.xyz + right.xyz * ndc.x * right.w + up.xyz * ndc.y * up.w;
    return environment.Sample(filter, d);
}
struct In { float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD; float3 color : COLOR; };
struct Out { float4 position : SV_Position; float3 normal : NORMAL; float3 world_position : POSITION; };
Out vs(In v) {
    Out o;
    float4 w = mul(float4(v.position, 1), world);
    o.position = mul(w, view_projection);
    o.world_position = w.xyz;
    o.normal = mul(float4(v.normal, 0), world).xyz;
    return o;
}
float4 ps(Out p) : SV_Target {
    float3 n = normalize(p.normal);
    float3 i = normalize(p.world_position - eye.xyz);
    float3 r = reflect(i, n);
    float fresnel = 0.55 + 0.45 * pow(1 - saturate(dot(-i, n)), 3);
    return float4(environment.Sample(filter, r).rgb * fresnel, 1);
}
)";

struct CubemapFrame {
    float4x4 world;
    float4x4 view_projection;
    float4   eye, right, up, forward;
};

class TextureCubemap final : public Sample {
public:
    const char *name() const override { return "texturecubemap"; }
    bool init(Gpu &gpu) override
    {
        root_ = gpu.root_signature(RootLayout().cbv(0).srv_table(0, 1, D3D12_SHADER_VISIBILITY_PIXEL)
                                       .sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP));
        sphere_pipeline_ = gpu.pipeline(gpu.graphics(root_, gpu.vs("texturecubemap_vs", texturecubemap_source),
                                                     gpu.ps("texturecubemap_ps", texturecubemap_source)));
        D3D12_GRAPHICS_PIPELINE_STATE_DESC sky =
            gpu.graphics(root_, gpu.vs("texturecubemap_sky_vs", texturecubemap_source, "sky_vs"),
                         gpu.ps("texturecubemap_sky_ps", texturecubemap_source, "sky_ps"));
        sky.InputLayout = {nullptr, 0};
        set_depth(sky, false, false);
        sky_pipeline_ = gpu.pipeline(sky);
        Mesh mesh = sphere_mesh(1.0f, 96, 48);
        sphere_ = gpu.mesh(mesh);
        mesh.release();
        const int size = 512;
        uint32_t *texels = static_cast<uint32_t *>(allocate(size * size * 4 * 6));
        for (int face = 0; face < 6; face++)
            for (int y = 0; y < size; y++)
                for (int x = 0; x < size; x++) {
                    const float u = (static_cast<float>(x) + 0.5f) / size * 2.0f - 1.0f;
                    const float v = (static_cast<float>(y) + 0.5f) / size * 2.0f - 1.0f;
                    texels[(face * size + y) * size + x] = rgba(city_environment(normalize(cube_direction(face, u, v))));
                }
        cube_ = gpu.texture(texels, size, size, 6, true);
        release_memory(texels);
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        auto *l = gpu.list;
        gpu.begin_frame(0, 0, 0);
        const float a = time * 0.3f;
        const float3 eye = {sin(a) * 3.6f, 0.35f + 0.2f * sin(time * 0.5f), -cos(a) * 3.6f};
        const float3 at = {0, 0.1f, 0};
        const float fov = radians(60);
        CubemapFrame frame = {};
        frame.world = float4x4::identity();
        frame.view_projection = look_at(eye, at, {0, 1, 0}) * perspective(fov, gpu.aspect(), 0.01f, 256);
        const float3 forward = normalize(at - eye), right = normalize(cross({0, 1, 0}, forward)), up = cross(forward, right);
        const float ty = tan(fov * 0.5f);
        frame.eye = {eye.x, eye.y, eye.z, 1};
        frame.right = {right.x, right.y, right.z, ty * gpu.aspect()};
        frame.up = {up.x, up.y, up.z, ty};
        frame.forward = {forward.x, forward.y, forward.z, 0};
        l->SetGraphicsRootSignature(root_);
        l->SetGraphicsRootConstantBufferView(0, gpu.constants(&frame, sizeof frame));
        l->SetGraphicsRootDescriptorTable(1, cube_.srv.gpu);
        l->SetPipelineState(sky_pipeline_);
        l->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        l->DrawInstanced(3, 1, 0, 0);
        l->SetPipelineState(sphere_pipeline_);
        sphere_.draw(l);
    }
    void release() override
    {
        safe_release(root_);
        safe_release(sphere_pipeline_);
        safe_release(sky_pipeline_);
        sphere_.release();
        cube_.release();
    }

private:
    ID3D12RootSignature *root_ = nullptr;
    ID3D12PipelineState *sphere_pipeline_ = nullptr, *sky_pipeline_ = nullptr;
    DrawMesh             sphere_;
    Texture              cube_;
};

struct Segment {
    float ax, ay, bx, by;
};

uint32_t glyph_skeleton(char c, Segment *out)
{
    uint32_t n = 0;
    for (int y = 0; y < 7; y++)
        for (int x = 0; x < 5; x++) {
            if (!font_pixel(c, x, y)) continue;
            const float px = x + 0.5f, py = y + 0.5f;
            out[n++] = {px, py, px, py};
            if (font_pixel(c, x + 1, y)) out[n++] = {px, py, px + 1, py};
            if (font_pixel(c, x, y + 1)) out[n++] = {px, py, px, py + 1};
            if (font_pixel(c, x + 1, y + 1) && !font_pixel(c, x + 1, y) && !font_pixel(c, x, y + 1))
                out[n++] = {px, py, px + 1, py + 1};
            if (font_pixel(c, x - 1, y + 1) && !font_pixel(c, x - 1, y) && !font_pixel(c, x, y + 1))
                out[n++] = {px, py, px - 1, py + 1};
        }
    return n;
}

float segment_distance(float px, float py, const Segment &s)
{
    const float dx = s.bx - s.ax, dy = s.by - s.ay;
    const float l2 = dx * dx + dy * dy;
    const float t = l2 > 0 ? saturate(((px - s.ax) * dx + (py - s.ay) * dy) / l2) : 0.0f;
    const float ex = px - (s.ax + dx * t), ey = py - (s.ay + dy * t);
    return sqrt(ex * ex + ey * ey);
}

const char *const distancefield_source = R"(
cbuffer Frame : register(b0) { float4 place; float4 screen; float4 outline_color; float4 outline; };
Texture2D atlas : register(t0);
SamplerState filter : register(s0);
struct Out { float4 position : SV_Position; float2 uv : TEXCOORD; };
Out vs(float2 position : POSITION, float2 uv : TEXCOORD) {
    Out o;
    float2 pixel = position * place.x + place.yz;
    o.position = float4(pixel.x / screen.x * 2 - 1, 1 - pixel.y / screen.y * 2, 0, 1);
    o.uv = uv;
    return o;
}
float4 sdf_ps(Out p) : SV_Target {
    float distance = atlas.Sample(filter, p.uv).a;
    float smooth_width = fwidth(distance);
    float alpha = smoothstep(0.5 - smooth_width, 0.5 + smooth_width, distance);
    float3 rgb = alpha;
    if (outline.y > 0.0) {
        float w = 1.0 - outline.x;
        alpha = smoothstep(w - smooth_width, w + smooth_width, distance);
        rgb += lerp(alpha.xxx, outline_color.rgb, alpha);
    }
    return float4(saturate(rgb), alpha);
}
float4 bitmap_ps(Out p) : SV_Target { return float4(1, 1, 1, atlas.Sample(filter, p.uv).a); }
)";

struct DistanceFieldFrame {
    float4 place;
    float4 screen;
    float4 outline_color;
    float4 outline;
};

class DistanceFieldFonts final : public Sample {
public:
    const char *name() const override { return "distancefieldfonts"; }
    bool init(Gpu &gpu) override
    {
        static const D3D12_INPUT_ELEMENT_DESC layout[2] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
        root_ = gpu.root_signature(RootLayout().cbv(0).srv_table(0, 1, D3D12_SHADER_VISIBILITY_PIXEL)
                                       .sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP));
        const D3D12_SHADER_BYTECODE vs = gpu.vs("distancefieldfonts_vs", distancefield_source);
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc =
            gpu.graphics(root_, vs, gpu.ps("distancefieldfonts_sdf_ps", distancefield_source, "sdf_ps"));
        desc.InputLayout = {layout, 2};
        set_blend(desc, Blend::alpha);
        set_depth(desc, false, false);
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        sdf_pipeline_ = gpu.pipeline(desc);
        desc.PS = gpu.ps("distancefieldfonts_bitmap_ps", distancefield_source, "bitmap_ps");
        bitmap_pipeline_ = gpu.pipeline(desc);
        build_atlases(gpu);
        build_text(gpu, text);
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        auto *l = gpu.list;
        gpu.begin_frame(0.02f, 0.02f, 0.02f);
        l->SetGraphicsRootSignature(root_);
        l->IASetVertexBuffers(0, 1, &vertex_view_);
        l->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        const float w = static_cast<float>(gpu.width), h = static_cast<float>(gpu.height);
        const float base = w * 0.8f / text_width_;
        DistanceFieldFrame frame = {};
        frame.screen = {w, h, 0, 0};
        frame.outline_color = {1, 0, 0, 1};
        frame.outline = {0.6f, 1, 0, 0};
        const float top = base * (1.0f + 0.06f * sin(time * 1.3f));
        frame.place = {top, w * 0.5f - text_width_ * 0.5f * top, h * 0.27f - 3.5f * top, 0};
        l->SetGraphicsRootConstantBufferView(0, gpu.constants(&frame, sizeof frame));
        l->SetPipelineState(sdf_pipeline_);
        l->SetGraphicsRootDescriptorTable(1, sdf_.srv.gpu);
        l->DrawInstanced(vertex_count_, 1, 0, 0);
        frame.place = {base, w * 0.5f - text_width_ * 0.5f * base, h * 0.73f - 3.5f * base, 0};
        frame.outline = {0, 0, 0, 0};
        l->SetGraphicsRootConstantBufferView(0, gpu.constants(&frame, sizeof frame));
        l->SetPipelineState(bitmap_pipeline_);
        l->SetGraphicsRootDescriptorTable(1, bitmap_.srv.gpu);
        l->DrawInstanced(vertex_count_, 1, 0, 0);
    }
    void release() override
    {
        safe_release(root_);
        safe_release(sdf_pipeline_);
        safe_release(bitmap_pipeline_);
        sdf_.release();
        bitmap_.release();
        safe_release(vertices_);
    }

private:
    static constexpr int cell_w = 40, cell_h = 50, texels_per_unit = 5, atlas_w = 16 * cell_w, atlas_h = 6 * cell_h;
    static constexpr float pad = 1.5f, stroke = 0.72f, advance = 6.3f;
    const char *const text = "Direct3D";

    void build_atlases(Gpu &gpu)
    {
        uint32_t *sdf = static_cast<uint32_t *>(allocate(atlas_w * atlas_h * 4));
        uint32_t *bitmap = static_cast<uint32_t *>(allocate(atlas_w * atlas_h * 4));
        Segment *segments = static_cast<Segment *>(allocate(sizeof(Segment) * 5 * 7 * 5));
        for (int g = 0; g < 95; g++) {
            const uint32_t count = glyph_skeleton(static_cast<char>(32 + g), segments);
            const int ox = (g % 16) * cell_w, oy = (g / 16) * cell_h;
            for (int ty = 0; ty < cell_h; ty++)
                for (int tx = 0; tx < cell_w; tx++) {
                    const float gx = (tx + 0.5f) / texels_per_unit - pad, gy = (ty + 0.5f) / texels_per_unit - pad;
                    float d = 1e9f;
                    for (uint32_t s = 0; s < count; s++) d = min(d, segment_distance(gx, gy, segments[s]));
                    d -= stroke;
                    const int at = (oy + ty) * atlas_w + ox + tx;
                    sdf[at] = rgba({1, 1, 1}, 0.5f - d * 0.25f);
                    bitmap[at] = rgba({1, 1, 1}, 0.5f - d * 2.0f);
                }
        }
        release_memory(segments);
        sdf_ = gpu.texture(sdf, atlas_w, atlas_h);
        bitmap_ = gpu.texture(bitmap, atlas_w, atlas_h);
        release_memory(sdf);
        release_memory(bitmap);
    }
    void build_text(Gpu &gpu, const char *line)
    {
        const uint32_t n = text_length(line);
        float *quads = static_cast<float *>(allocate(n * 24 * 4));
        vertex_count_ = 0;
        for (uint32_t i = 0; i < n; i++) {
            const int g = line[i] - 32;
            const float x0 = i * advance - pad, y0 = -pad;
            const float x1 = x0 + static_cast<float>(cell_w) / texels_per_unit, y1 = y0 + static_cast<float>(cell_h) / texels_per_unit;
            const float u0 = static_cast<float>((g % 16) * cell_w) / atlas_w, v0 = static_cast<float>((g / 16) * cell_h) / atlas_h;
            const float u1 = u0 + static_cast<float>(cell_w) / atlas_w, v1 = v0 + static_cast<float>(cell_h) / atlas_h;
            const float quad[24] = {x0, y0, u0, v0, x1, y0, u1, v0, x1, y1, u1, v1,
                                    x0, y0, u0, v0, x1, y1, u1, v1, x0, y1, u0, v1};
            for (int k = 0; k < 24; k++) quads[vertex_count_ * 4 + k] = quad[k];
            vertex_count_ += 6;
        }
        text_width_ = n * advance - (advance - 5.0f);
        vertices_ = gpu.buffer(quads, vertex_count_ * 16, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        if (vertices_ != nullptr) vertex_view_ = {vertices_->GetGPUVirtualAddress(), vertex_count_ * 16, 16};
        release_memory(quads);
    }

    ID3D12RootSignature     *root_ = nullptr;
    ID3D12PipelineState     *sdf_pipeline_ = nullptr, *bitmap_pipeline_ = nullptr;
    Texture                  sdf_, bitmap_;
    ID3D12Resource          *vertices_ = nullptr;
    D3D12_VERTEX_BUFFER_VIEW vertex_view_ = {};
    uint32_t                 vertex_count_ = 0;
    float                    text_width_ = 1.0f;
};

uint32_t panel_texel(int x, int y, int size)
{
    const float u = (x + 0.5f) / size * 2.0f - 1.0f, v = (y + 0.5f) / size * 2.0f - 1.0f;
    const float r = sqrt(u * u + v * v);
    const float n = fbm(u * 6 + 10, v * 6 + 10, 31, 4);
    const float3 metal = float3{0.3f, 0.3f, 0.31f} * (0.7f + 0.5f * n);
    const float3 brass = float3{0.66f, 0.56f, 0.38f} * (0.75f + 0.4f * n);
    const float3 teal = float3{0.22f, 0.58f, 0.58f} * (0.7f + 0.5f * fbm(u * 10, v * 10, 37, 4));
    const float cu = abs(u) - 0.84f, cv = abs(v) - 0.84f;
    if (cu * cu + cv * cv < 0.0036f) return rgba({0.85f, 0.8f, 0.65f}, 0.9f);
    if (abs(u) > 0.97f || abs(v) > 0.97f) return rgba(metal * 0.55f, 0.1f);
    if (r > 0.86f) return rgba(metal, 0.25f);
    if (r > 0.84f || (r > 0.72f && r < 0.74f)) return rgba(brass * 0.5f, 0.2f);
    if (r > 0.72f) return rgba(brass, 0.7f);
    const bool spoke = (abs(u - v) < 0.08f || abs(u + v) < 0.08f) && r > 0.38f;
    if (spoke || (r > 0.3f && r < 0.4f)) return rgba(brass, 0.7f);
    return rgba(teal * (0.75f + 0.35f * (1.0f - r)), 0.35f);
}

const char *const textoverlay_source = R"(
cbuffer Frame : register(b0) { row_major float4x4 world; row_major float4x4 view_projection; float4 light; float4 eye; float4 screen; };
Texture2D image : register(t0);
SamplerState filter : register(s0);
struct In { float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD; float3 color : COLOR; };
struct Out { float4 position : SV_Position; float3 normal : NORMAL; float2 uv : TEXCOORD; float3 world_position : POSITION; };
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
    float4 color = image.Sample(filter, p.uv);
    float3 n = normalize(p.normal), l = normalize(light.xyz), v = normalize(eye.xyz - p.world_position);
    float3 r = reflect(-l, n);
    float3 lit = color.rgb * (0.3 + 0.9 * max(dot(n, l), 0.0)) + pow(max(dot(r, v), 0.0), 16.0) * color.a;
    return float4(lit, 1);
}
float hash2(int2 p) {
    uint h = uint(p.x) * 0x8da6b343u ^ uint(p.y) * 0xd8163841u;
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return float(h & 0xffffffu) / 16777216.0;
}
float noise(float2 x) {
    int2 i = int2(floor(x));
    float2 f = frac(x);
    f = f * f * (3 - 2 * f);
    return lerp(lerp(hash2(i), hash2(i + int2(1, 0)), f.x), lerp(hash2(i + int2(0, 1)), hash2(i + int2(1, 1)), f.x), f.y);
}
float clouds(float2 x) {
    float s = 0, a = 0.5;
    for (int k = 0; k < 5; k++) { s += noise(x) * a; x *= 2.07; a *= 0.5; }
    return s;
}
struct Screen { float4 position : SV_Position; float2 uv : TEXCOORD; };
Screen sky_vs(uint id : SV_VertexID) {
    Screen o;
    float2 uv = float2((id << 1) & 2, id & 2);
    o.position = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
    o.uv = uv;
    return o;
}
float4 sky_ps(Screen p) : SV_Target {
    float2 uv = p.uv;
    float horizon = 0.52;
    float3 top = float3(0.36, 0.42, 0.6), glow = float3(0.8, 0.83, 0.9), low = float3(0.3, 0.34, 0.52);
    float3 c = uv.y < horizon ? lerp(top, glow, pow(uv.y / horizon, 3)) : lerp(glow, low, sqrt((uv.y - horizon) / (1 - horizon)));
    float above = saturate((horizon - uv.y) / horizon);
    float depth = 1.0 / (above + 0.04);   // a cloud layer seen in perspective: thin near the horizon
    float cover = clouds(float2((uv.x - 0.5) * depth * 1.2 + 7.0, depth * 2.5));
    c = lerp(c, float3(0.9, 0.91, 0.95), smoothstep(0.45, 0.8, cover) * smoothstep(0.0, 0.08, above) * 0.75);
    float2 d = (uv - float2(0.095, 0.22)) * float2(screen.x / screen.y, 1);
    float sun = length(d);
    c += float3(0.45, 0.45, 0.5) * exp(-sun * 9) + smoothstep(0.009, 0.005, sun);
    return float4(c, 1);
}
)";

struct TextOverlayFrame {
    float4x4 world;
    float4x4 view_projection;
    float4   light, eye, screen;
};

class TextOverlay final : public Sample {
public:
    const char *name() const override { return "textoverlay"; }
    bool init(Gpu &gpu) override
    {
        root_ = gpu.root_signature(RootLayout().cbv(0).srv_table(0, 1, D3D12_SHADER_VISIBILITY_PIXEL).sampler(0));
        D3D12_GRAPHICS_PIPELINE_STATE_DESC cube =
            gpu.graphics(root_, gpu.vs("textoverlay_vs", textoverlay_source), gpu.ps("textoverlay_ps", textoverlay_source));
        cube.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        cube_pipeline_ = gpu.pipeline(cube);
        D3D12_GRAPHICS_PIPELINE_STATE_DESC sky =
            gpu.graphics(root_, gpu.vs("textoverlay_sky_vs", textoverlay_source, "sky_vs"),
                         gpu.ps("textoverlay_sky_ps", textoverlay_source, "sky_ps"));
        sky.InputLayout = {nullptr, 0};
        sky.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        set_depth(sky, false, false);
        sky_pipeline_ = gpu.pipeline(sky);
        Mesh mesh = cube_mesh(2.0f);
        cube_ = gpu.mesh(mesh);
        mesh.release();
        const int size = 256;
        uint32_t *texels = static_cast<uint32_t *>(allocate(size * size * 4));
        for (int y = 0; y < size; y++)
            for (int x = 0; x < size; x++) texels[y * size + x] = panel_texel(x, y, size);
        panel_ = gpu.texture(texels, size, size);
        release_memory(texels);
        text_.init(gpu);
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        auto *l = gpu.list;
        gpu.begin_frame(0, 0, 0);
        TextOverlayFrame frame = {};
        frame.world = rotate_y(time * 0.35f - 0.3f);
        const float4x4 view = rotate_x(radians(-25.0f)) * translate({0, 0, 4.5f});
        frame.view_projection = view * perspective(radians(60), gpu.aspect(), 0.1f, 256);
        frame.light = {-0.4f, 0.8f, -1.0f, 0};
        frame.eye = {0, 4.5f * sin(radians(25.0f)), -4.5f * cos(radians(25.0f)), 1};
        frame.screen = {static_cast<float>(gpu.width), static_cast<float>(gpu.height), 0, 0};
        l->SetGraphicsRootSignature(root_);
        l->SetGraphicsRootConstantBufferView(0, gpu.constants(&frame, sizeof frame));
        l->SetGraphicsRootDescriptorTable(1, panel_.srv.gpu);
        l->SetPipelineState(sky_pipeline_);
        l->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        l->DrawInstanced(3, 1, 0, 0);
        l->SetPipelineState(cube_pipeline_);
        cube_.draw(l);
        write_text(gpu, frame, view, time);
        text_.draw(gpu);
    }
    void release() override
    {
        safe_release(root_);
        safe_release(cube_pipeline_);
        safe_release(sky_pipeline_);
        cube_.release();
        panel_.release();
        text_.release();
    }

private:
    void write_text(Gpu &gpu, const TextOverlayFrame &frame, const float4x4 &view, float time)
    {
        const float w = static_cast<float>(gpu.width), h = static_cast<float>(gpu.height);
        text_.clear();
        text_.add("Direct3D 12 Example - Text overlay", 5, 5);
        char line[128] = {};
        copy_text(line, sizeof line, "t = ");
        append_number(line, sizeof line, time, 2);
        append_text(line, sizeof line, " s (dynamic vertex buffer, 5x7 font)");
        text_.add(line, 5, 25);
        text_.add("model view matrix", w - 5, 5, BitmapText::right);
        const float4x4 model_view = frame.world * view;
        for (int row = 0; row < 4; row++) {
            line[0] = '\0';
            for (int column = 0; column < 4; column++) {
                if (column > 0) append_text(line, sizeof line, " ");
                append_signed(line, sizeof line, model_view.m[column][row]);
            }
            text_.add(line, w - 5, 25.0f + row * 20.0f, BitmapText::right);
        }
        const float4x4 wvp = frame.world * frame.view_projection;
        for (int corner = 0; corner < 8; corner++) {
            const float3 p = {corner & 1 ? 1.0f : -1.0f, corner & 2 ? 1.0f : -1.0f, corner & 4 ? 1.0f : -1.0f};
            const float2 at = project(wvp, p, gpu);
            copy_text(line, sizeof line, p.x > 0 ? "+1/" : "-1/");
            append_text(line, sizeof line, p.y > 0 ? "+1/" : "-1/");
            append_text(line, sizeof line, p.z > 0 ? "+1" : "-1");
            text_.add(line, at.x, at.y + (p.y > 0 ? -20.0f : 6.0f), BitmapText::center);
        }
        const float2 middle = project(wvp, {0, 0, 0}, gpu);
        text_.add("Uniform cube", middle.x, middle.y - 7, BitmapText::center);
        text_.add("Glyphs come from a font table in code: no font file", 5, h - 40);
        text_.add("Every frame writes this text into the frame's upload ring", 5, h - 20);
    }

    ID3D12RootSignature *root_ = nullptr;
    ID3D12PipelineState *cube_pipeline_ = nullptr, *sky_pipeline_ = nullptr;
    DrawMesh             cube_;
    Texture              panel_;
    BitmapText           text_;
};

const GUID debug_object_name = {0x429b8c22, 0x9188, 0x4b0c, {0x87, 0x42, 0xac, 0xb0, 0xbf, 0x85, 0xc2, 0x00}};

const char *const debugmarker_source = R"(
cbuffer Frame : register(b0) { row_major float4x4 view_projection; float4 light; };
Texture2D glow : register(t0);
SamplerState filter : register(s0);
struct In { float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD; float3 color : COLOR; };
struct Out { float4 position : SV_Position; float3 normal : NORMAL; float3 color : COLOR; };
Out vs(In v) {
    Out o;
    o.position = mul(float4(v.position, 1), view_projection);
    o.normal = v.normal;
    o.color = v.color;
    return o;
}
float4 toon_ps(Out p) : SV_Target {
    float intensity = dot(normalize(p.normal), normalize(light.xyz));
    float shade = 1.0;
    shade = intensity < 0.5 ? 0.75 : shade;
    shade = intensity < 0.35 ? 0.6 : shade;
    shade = intensity < 0.25 ? 0.5 : shade;
    shade = intensity < 0.1 ? 0.25 : shade;
    return float4(p.color * 1.3 * shade, 1);
}
float4 color_ps(Out p) : SV_Target { return float4(p.color, 1); }
struct Screen { float4 position : SV_Position; float2 uv : TEXCOORD; };
Screen post_vs(uint id : SV_VertexID) {
    Screen o;
    float2 uv = float2((id << 1) & 2, id & 2);
    o.position = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
    o.uv = uv;
    return o;
}
float4 post_ps(Screen p) : SV_Target {
    const float2 o = float2(0.01, 0.01);
    float4 sum = glow.Sample(filter, p.uv + float2(-o.x, -o.y)) + 2 * glow.Sample(filter, p.uv + float2(0, -o.y)) +
                 glow.Sample(filter, p.uv + float2(o.x, -o.y)) + 2 * glow.Sample(filter, p.uv + float2(-o.x, 0)) +
                 4 * glow.Sample(filter, p.uv) + 2 * glow.Sample(filter, p.uv + float2(o.x, 0)) +
                 glow.Sample(filter, p.uv + float2(-o.x, o.y)) + 2 * glow.Sample(filter, p.uv + float2(0, o.y)) +
                 glow.Sample(filter, p.uv + float2(o.x, o.y));
    return float4(sum.rgb / 16.0 * 1.5, 1);
}
)";

float terrain_height(float x, float z)
{
    const float a = (x + 4) * (x + 4) + (z - 5) * (z - 5), b = (x - 6) * (x - 6) + (z - 4) * (z - 4);
    return 2.2f / (1.0f + a / 14.0f) + 1.0f / (1.0f + b / 10.0f) + 0.18f * sin(x * 0.7f) * cos(z * 0.6f) - 0.3f;
}

Mesh terrain_mesh()
{
    Mesh mesh = plane_mesh(26.0f, 52, 1.0f);
    for (uint32_t i = 0; i < mesh.vertex_count; i++) {
        Vertex &v = mesh.vertices[i];
        const float x = v.position.x, z = v.position.z, e = 0.05f;
        v.position.y = terrain_height(x, z);
        const float dx = (terrain_height(x + e, z) - terrain_height(x - e, z)) / (2 * e);
        const float dz = (terrain_height(x, z + e) - terrain_height(x, z - e)) / (2 * e);
        v.normal = normalize({-dx, 1, -dz});
        const float n = fbm(x * 0.4f + 50, z * 0.4f, 71, 3);
        v.color = mix3({0.13f, 0.3f, 0.08f}, {0.3f, 0.26f, 0.12f}, smooth01(saturate((n - 0.45f) * 4)));
    }
    return mesh;
}

Mesh recolored(Mesh mesh, float3 color)
{
    paint(mesh, color, 0);
    return mesh;
}

enum ScenePart { hill, rocks, tree, stems, blue_caps, red_caps, grass, chest_box, chest_fittings, part_count };

const char16_t *const part_events[part_count] = {u"Draw \"hill\"", u"Draw \"rocks\"", u"Draw \"tree\"",
                                                u"Draw \"mushroom stems\"", u"Draw \"blue mushroom caps\"",
                                                u"Draw \"red mushroom caps\"", u"Draw \"grass blades\"",
                                                u"Draw \"chest box\"", u"Draw \"chest fittings\""};
const char *const part_names[part_count] = {"hill vertices", "rocks vertices", "tree vertices", "mushroom stems vertices",
                                            "blue mushroom caps vertices", "red mushroom caps vertices",
                                            "grass blades vertices", "chest box vertices", "chest fittings vertices"};

void build_scene_part(ScenePart part, MeshBuilder &b)
{
    const float4x4 id = float4x4::identity();
    switch (part) {
    case hill: b.take(terrain_mesh(), id); break;
    case rocks: {
        const float3 at[6] = {{2.4f, 0, -1.5f}, {3.2f, 0, -0.8f}, {-1.5f, 0, -3.0f}, {5.0f, 0, 1.0f}, {-6, 0, 1}, {0.2f, 0, 4}};
        const float3 size[6] = {{0.9f, 0.6f, 0.8f}, {0.5f, 0.4f, 0.6f}, {0.7f, 0.45f, 0.6f}, {1.2f, 0.9f, 1.0f},
                                {1.0f, 0.8f, 1.3f}, {0.6f, 0.5f, 0.5f}};
        for (int i = 0; i < 6; i++)
            b.take(recolored(sphere_mesh(1.0f, 10, 6), {0.36f, 0.34f, 0.33f}),
                   rotate_y(i * 1.3f) * scale(size[i]) * translate({at[i].x, terrain_height(at[i].x, at[i].z), at[i].z}));
        break;
    }
    case tree: {
        const float2 trunk[] = {{1.1f, -0.5f}, {0.8f, 0.3f}, {0.6f, 1.5f}, {0.55f, 3.5f}, {0.45f, 5.0f}, {0.2f, 6.2f}, {0, 6.4f}};
        const float tx = -4.0f, tz = 3.5f, ty = terrain_height(tx, tz);
        b.take(lathe_mesh(trunk, 7, 16, {0.36f, 0.1f, 0.07f}), rotate_z(0.08f) * translate({tx, ty, tz}));
        b.take(tube_mesh({0, 3.2f, 0}, {1.0f, 3.6f, 0}, {1.8f, 4.4f, 0}, {2.2f, 5.4f, 0}, 0.3f, 0.1f, 12, 8,
                         {0.36f, 0.1f, 0.07f}),
               rotate_y(0.6f) * translate({tx, ty, tz}));
        b.take(tube_mesh({0, 4.0f, 0}, {-0.8f, 4.3f, 0}, {-1.4f, 5.0f, 0}, {-1.6f, 5.8f, 0}, 0.25f, 0.08f, 12, 8,
                         {0.36f, 0.1f, 0.07f}),
               rotate_y(-0.4f) * translate({tx, ty, tz}));
        break;
    }
    case stems:
    case blue_caps:
    case red_caps: {
        const float3 at[8] = {{-1.2f, 0, -1.0f}, {-0.6f, 0, -0.4f}, {-2.2f, 0, 0.2f}, {0.3f, 0, 0.8f},
                              {3.8f, 0, 0.6f}, {4.4f, 0, 1.4f}, {3.4f, 0, 2.2f}, {-2.8f, 0, -1.6f}};
        const float2 stem[] = {{0.09f, -0.05f}, {0.08f, 0.3f}, {0.06f, 0.5f}};
        const float2 cap[] = {{0, 0.42f}, {0.3f, 0.42f}, {0.33f, 0.48f}, {0.27f, 0.62f}, {0.14f, 0.71f}, {0, 0.73f}};
        for (int i = 0; i < 8; i++) {
            const bool red = i >= 4;
            const float s = 1.0f + 0.5f * hash_unit(i, 0, 3);
            const float4x4 place = scale({s, s, s}) * translate({at[i].x, terrain_height(at[i].x, at[i].z), at[i].z});
            if (part == stems) b.take(lathe_mesh(stem, 3, 10, {0.85f, 0.8f, 0.7f}), place);
            if (part == blue_caps && !red) b.take(lathe_mesh(cap, 6, 16, {0.5f, 0.38f, 0.9f}), place);
            if (part == red_caps && red) b.take(lathe_mesh(cap, 6, 16, {0.95f, 0.08f, 0.05f}), place);
        }
        break;
    }
    case grass: {
        Random random(77);
        const uint32_t blades = 700;
        Mesh mesh;
        mesh.vertices = static_cast<Vertex *>(allocate(sizeof(Vertex) * blades * 3));
        mesh.indices = static_cast<uint32_t *>(allocate(sizeof(uint32_t) * blades * 3));
        for (uint32_t i = 0; i < blades; i++) {
            const float x = random.range(-10, 10), z = random.range(-4, 9);
            const float y = terrain_height(x, z), a = random.range(0, 2 * pi), h = random.range(0.3f, 0.8f);
            const float w = 0.07f, lean = random.range(-0.25f, 0.25f);
            const float3 side = {cos(a) * w, 0, sin(a) * w};
            const float3 base = {x, y - 0.05f, z};
            const float3 tip = {x + lean, y + h, z + lean * 0.5f};
            const float3 n = normalize(cross(side, tip - base));
            const float3 color = float3{0.22f, 0.55f, 0.12f} * (0.8f + 0.4f * random.unit());
            mesh.vertices[mesh.vertex_count++] = Vertex{base - side, n, {0, 0}, color};
            mesh.vertices[mesh.vertex_count++] = Vertex{base + side, n, {1, 0}, color};
            mesh.vertices[mesh.vertex_count++] = Vertex{tip, n, {0.5f, 1}, color};
            for (int k = 0; k < 3; k++) mesh.indices[mesh.index_count] = mesh.index_count, mesh.index_count++;
        }
        b.take(mesh, id);
        break;
    }
    case chest_box:
    case chest_fittings: {
        const float cx = -0.4f, cz = -2.4f, cy = terrain_height(cx, cz) + 0.3f;
        const float4x4 place = rotate_y(0.35f) * translate({cx, cy, cz});
        if (part == chest_box) {
            b.take(recolored(cube_mesh(1.0f), {0.42f, 0.22f, 0.1f}), scale({1.4f, 0.7f, 0.9f}) * place);
            b.take(recolored(cube_mesh(1.0f), {0.38f, 0.2f, 0.09f}), scale({1.42f, 0.3f, 0.92f}) * translate({0, 0.48f, 0}) * place);
        } else {
            const float3 gold = {1.0f, 0.72f, 0.15f};
            for (int i = 0; i < 2; i++)
                b.take(recolored(cube_mesh(1.0f), gold),
                       scale({0.1f, 0.98f, 0.96f}) * translate({i == 0 ? -0.45f : 0.45f, 0.14f, 0}) * place);
            b.take(recolored(cube_mesh(1.0f), gold), scale({0.22f, 0.26f, 0.08f}) * translate({0, 0.3f, -0.47f}) * place);
            b.take(recolored(sphere_mesh(1.0f, 12, 8), {1.0f, 0.85f, 0.3f}),
                   scale({0.35f, 0.25f, 0.3f}) * translate({0.9f, -0.15f, -0.3f}) * place);
        }
        break;
    }
    default: break;
    }
}

struct DebugMarkerFrame {
    float4x4 view_projection;
    float4   light;
};

class DebugMarker final : public Sample {
public:
    const char *name() const override { return "debugmarker"; }
    bool init(Gpu &gpu) override
    {
        root_ = gpu.root_signature(RootLayout().cbv(0).srv_table(0, 1, D3D12_SHADER_VISIBILITY_PIXEL)
                                       .sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP));
        const D3D12_SHADER_BYTECODE vs = gpu.vs("debugmarker_vs", debugmarker_source);
        const D3D12_SHADER_BYTECODE color_ps = gpu.ps("debugmarker_color_ps", debugmarker_source, "color_ps");
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = gpu.graphics(root_, vs, color_ps);
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        desc.RTVFormats[0] = RenderTexture::format;
        glow_pipeline_ = gpu.pipeline(desc);
        desc.RTVFormats[0] = Gpu::color_format;
        desc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
        wire_pipeline_ = gpu.pipeline(desc);
        desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        desc.PS = gpu.ps("debugmarker_toon_ps", debugmarker_source, "toon_ps");
        toon_pipeline_ = gpu.pipeline(desc);
        D3D12_GRAPHICS_PIPELINE_STATE_DESC post =
            gpu.graphics(root_, gpu.vs("debugmarker_post_vs", debugmarker_source, "post_vs"),
                         gpu.ps("debugmarker_post_ps", debugmarker_source, "post_ps"));
        post.InputLayout = {nullptr, 0};
        post.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        set_blend(post, Blend::additive);
        set_depth(post, false, false);
        post_pipeline_ = gpu.pipeline(post);
        for (int p = 0; p < part_count; p++) {
            MeshBuilder builder;
            build_scene_part(static_cast<ScenePart>(p), builder);
            Mesh mesh = builder.finish();
            parts_[p] = gpu.mesh(mesh);
            mesh.release();
            name_object(parts_[p].vertices, part_names[p]);
        }
        glow_.create(gpu, gpu.width / 4, gpu.height / 4);
        name_object(glow_.texture, "Off-screen glow target");
        name_object(toon_pipeline_, "Toon pipeline state");
        name_object(wire_pipeline_, "Wireframe pipeline state");
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        auto *l = gpu.list;
        const float a = 0.3f * sin(time * 0.4f);
        const float3 eye = {sin(a) * -10.5f, 3.4f, -cos(a) * 10.5f};
        DebugMarkerFrame frame = {};
        frame.view_projection = look_at(eye, {0.3f, 1.2f, 1.0f}, {0, 1, 0}) * perspective(radians(60), gpu.aspect(), 0.1f, 256);
        frame.light = {0.5f, 1.0f, -0.6f, 0};
        l->SetGraphicsRootSignature(root_);
        l->SetGraphicsRootConstantBufferView(0, gpu.constants(&frame, sizeof frame));

        begin(l, u"Off-screen scene rendering");
        const float black[4] = {0, 0, 0, 1};
        glow_.begin(gpu);
        l->OMSetRenderTargets(1, &glow_.target, FALSE, &glow_.depth);
        l->ClearRenderTargetView(glow_.target, black, 0, nullptr);
        l->ClearDepthStencilView(glow_.depth, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        gpu.viewport(0, 0, static_cast<float>(glow_.width), static_cast<float>(glow_.height));
        l->SetPipelineState(glow_pipeline_);
        draw_part(l, red_caps);
        draw_part(l, chest_fittings);
        glow_.end(gpu);
        end(l);

        begin(l, u"Render scene");
        gpu.begin_frame(0.02f, 0.02f, 0.025f);
        const uint32_t half = gpu.width / 2;
        begin(l, u"Toon shading draw");
        set_scissor(l, 0, 0, half, gpu.height);
        l->SetPipelineState(toon_pipeline_);
        for (int p = 0; p < part_count; p++) draw_part(l, static_cast<ScenePart>(p));
        end(l);
        begin(l, u"Wireframe draw");
        set_scissor(l, half, 0, gpu.width, gpu.height);
        l->SetPipelineState(wire_pipeline_);
        for (int p = 0; p < part_count; p++) draw_part(l, static_cast<ScenePart>(p));
        end(l);
        begin(l, u"Apply post processing");
        set_scissor(l, 0, 0, gpu.width, gpu.height);
        l->SetPipelineState(post_pipeline_);
        l->SetGraphicsRootDescriptorTable(1, glow_.view.gpu);
        l->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        l->DrawInstanced(3, 1, 0, 0);
        end(l);
        end(l);
        static const char16_t done[] = u"Frame done";
        l->SetMarker(event_unicode, done, sizeof done);
    }
    void release() override
    {
        safe_release(root_);
        safe_release(glow_pipeline_);
        safe_release(wire_pipeline_);
        safe_release(toon_pipeline_);
        safe_release(post_pipeline_);
        for (DrawMesh &part : parts_) part.release();
        glow_.release();
    }

private:
    static constexpr UINT event_unicode = 0;

    void name_object(ID3D12Object *object, const char *label)
    {
        if (object != nullptr) object->SetPrivateData(debug_object_name, text_length(label), label);
    }
    void begin(ID3D12GraphicsCommandList *l, const char16_t *label)
    {
        uint32_t n = 0;
        while (label[n] != 0) n++;
        l->BeginEvent(event_unicode, label, (n + 1) * 2u);
    }
    void end(ID3D12GraphicsCommandList *l) { l->EndEvent(); }
    void set_scissor(ID3D12GraphicsCommandList *l, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)
    {
        const D3D12_RECT rect = {static_cast<LONG>(x0), static_cast<LONG>(y0), static_cast<LONG>(x1), static_cast<LONG>(y1)};
        l->RSSetScissorRects(1, &rect);
    }
    void draw_part(ID3D12GraphicsCommandList *l, ScenePart part)
    {
        begin(l, part_events[part]);
        parts_[part].draw(l);
        end(l);
    }

    ID3D12RootSignature *root_ = nullptr;
    ID3D12PipelineState *glow_pipeline_ = nullptr, *wire_pipeline_ = nullptr, *toon_pipeline_ = nullptr;
    ID3D12PipelineState *post_pipeline_ = nullptr;
    DrawMesh             parts_[part_count];
    RenderTexture        glow_;
};

Mesh teapot_mesh(float3 color)
{
    const float2 body[] = {{0, 0},       {1.4f, 0},     {1.4f, 0},     {1.72f, 0.12f}, {1.95f, 0.45f}, {2.02f, 0.8f},
                           {1.95f, 1.2f}, {1.8f, 1.6f},  {1.6f, 1.95f}, {1.45f, 2.2f},  {1.42f, 2.25f}, {1.42f, 2.25f},
                           {1.3f, 2.3f}, {1.0f, 2.42f}, {0.6f, 2.55f}, {0.25f, 2.62f}, {0.2f, 2.8f},   {0.35f, 2.95f},
                           {0.25f, 3.1f}, {0, 3.14f}};
    MeshBuilder b;
    b.take(lathe_mesh(body, sizeof body / sizeof body[0], 48, color), float4x4::identity());
    b.take(tube_mesh({1.6f, 0.75f, 0}, {2.6f, 0.9f, 0}, {2.5f, 1.9f, 0}, {3.3f, 2.35f, 0}, 0.48f, 0.16f, 24, 16, color),
           float4x4::identity());
    b.take(tube_mesh({-1.55f, 1.95f, 0}, {-3.1f, 2.2f, 0}, {-3.2f, 0.6f, 0}, {-1.85f, 0.65f, 0}, 0.17f, 0.17f, 24, 12, color),
           float4x4::identity());
    return b.finish();
}

const char *const occlusionquery_source = R"(
cbuffer Frame : register(b0) {
    row_major float4x4 world; row_major float4x4 view_projection;
    float4 light; float4 eye; float4 params;   // x: visible, y: alpha, z: flat colour
};
struct In { float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD; float3 color : COLOR; };
struct Out { float4 position : SV_Position; float3 normal : NORMAL; float3 color : COLOR; float3 world_position : POSITION; };
Out vs(In v) {
    Out o;
    float4 w = mul(float4(v.position, 1), world);
    o.position = mul(w, view_projection);
    o.world_position = w.xyz;
    o.normal = mul(float4(v.normal, 0), world).xyz;
    o.color = v.color;
    return o;
}
float4 ps(Out p) : SV_Target {
    if (params.z > 0) return float4(p.color, params.y);
    if (params.x <= 0) return float4(0.1, 0.1, 0.1, 1);
    float3 n = normalize(p.normal), l = normalize(light.xyz - p.world_position), v = normalize(eye.xyz - p.world_position);
    float3 r = reflect(-l, n);
    float3 diffuse = max(dot(n, l), 0.0) * p.color;
    float3 specular = pow(max(dot(r, v), 0.0), 8.0) * 0.75;
    return float4(diffuse + specular, 1);
}
)";

struct OcclusionFrame {
    float4x4 world;
    float4x4 view_projection;
    float4   light, eye, params;
};

class OcclusionQuery final : public Sample {
public:
    const char *name() const override { return "occlusionquery"; }
    bool init(Gpu &gpu) override
    {
        root_ = gpu.root_signature(RootLayout().cbv(0));
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc =
            gpu.graphics(root_, gpu.vs("occlusionquery_vs", occlusionquery_source), {nullptr, 0});
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        set_blend(desc, Blend::no_color);
        occluder_ = gpu.pipeline(desc);
        set_depth(desc, true, false);
        counted_ = gpu.pipeline(desc);
        desc = gpu.graphics(root_, desc.VS, gpu.ps("occlusionquery_ps", occlusionquery_source));
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        lit_ = gpu.pipeline(desc);
        set_blend(desc, Blend::alpha);
        set_depth(desc, true, false);
        plane_pipeline_ = gpu.pipeline(desc);
        Mesh teapot = teapot_mesh({1, 0, 0});
        teapot_ = gpu.mesh(teapot);
        teapot.release();
        Mesh sphere = sphere_mesh(2.2f, 48, 24);
        paint(sphere, {0, 1, 0}, 0);
        sphere_ = gpu.mesh(sphere);
        sphere.release();
        Mesh plane = plane_mesh(1.0f, 1, 1.0f);
        paint(plane, {0, 0, 1}, 0);
        plane_ = gpu.mesh(plane);
        plane.release();
        D3D12_QUERY_HEAP_DESC heap = {};
        heap.Type = D3D12_QUERY_HEAP_TYPE_OCCLUSION;
        heap.Count = 2;
        if (FAILED(gpu.device->CreateQueryHeap(&heap, __uuidof(ID3D12QueryHeap), reinterpret_cast<void **>(&queries_))))
            gpu.failed = true;
        D3D12_HEAP_PROPERTIES readback = {};
        readback.Type = D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC buffer = {};
        buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        buffer.Width = sizeof(UINT64) * 2 * Gpu::slots;
        buffer.Height = 1;
        buffer.DepthOrArraySize = 1;
        buffer.MipLevels = 1;
        buffer.SampleDesc.Count = 1;
        buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if (FAILED(gpu.device->CreateCommittedResource(&readback, D3D12_HEAP_FLAG_NONE, &buffer,
                                                       D3D12_RESOURCE_STATE_COPY_DEST, nullptr, __uuidof(ID3D12Resource),
                                                       reinterpret_cast<void **>(&results_))))
            gpu.failed = true;
        text_.init(gpu);
        counted_before_ = false;
        passed_[0] = passed_[1] = 0;
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        auto *l = gpu.list;
        if (counted_before_) read_counts(gpu);
        gpu.begin_frame(0.03f, 0.03f, 0.03f);
        const float a = radians(-52.0f) + 0.5f * sin(time * 0.5f);
        eye_ = {sin(a) * 19.0f, 1.0f, -cos(a) * 19.0f};
        view_projection_ = look_at(eye_, {0, 0, 0}, {0, 1, 0}) * perspective(radians(60), gpu.aspect(), 0.1f, 256);
        l->SetGraphicsRootSignature(root_);

        l->SetPipelineState(occluder_);
        draw(gpu, plane_, occluder_world(), {0, 0, 1, 0});
        l->SetPipelineState(counted_);
        l->BeginQuery(queries_, D3D12_QUERY_TYPE_OCCLUSION, 0);
        draw(gpu, teapot_, teapot_world(time), {1, 1, 0, 0});
        l->EndQuery(queries_, D3D12_QUERY_TYPE_OCCLUSION, 0);
        l->BeginQuery(queries_, D3D12_QUERY_TYPE_OCCLUSION, 1);
        draw(gpu, sphere_, sphere_world(), {1, 1, 0, 0});
        l->EndQuery(queries_, D3D12_QUERY_TYPE_OCCLUSION, 1);
        l->ResolveQueryData(queries_, D3D12_QUERY_TYPE_OCCLUSION, 0, 2, results_, sizeof(UINT64) * 2 * gpu.slot());
        counted_slot_ = gpu.slot();
        counted_before_ = true;
        const UINT64 *passed = passed_;

        l->ClearDepthStencilView(gpu.depth, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        l->SetPipelineState(lit_);
        draw(gpu, teapot_, teapot_world(time), {passed[0] > 0 ? 1.0f : 0.0f, 1, 0, 0});
        draw(gpu, sphere_, sphere_world(), {passed[1] > 0 ? 1.0f : 0.0f, 1, 0, 0});
        l->SetPipelineState(plane_pipeline_);
        draw(gpu, plane_, occluder_world(), {0, 0.5f, 1, 0});

        text_.clear();
        text_.add("Direct3D 12 Example - Occlusion queries", 5, 5);
        text_.add("Occlusion queries:", 5, 45);
        char line[96] = {};
        copy_text(line, sizeof line, "Teapot: ");
        append_number(line, sizeof line, static_cast<double>(passed[0]), 0);
        append_text(line, sizeof line, " samples passed");
        text_.add(line, 5, 65);
        copy_text(line, sizeof line, "Sphere: ");
        append_number(line, sizeof line, static_cast<double>(passed[1]), 0);
        append_text(line, sizeof line, " samples passed");
        text_.add(line, 5, 85);
        text_.draw(gpu);
    }
    void release() override
    {
        safe_release(root_);
        safe_release(occluder_);
        safe_release(counted_);
        safe_release(lit_);
        safe_release(plane_pipeline_);
        teapot_.release();
        sphere_.release();
        plane_.release();
        safe_release(queries_);
        safe_release(results_);
        text_.release();
    }

private:
    float4x4 occluder_world() const { return scale({16, 1, 16}) * rotate_x(-0.5f * pi); }
    float4x4 teapot_world(float time) const { return scale({1.3f, 1.3f, 1.3f}) * rotate_y(time * 0.5f) * translate({0, -2.0f, -8.0f}); }
    float4x4 sphere_world() const { return translate({-8.5f, 0.5f, 6.0f}); }
    void draw(Gpu &gpu, const DrawMesh &mesh, const float4x4 &world, float4 params)
    {
        OcclusionFrame frame = {};
        frame.world = world;
        frame.view_projection = view_projection_;
        frame.light = {-6, 8, -20, 0};
        frame.eye = {eye_.x, eye_.y, eye_.z, 1};
        frame.params = params;
        gpu.list->SetGraphicsRootConstantBufferView(0, gpu.constants(&frame, sizeof frame));
        mesh.draw(gpu.list);
    }
    void read_counts(Gpu &gpu)
    {
        gpu.wait(gpu.submitted());
        const D3D12_RANGE range = {sizeof(UINT64) * 2 * counted_slot_, sizeof(UINT64) * 2 * (counted_slot_ + 1)};
        const UINT64 *mapped = nullptr;
        if (FAILED(results_->Map(0, &range, reinterpret_cast<void **>(const_cast<UINT64 **>(&mapped))))) {
            gpu.failed = true;
            return;
        }
        passed_[0] = mapped[counted_slot_ * 2];
        passed_[1] = mapped[counted_slot_ * 2 + 1];
        const D3D12_RANGE none = {0, 0};
        results_->Unmap(0, &none);
    }

    ID3D12RootSignature *root_ = nullptr;
    ID3D12PipelineState *occluder_ = nullptr, *counted_ = nullptr, *lit_ = nullptr, *plane_pipeline_ = nullptr;
    DrawMesh             teapot_, sphere_, plane_;
    ID3D12QueryHeap     *queries_ = nullptr;
    ID3D12Resource      *results_ = nullptr;
    uint32_t             counted_slot_ = 0;
    bool                 counted_before_ = false;
    UINT64               passed_[2] = {};
    BitmapText           text_;
    float3               eye_ = {0, 0, 0};
    float4x4             view_projection_ = float4x4::identity();
};

Mesh ufo_mesh()
{
    MeshBuilder b;
    const float4x4 id = float4x4::identity();
    const float2 hull[] = {{0, -0.26f}, {0.35f, -0.25f}, {0.75f, -0.17f}, {1.1f, -0.06f}, {1.25f, 0.0f},
                           {1.2f, 0.05f}, {0.95f, 0.11f}, {0.6f, 0.17f}, {0, 0.18f}};
    Mesh part = lathe_mesh(hull, 9, 40, {1, 1, 1});
    paint(part, {1, 1, 1}, 1);
    b.take(part, id);
    const float2 dome[] = {{0.58f, 0.12f}, {0.56f, 0.3f}, {0.48f, 0.45f}, {0.34f, 0.56f}, {0.17f, 0.62f}, {0, 0.64f}};
    part = lathe_mesh(dome, 6, 32, {0.45f, 0.55f, 1.0f});
    paint(part, {0.45f, 0.55f, 1.0f}, 0);
    b.take(part, id);
    part = torus_mesh(1.21f, 0.035f, 40, 6);
    paint(part, {0.92f, 0.92f, 0.95f}, 0);
    b.take(part, translate({0, 0.03f, 0}));
    part = torus_mesh(0.62f, 0.04f, 32, 6);
    paint(part, {0.92f, 0.92f, 0.95f}, 0);
    b.take(part, translate({0, 0.15f, 0}));
    for (int i = 0; i < 10; i++) {
        const float a = static_cast<float>(i) / 10.0f * 2.0f * pi;
        part = sphere_mesh(0.085f, 10, 6);
        paint(part, {0.1f, 0.4f, 0.5f}, 0);
        b.take(part, translate({cos(a) * 0.86f, 0.12f, sin(a) * 0.86f}));
    }
    part = sphere_mesh(1.0f, 16, 8);
    paint(part, {1.0f, 0.65f, 0.15f}, 0);
    b.take(part, scale({0.35f, 0.07f, 0.35f}) * translate({0, -0.25f, 0}));
    part = tube_mesh({0, 0.6f, 0}, {0, 0.75f, 0}, {0.05f, 0.85f, 0}, {0.14f, 0.88f, 0}, 0.02f, 0.02f, 8, 6, {1, 1, 1});
    paint(part, {1, 1, 1}, 1);
    b.take(part, id);
    part = sphere_mesh(0.045f, 8, 6);
    paint(part, {1, 1, 1}, 1);
    b.take(part, translate({0.15f, 0.88f, 0}));
    return b.finish();
}

const char *const multithreading_source = R"(
cbuffer Object : register(b0) { row_major float4x4 world; row_major float4x4 view_projection; float4 tint; float4 eye; };
struct In { float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD; float3 color : COLOR; };
struct Out { float4 position : SV_Position; float3 normal : NORMAL; float3 color : COLOR; float3 world_position : POSITION; };
Out vs(In v) {
    Out o;
    float4 w = mul(float4(v.position, 1), world);
    o.position = mul(w, view_projection);
    o.world_position = w.xyz;
    o.normal = mul(float4(v.normal, 0), world).xyz;
    o.color = lerp(v.color, tint.rgb, v.uv.x);
    return o;
}
float4 ps(Out p) : SV_Target {
    float3 n = normalize(p.normal), l = normalize(float3(0.3, 1.0, -0.6)), v = normalize(eye.xyz - p.world_position);
    float3 r = reflect(-l, n);
    float3 diffuse = (0.15 + max(dot(n, l), 0.0)) * p.color;
    float3 specular = pow(max(dot(r, v), 0.0), 8.0) * 0.75;
    return float4(diffuse + specular, 1);
}
float hash3(int3 p) {
    uint h = uint(p.x) * 0x8da6b343u ^ uint(p.y) * 0xd8163841u ^ uint(p.z) * 0xcb1ab31fu;
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return float(h & 0xffffffu) / 16777216.0;
}
struct Sky { float4 position : SV_Position; float3 direction : DIRECTION; };
Sky star_vs(In v) {
    Sky o;
    o.position = mul(float4(v.position + eye.xyz, 1), view_projection).xyww;
    o.direction = v.position;
    return o;
}
float4 star_ps(Sky p) : SV_Target {
    float3 d = normalize(p.direction);
    float r = hash3(int3(floor(d * 350)));
    const float threshold = 0.993;
    float star = r >= threshold ? pow((r - threshold) / (1 - threshold), 6.0) : 0;
    float3 atmosphere = saturate(float3(0.1, 0.15, 0.4) * (-d.y - 0.08) * 14.0);
    return float4(star + atmosphere, 1);
}
)";

struct UfoConstants {
    float4x4 world;
    float4x4 view_projection;
    float4   tint;
    float4   eye;
};

struct UfoState {
    float3 position;
    float  rotation, delta, direction, speed, size;
    float4 color;
};


class Multithreading final : public Sample {
public:
    const char *name() const override { return "multithreading"; }
    bool init(Gpu &gpu) override
    {
        root_ = gpu.root_signature(RootLayout().cbv(0));
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc =
            gpu.graphics(root_, gpu.vs("multithreading_vs", multithreading_source),
                         gpu.ps("multithreading_ps", multithreading_source));
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        ufo_pipeline_ = gpu.pipeline(desc);
        desc.VS = gpu.vs("multithreading_star_vs", multithreading_source, "star_vs");
        desc.PS = gpu.ps("multithreading_star_ps", multithreading_source, "star_ps");
        set_depth(desc, true, false, D3D12_COMPARISON_FUNC_LESS_EQUAL);
        sky_pipeline_ = gpu.pipeline(desc);
        Mesh ufo = ufo_mesh();
        ufo_ = gpu.mesh(ufo);
        ufo.release();
        Mesh sky = sphere_mesh(100.0f, 48, 24);
        sky_ = gpu.mesh(sky);
        sky.release();
        Random random(4096);
        const float grid = 16.0f;
        for (uint32_t i = 0; i < objects; i++) {
            UfoState &u = ufos_[i];
            const float gx = static_cast<float>(i % 16u), gz = static_cast<float>(i / 16u);
            u.position = {(gx - grid / 2) * 3.0f + random.range(-1.5f, 1.5f), 0,
                          (gz - grid / 2) * 3.0f + random.range(-1.5f, 1.5f)};
            u.rotation = random.range(0, 360);
            u.delta = random.unit();
            u.direction = random.unit() < 0.5f ? 1.0f : -1.0f;
            u.speed = (2.0f + random.range(0, 4)) * u.direction;
            u.size = 0.75f + random.range(0, 0.5f);
            u.color = {random.unit(), random.unit(), random.unit(), 1};
        }
        for (uint32_t t = 0; t < threads; t++) {
            Worker &w = workers_[t];
            w.owner = this;
            w.first = t * (objects / threads);
            w.count = objects / threads;
            for (ID3D12CommandAllocator *&allocator : w.allocators)
                if (FAILED(gpu.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator),
                                                              reinterpret_cast<void **>(&allocator))))
                    gpu.failed = true;
            if (gpu.failed ||
                FAILED(gpu.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, w.allocators[0], nullptr,
                                                     __uuidof(ID3D12GraphicsCommandList),
                                                     reinterpret_cast<void **>(&w.list)))) {
                gpu.failed = true;
                continue;
            }
            w.list->Close();
            w.go = make_signal();
            w.done = make_signal();
            w.thread = start_thread(thread_entry, &w);
        }
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        auto *l = gpu.list;
        gpu_ = &gpu;
        time_ = time;
        const float a = radians(37.5f) + time * 0.04f;
        eye_ = {sin(a) * 30.0f, 1.2f, -cos(a) * 30.0f};
        view_projection_ = look_at(eye_, {0, 2.5f, 0}, {0, 1, 0}) * perspective(radians(60), gpu.aspect(), 0.1f, 256);
        for (Worker &w : workers_) {
            w.constants = gpu.upload(w.count * constant_stride);
            start(w);
        }
        gpu.begin_frame(0, 0, 0.2f);
        UfoConstants sky = {};
        sky.world = float4x4::identity();
        sky.view_projection = view_projection_;
        sky.eye = {eye_.x, eye_.y, eye_.z, 1};
        l->SetGraphicsRootSignature(root_);
        l->SetPipelineState(sky_pipeline_);
        l->SetGraphicsRootConstantBufferView(0, gpu.constants(&sky, sizeof sky));
        sky_.draw(l);
        ID3D12CommandList *lists[threads] = {};
        for (uint32_t t = 0; t < threads; t++) {
            join(workers_[t]);
            if (!workers_[t].recorded) gpu.failed = true;
            lists[t] = workers_[t].list;
        }
        if (!gpu.failed) gpu.queue_lists(lists, threads);
    }
    void release() override
    {
        for (Worker &w : workers_) {
            if (w.thread != nullptr) {
                w.quit = true;
                raise_signal(w.go);
                join_thread(w.thread);
            }
            release_signal(w.go);
            release_signal(w.done);
            safe_release(w.list);
            for (ID3D12CommandAllocator *&allocator : w.allocators) safe_release(allocator);
            w = Worker();
        }
        safe_release(root_);
        safe_release(ufo_pipeline_);
        safe_release(sky_pipeline_);
        ufo_.release();
        sky_.release();
    }

private:
    static constexpr uint32_t threads = 4, objects = 256, constant_stride = 256;
    struct Worker {
        ID3D12CommandAllocator    *allocators[Gpu::slots] = {};
        ID3D12GraphicsCommandList *list = nullptr;
        Upload                     constants;
        Multithreading            *owner = nullptr;
        uint32_t                   first = 0, count = 0;
        bool                       quit = false, recorded = false;
        Thread                    *thread = nullptr;
        Signal                    *go = nullptr, *done = nullptr;
    };

    static void thread_entry(void *argument)
    {
        Worker *w = static_cast<Worker *>(argument);
        for (;;) {
            wait_signal(w->go);
            if (w->quit) return;
            w->owner->record(*w);
            raise_signal(w->done);
        }
    }
    void start(Worker &w)
    {
        if (w.thread == nullptr) record(w);
        else raise_signal(w.go);
    }
    void join(Worker &w)
    {
        if (w.thread != nullptr) wait_signal(w.done);
    }

    void record(Worker &w)
    {
        Gpu &gpu = *gpu_;
        w.recorded = false;
        ID3D12CommandAllocator *allocator = w.allocators[gpu.slot()];
        if (w.constants.cpu == nullptr || FAILED(allocator->Reset()) || FAILED(w.list->Reset(allocator, ufo_pipeline_)))
            return;
        ID3D12GraphicsCommandList *l = w.list;
        l->SetGraphicsRootSignature(root_);
        l->OMSetRenderTargets(1, &gpu.target, FALSE, &gpu.depth);
        const D3D12_VIEWPORT view = {0, 0, static_cast<float>(gpu.width), static_cast<float>(gpu.height), 0, 1};
        l->RSSetViewports(1, &view);
        const D3D12_RECT scissor = {0, 0, static_cast<LONG>(gpu.width), static_cast<LONG>(gpu.height)};
        l->RSSetScissorRects(1, &scissor);
        l->IASetVertexBuffers(0, 1, &ufo_.vertex_view);
        l->IASetIndexBuffer(&ufo_.index_view);
        l->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        for (uint32_t i = w.first, k = 0; i < w.first + w.count; i++, k++) {
            const UfoState &u = ufos_[i];
            const float delta = fract(u.delta + 0.15f * time_);
            const float angle = radians(u.rotation + 2.5f * u.speed * time_) + delta * 2.0f * pi;
            const float3 position = {u.position.x, sin(delta * 2.0f * pi) * 2.5f, u.position.z};
            UfoConstants object = {};
            object.world = scale({u.size, u.size, u.size}) * rotate_y(angle * u.direction) *
                           rotate_x(-sin(delta * 2.0f * pi) * 0.25f * u.direction) * translate(position);
            object.view_projection = view_projection_;
            object.tint = u.color;
            object.eye = {eye_.x, eye_.y, eye_.z, 1};
            copy_bytes(static_cast<uint8_t *>(w.constants.cpu) + k * constant_stride, &object, sizeof object);
            l->SetGraphicsRootConstantBufferView(0, w.constants.gpu + k * constant_stride);
            l->DrawIndexedInstanced(ufo_.index_count, 1, 0, 0, 0);
        }
        w.recorded = SUCCEEDED(l->Close());
    }

    ID3D12RootSignature *root_ = nullptr;
    ID3D12PipelineState *ufo_pipeline_ = nullptr, *sky_pipeline_ = nullptr;
    DrawMesh             ufo_, sky_;
    UfoState             ufos_[objects] = {};
    Worker               workers_[threads];
    Gpu                 *gpu_ = nullptr;
    float                time_ = 0;
    float3               eye_ = {0, 0, 0};
    float4x4             view_projection_ = float4x4::identity();
};

}

void register_textures(Sample **all, uint32_t &made)
{
    all[made++] = new TextureArray();
    all[made++] = new TextureCubemap();
    all[made++] = new DistanceFieldFonts();
    all[made++] = new TextOverlay();
    all[made++] = new DebugMarker();
    all[made++] = new OcclusionQuery();
    all[made++] = new Multithreading();
}

}
