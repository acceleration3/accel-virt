// texturearray, texturecubemap, distancefieldfonts, textoverlay, debugmarker, occlusionquery, multithreading
#include "framework.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace samples {

namespace {

// ---------------------------------------------------------------- small math and noise

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

// ---------------------------------------------------------------- GL state helpers

#define MESH_INPUTS                                  \
    "layout(location = 0) in vec3 position;\n"      \
    "layout(location = 1) in vec3 normal;\n"        \
    "layout(location = 2) in vec2 uv;\n"            \
    "layout(location = 3) in vec3 color;\n"

// The fullscreen triangle's corners, as the D3D11 samples' vertex-ID shaders make them.
#define SCREEN_VERTEX R"(
out vec2 v_uv;
void main() {
    int id = gl_VertexID;
    v_uv = vec2((id << 1) & 2, id & 2);
    gl_Position = vec4(v_uv * vec2(2, -2) + vec2(-1, 1), 0, 1);
}
)"

GLuint dynamic_buffer(uint32_t bytes)
{
    GLuint made = 0;
    glCreateBuffers(1, &made);
    glNamedBufferData(made, bytes, nullptr, GL_DYNAMIC_DRAW);
    return made;
}

// Direct3D's Map(WRITE_DISCARD): the old contents given up, the new written through a mapping.
bool write_buffer(GLuint buffer, const void *data, uint32_t bytes)
{
    void *mapped = glMapNamedBufferRange(buffer, 0, bytes, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    if (mapped == nullptr) return false;
    const uint8_t *from = static_cast<const uint8_t *>(data);
    uint8_t *to = static_cast<uint8_t *>(mapped);
    for (uint32_t i = 0; i < bytes; i++) to[i] = from[i];
    return glUnmapNamedBuffer(buffer) == GL_TRUE;
}

void raster_state(bool wireframe, bool cull, bool scissor)
{
    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
    if (cull) glEnable(GL_CULL_FACE);
    else glDisable(GL_CULL_FACE);
    if (scissor) glEnable(GL_SCISSOR_TEST);
    else glDisable(GL_SCISSOR_TEST);
}

enum class Blend { none, alpha, additive, no_color };

void blend_state(Blend mode)
{
    glColorMask(mode == Blend::no_color ? GL_FALSE : GL_TRUE, mode == Blend::no_color ? GL_FALSE : GL_TRUE,
                mode == Blend::no_color ? GL_FALSE : GL_TRUE, mode == Blend::no_color ? GL_FALSE : GL_TRUE);
    if (mode == Blend::none || mode == Blend::no_color) {
        glDisable(GL_BLEND);
        return;
    }
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(mode == Blend::alpha ? GL_SRC_ALPHA : GL_ONE, mode == Blend::alpha ? GL_ONE_MINUS_SRC_ALPHA : GL_ONE,
                        GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

void depth_state(bool test, bool write, GLenum func = GL_LESS)
{
    if (test) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);
    glDepthMask(write ? GL_TRUE : GL_FALSE);
    glDepthFunc(func);
}

// A colour target and a depth buffer, for off-screen passes.
struct RenderTexture {
    GLuint   texture = 0;
    GLuint   depth = 0;
    GLuint   target = 0;
    uint32_t width = 0, height = 0;

    void create(Gpu &gpu, uint32_t w, uint32_t h)
    {
        width = w;
        height = h;
        texture = gpu.render_texture(GL_RGBA8, w, h);
        depth = gpu.render_texture(GL_DEPTH24_STENCIL8, w, h);
        target = gpu.framebuffer(&texture, 1, depth);
    }
    void release()
    {
        glDeleteFramebuffers(1, &target);
        glDeleteTextures(1, &texture);
        glDeleteTextures(1, &depth);
        target = texture = depth = 0;
    }
};

// ---------------------------------------------------------------- mesh building

void paint(Mesh &mesh, float3 color, float mask)
{
    for (uint32_t i = 0; i < mesh.vertex_count; i++) {
        mesh.vertices[i].color = color;
        mesh.vertices[i].uv = {mask, mesh.vertices[i].uv.y};
    }
}

// Many parts, transformed, into one mesh.
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
    // Adds the part and frees it.
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

// A surface of revolution about y. `profile` holds (radius, height) from the bottom up; a
// repeated point makes a hard edge.
Mesh lathe_mesh(const float2 *profile, uint32_t count, uint32_t sides, float3 color)
{
    Mesh mesh;
    mesh.vertices = static_cast<Vertex *>(allocate(sizeof(Vertex) * count * (sides + 1)));
    mesh.indices = static_cast<uint32_t *>(allocate(sizeof(uint32_t) * (count - 1) * sides * 6));
    // The normal of the segment from point i to i + 1, (0, 0) when the two points coincide.
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
        // A duplicated point is a hard edge: each copy takes the normal of its own side only.
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

// A tube along a cubic Bezier lying in the xy plane, its radius going from r0 to r1.
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

// ---------------------------------------------------------------- the built-in 5x7 font

// Columns left to right, bit 0 the top row: ASCII 32..126.
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

// "+1.25" / "-0.50": always signed, two decimals.
void append_signed(char *to, size_t capacity, float value)
{
    append_text(to, capacity, value <= -0.005f ? "-" : "+");
    append_number(to, capacity, abs(value), 2);
}

// Screen-space text from the 5x7 font: one dynamic vertex buffer rebuilt each frame.
// Screen-space text from the 5x7 font: one dynamic vertex buffer rebuilt each frame.
const char *const bitmap_text_common = R"(
layout(std140, binding = 0) uniform Screen { vec4 screen; vec4 color; };
)";

const char *const bitmap_text_vertex = R"(
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 uv;
out vec2 v_uv;
void main() {
    gl_Position = vec4(position.x / screen.x * 2 - 1, 1 - position.y / screen.y * 2, 0, 1);
    v_uv = uv;
}
)";

const char *const bitmap_text_fragment = R"(
layout(binding = 0) uniform sampler2D atlas;
in vec2 v_uv;
layout(location = 0) out vec4 target;
void main() {
    float a = textureLod(atlas, v_uv, 0).a;
    target = vec4(color.rgb, a * color.a);
}
)";

// The layout of a 2D position and uv at a 16-byte stride, over `vertices`.
GLuint text_layout(GLuint vertices)
{
    GLuint made = 0;
    glCreateVertexArrays(1, &made);
    for (GLuint a = 0; a < 2; a++) {
        glEnableVertexArrayAttrib(made, a);
        glVertexArrayAttribFormat(made, a, 2, GL_FLOAT, GL_FALSE, a * 8);
        glVertexArrayAttribBinding(made, a, 0);
    }
    glVertexArrayVertexBuffer(made, 0, vertices, 0, 16);
    return made;
}

class BitmapText {
public:
    enum Align { left, center, right };
    static constexpr uint32_t max_chars = 2048;

    bool init(Gpu &gpu)
    {
        program_ = gpu.program("bitmaptext", {.common = bitmap_text_common, .vertex = bitmap_text_vertex,
                                              .fragment = bitmap_text_fragment});
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
        sampler_ = gpu.sampler(GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, 1.0f);
        vertices_ = dynamic_buffer(max_chars * 6 * 16);
        layout_ = text_layout(vertices_);
        constants_ = gpu.uniform_buffer(32);
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
        const float constants[8] = {static_cast<float>(gpu.width), static_cast<float>(gpu.height), 0, 0,
                                    color.x, color.y, color.z, color.w};
        gpu.update(constants_, constants, sizeof constants);
        if (!write_buffer(vertices_, staging_, count_ * 24 * 4)) {
            gpu.failed = true;
            return;
        }
        glUseProgram(program_);
        glBindVertexArray(layout_);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, constants_);
        glBindTextureUnit(0, atlas_);
        glBindSampler(0, sampler_);
        blend_state(Blend::alpha);
        depth_state(false, false);
        raster_state(false, false, false);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(count_ * 6));
    }
    void release()
    {
        glDeleteProgram(program_);
        glDeleteVertexArrays(1, &layout_);
        glDeleteTextures(1, &atlas_);
        glDeleteSamplers(1, &sampler_);
        glDeleteBuffers(1, &vertices_);
        glDeleteBuffers(1, &constants_);
        release_memory(staging_);
        staging_ = nullptr;
        count_ = 0;
    }

private:
    GLuint   program_ = 0;
    GLuint   layout_ = 0;
    GLuint   atlas_ = 0;
    GLuint   sampler_ = 0;
    GLuint   vertices_ = 0;
    GLuint   constants_ = 0;
    float   *staging_ = nullptr;
    uint32_t count_ = 0;
};

// Pixel position of a world point, as the text overlay places its labels.
float2 project(const float4x4 &world_view_projection, float3 p, const Gpu &gpu)
{
    const float4 clip = transform4(world_view_projection, p);
    return {(clip.x / clip.w * 0.5f + 0.5f) * static_cast<float>(gpu.width),
            (0.5f - clip.y / clip.w * 0.5f) * static_cast<float>(gpu.height)};
}

// ================================================================ texturearray

const char *const texturearray_common = R"(
layout(std140, binding = 0, row_major) uniform Frame { mat4 view_projection; mat4 world[7]; vec4 layer[7]; };
)";

const char *const texturearray_vertex = MESH_INPUTS R"(
out vec3 v_uv;
void main() {
    gl_Position = (vec4(position, 1) * world[gl_InstanceID]) * view_projection;
    v_uv = vec3(uv, layer[gl_InstanceID].x);
}
)";

const char *const texturearray_fragment = R"(
layout(binding = 0) uniform sampler2DArray layers;
in vec3 v_uv;
layout(location = 0) out vec4 target;
void main() { target = texture(layers, v_uv); }
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

// Seven textures in one array texture, each instance of one quad picking its slice.
class TextureArray final : public Sample {
public:
    const char *name() const override { return "texturearray"; }
    bool init(Gpu &gpu) override
    {
        program_ = gpu.program("texturearray", {.common = texturearray_common, .vertex = texturearray_vertex,
                                                .fragment = texturearray_fragment});
        Mesh mesh = plane_mesh(5.0f, 1, 1.0f);
        quad_ = gpu.upload(mesh);
        mesh.release();
        const int size = 256;
        uint32_t *texels = static_cast<uint32_t *>(allocate(size * size * 4 * layers));
        for (int l = 0; l < static_cast<int>(layers); l++)
            for (int y = 0; y < size; y++)
                for (int x = 0; x < size; x++) texels[(l * size + y) * size + x] = array_texel(l, x, y, size);
        array_ = gpu.texture(texels, size, size, layers);
        release_memory(texels);
        sampler_ = gpu.sampler();
        constants_ = gpu.uniform_buffer(sizeof(TextureArrayFrame));
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        gpu.reset_state();
        gpu.begin_frame(0.02f, 0.02f, 0.02f);
        TextureArrayFrame frame = {};
        const float4x4 view = rotate_y(radians(35.0f) + time * 0.25f) * rotate_x(radians(15.0f)) * translate({0, 0.3f, 15.0f});
        frame.view_projection = view * perspective(radians(60), gpu.aspect(), 0.1f, 256);
        for (uint32_t i = 0; i < layers; i++) {
            frame.world[i] = rotate_x(radians(24.0f)) * translate({0, (3.0f - static_cast<float>(i)) * 1.5f, 0});
            frame.layer[i] = {static_cast<float>(i), 0, 0, 0};
        }
        gpu.update(constants_, &frame, sizeof frame);
        glUseProgram(program_);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, constants_);
        glBindTextureUnit(0, array_);
        glBindSampler(0, sampler_);
        raster_state(false, false, false);
        quad_.draw(layers);
    }
    void release() override
    {
        glDeleteProgram(program_);
        quad_.release();
        glDeleteTextures(1, &array_);
        glDeleteSamplers(1, &sampler_);
        glDeleteBuffers(1, &constants_);
    }

private:
    static constexpr uint32_t layers = 7;
    GLuint   program_ = 0;
    DrawMesh quad_;
    GLuint   array_ = 0;
    GLuint   sampler_ = 0;
    GLuint   constants_ = 0;
};

// ================================================================ texturecubemap

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

// D3D's cube faces in order +x, -x, +y, -y, +z, -z; u right, v down.
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

const char *const texturecubemap_common = R"(
layout(std140, binding = 0, row_major) uniform Frame {
    mat4 world; mat4 view_projection;
    vec4 eye; vec4 right; vec4 up; vec4 forward;   // w of right/up: the tangents of the half fovs
};
)";

const char *const texturecubemap_sky_fragment = R"(
layout(binding = 0) uniform samplerCube environment;
in vec2 v_uv;
layout(location = 0) out vec4 target;
void main() {
    vec2 ndc = vec2(v_uv.x * 2 - 1, 1 - v_uv.y * 2);
    vec3 d = forward.xyz + right.xyz * ndc.x * right.w + up.xyz * ndc.y * up.w;
    target = texture(environment, d);
}
)";

const char *const texturecubemap_vertex = MESH_INPUTS R"(
out vec3 v_normal;
out vec3 v_world_position;
void main() {
    vec4 w = vec4(position, 1) * world;
    gl_Position = w * view_projection;
    v_world_position = w.xyz;
    v_normal = (vec4(normal, 0) * world).xyz;
}
)";

const char *const texturecubemap_fragment = R"(
layout(binding = 0) uniform samplerCube environment;
in vec3 v_normal;
in vec3 v_world_position;
layout(location = 0) out vec4 target;
void main() {
    vec3 n = normalize(v_normal);
    vec3 i = normalize(v_world_position - eye.xyz);
    vec3 r = reflect(i, n);
    float fresnel = 0.55 + 0.45 * pow(1 - clamp(dot(-i, n), 0.0, 1.0), 3);
    target = vec4(texture(environment, r).rgb * fresnel, 1);
}
)";

struct CubemapFrame {
    float4x4 world;
    float4x4 view_projection;
    float4   eye, right, up, forward;
};

// A cube map as a skybox and in a mirror sphere's reflection.
class TextureCubemap final : public Sample {
public:
    const char *name() const override { return "texturecubemap"; }
    bool init(Gpu &gpu) override
    {
        static const char *const sky_vertex = SCREEN_VERTEX;
        program_ = gpu.program("texturecubemap", {.common = texturecubemap_common, .vertex = texturecubemap_vertex,
                                                  .fragment = texturecubemap_fragment});
        sky_ = gpu.program("texturecubemap_sky", {.common = texturecubemap_common, .vertex = sky_vertex,
                                                  .fragment = texturecubemap_sky_fragment});
        Mesh mesh = sphere_mesh(1.0f, 96, 48);
        sphere_ = gpu.upload(mesh);
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
        sampler_ = gpu.sampler(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, 1.0f);
        constants_ = gpu.uniform_buffer(sizeof(CubemapFrame));
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        gpu.reset_state();
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
        gpu.update(constants_, &frame, sizeof frame);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, constants_);
        glBindTextureUnit(0, cube_);
        glBindSampler(0, sampler_);
        glUseProgram(sky_);
        depth_state(false, false);
        gpu.draw_fullscreen();
        depth_state(true, true);
        glUseProgram(program_);
        sphere_.draw();
    }
    void release() override
    {
        glDeleteProgram(program_);
        glDeleteProgram(sky_);
        sphere_.release();
        glDeleteTextures(1, &cube_);
        glDeleteSamplers(1, &sampler_);
        glDeleteBuffers(1, &constants_);
    }

private:
    GLuint   program_ = 0;
    GLuint   sky_ = 0;
    DrawMesh sphere_;
    GLuint   cube_ = 0;
    GLuint   sampler_ = 0;
    GLuint   constants_ = 0;
};

// ================================================================ distancefieldfonts

struct Segment {
    float ax, ay, bx, by;
};

// The glyph's lit pixels joined to their lit neighbours: a stroke skeleton for a smooth, bold letter.
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

const char *const distancefield_common = R"(
layout(std140, binding = 0) uniform Frame { vec4 place; vec4 screen; vec4 outline_color; vec4 outline; };
)";

const char *const distancefield_vertex = R"(
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 uv;
out vec2 v_uv;
void main() {
    vec2 pixel = position * place.x + place.yz;
    gl_Position = vec4(pixel.x / screen.x * 2 - 1, 1 - pixel.y / screen.y * 2, 0, 1);
    v_uv = uv;
}
)";

const char *const distancefield_sdf_fragment = R"(
layout(binding = 0) uniform sampler2D atlas;
in vec2 v_uv;
layout(location = 0) out vec4 target;
void main() {
    float distance = texture(atlas, v_uv).a;
    float smooth_width = fwidth(distance);
    float alpha = smoothstep(0.5 - smooth_width, 0.5 + smooth_width, distance);
    vec3 rgb = vec3(alpha);
    if (outline.y > 0.0) {
        float w = 1.0 - outline.x;
        alpha = smoothstep(w - smooth_width, w + smooth_width, distance);
        rgb += mix(vec3(alpha), outline_color.rgb, alpha);
    }
    target = vec4(clamp(rgb, 0.0, 1.0), alpha);
}
)";

const char *const distancefield_bitmap_fragment = R"(
layout(binding = 0) uniform sampler2D atlas;
in vec2 v_uv;
layout(location = 0) out vec4 target;
void main() { target = vec4(1, 1, 1, texture(atlas, v_uv).a); }
)";

struct DistanceFieldFrame {
    float4 place;   // x: pixels per glyph unit, yz: pixel offset
    float4 screen;
    float4 outline_color;
    float4 outline;
};

// A signed distance field atlas, made on the CPU from the 5x7 font, drawn sharp with an
// outline at any scale; below it, the same glyphs as a plain coverage bitmap, magnified.
class DistanceFieldFonts final : public Sample {
public:
    const char *name() const override { return "distancefieldfonts"; }
    bool init(Gpu &gpu) override
    {
        sdf_program_ = gpu.program("distancefieldfonts_sdf", {.common = distancefield_common,
                                                              .vertex = distancefield_vertex,
                                                              .fragment = distancefield_sdf_fragment});
        bitmap_program_ = gpu.program("distancefieldfonts_bitmap", {.common = distancefield_common,
                                                                    .vertex = distancefield_vertex,
                                                                    .fragment = distancefield_bitmap_fragment});
        build_atlases(gpu);
        build_text(gpu, text);
        sampler_ = gpu.sampler(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, 1.0f);
        constants_ = gpu.uniform_buffer(sizeof(DistanceFieldFrame));
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        gpu.reset_state();
        gpu.begin_frame(0.02f, 0.02f, 0.02f);
        glBindVertexArray(layout_);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, constants_);
        glBindSampler(0, sampler_);
        blend_state(Blend::alpha);
        depth_state(false, false);
        raster_state(false, false, false);
        const float w = static_cast<float>(gpu.width), h = static_cast<float>(gpu.height);
        const float base = w * 0.8f / text_width_;
        DistanceFieldFrame frame = {};
        frame.screen = {w, h, 0, 0};
        frame.outline_color = {1, 0, 0, 1};
        frame.outline = {0.6f, 1, 0, 0};
        const float top = base * (1.0f + 0.06f * sin(time * 1.3f));
        frame.place = {top, w * 0.5f - text_width_ * 0.5f * top, h * 0.27f - 3.5f * top, 0};
        gpu.update(constants_, &frame, sizeof frame);
        glUseProgram(sdf_program_);
        glBindTextureUnit(0, sdf_);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertex_count_));
        frame.place = {base, w * 0.5f - text_width_ * 0.5f * base, h * 0.73f - 3.5f * base, 0};
        frame.outline = {0, 0, 0, 0};
        gpu.update(constants_, &frame, sizeof frame);
        glUseProgram(bitmap_program_);
        glBindTextureUnit(0, bitmap_);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertex_count_));
    }
    void release() override
    {
        glDeleteProgram(sdf_program_);
        glDeleteProgram(bitmap_program_);
        glDeleteVertexArrays(1, &layout_);
        glDeleteTextures(1, &sdf_);
        glDeleteTextures(1, &bitmap_);
        glDeleteBuffers(1, &vertices_);
        glDeleteSamplers(1, &sampler_);
        glDeleteBuffers(1, &constants_);
    }

private:
    static constexpr int cell_w = 40, cell_h = 50, texels_per_unit = 5, atlas_w = 16 * cell_w, atlas_h = 6 * cell_h;
    static constexpr float pad = 1.5f, stroke = 0.72f, advance = 6.3f;
    const char *const text = "OpenGL";

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
    // One quad per glyph cell, in glyph units from the text's top-left.
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
        vertices_ = gpu.buffer(quads, vertex_count_ * 16);
        layout_ = text_layout(vertices_);
        release_memory(quads);
    }

    GLuint   sdf_program_ = 0, bitmap_program_ = 0;
    GLuint   layout_ = 0;
    GLuint   sdf_ = 0, bitmap_ = 0;
    GLuint   vertices_ = 0;
    GLuint   sampler_ = 0;
    GLuint   constants_ = 0;
    uint32_t vertex_count_ = 0;
    float    text_width_ = 1.0f;
};

// ================================================================ textoverlay

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

const char *const textoverlay_common = R"(
layout(std140, binding = 0, row_major) uniform Frame { mat4 world; mat4 view_projection; vec4 light; vec4 eye; vec4 screen; };
)";

const char *const textoverlay_vertex = MESH_INPUTS R"(
out vec3 v_normal;
out vec2 v_uv;
out vec3 v_world_position;
void main() {
    vec4 w = vec4(position, 1) * world;
    gl_Position = w * view_projection;
    v_world_position = w.xyz;
    v_normal = (vec4(normal, 0) * world).xyz;
    v_uv = uv;
}
)";

const char *const textoverlay_fragment = R"(
layout(binding = 0) uniform sampler2D image;
in vec3 v_normal;
in vec2 v_uv;
in vec3 v_world_position;
layout(location = 0) out vec4 target;
void main() {
    vec4 color = texture(image, v_uv);
    vec3 n = normalize(v_normal), l = normalize(light.xyz), v = normalize(eye.xyz - v_world_position);
    vec3 r = reflect(-l, n);
    vec3 lit = color.rgb * (0.3 + 0.9 * max(dot(n, l), 0.0)) + pow(max(dot(r, v), 0.0), 16.0) * color.a;
    target = vec4(lit, 1);
}
)";

const char *const textoverlay_sky_fragment = R"(
in vec2 v_uv;
layout(location = 0) out vec4 target;
float hash2(ivec2 p) {
    uint h = uint(p.x) * 0x8da6b343u ^ uint(p.y) * 0xd8163841u;
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return float(h & 0xffffffu) / 16777216.0;
}
float noise(vec2 x) {
    ivec2 i = ivec2(floor(x));
    vec2 f = fract(x);
    f = f * f * (3 - 2 * f);
    return mix(mix(hash2(i), hash2(i + ivec2(1, 0)), f.x), mix(hash2(i + ivec2(0, 1)), hash2(i + ivec2(1, 1)), f.x), f.y);
}
float clouds(vec2 x) {
    float s = 0, a = 0.5;
    for (int k = 0; k < 5; k++) { s += noise(x) * a; x *= 2.07; a *= 0.5; }
    return s;
}
void main() {
    vec2 uv = v_uv;
    float horizon = 0.52;
    vec3 top = vec3(0.36, 0.42, 0.6), glow = vec3(0.8, 0.83, 0.9), low = vec3(0.3, 0.34, 0.52);
    vec3 c = uv.y < horizon ? mix(top, glow, pow(uv.y / horizon, 3)) : mix(glow, low, sqrt((uv.y - horizon) / (1 - horizon)));
    float above = clamp((horizon - uv.y) / horizon, 0.0, 1.0);
    float depth = 1.0 / (above + 0.04);   // a cloud layer seen in perspective: thin near the horizon
    float cover = clouds(vec2((uv.x - 0.5) * depth * 1.2 + 7.0, depth * 2.5));
    c = mix(c, vec3(0.9, 0.91, 0.95), smoothstep(0.45, 0.8, cover) * smoothstep(0.0, 0.08, above) * 0.75);
    vec2 d = (uv - vec2(0.095, 0.22)) * vec2(screen.x / screen.y, 1);
    float sun = length(d);
    c += vec3(0.45, 0.45, 0.5) * exp(-sun * 9) + smoothstep(0.009, 0.005, sun);
    target = vec4(c, 1);
}
)";

struct TextOverlayFrame {
    float4x4 world;
    float4x4 view_projection;
    float4   light, eye, screen;
};

// Screen text over a 3D scene: a bitmap font in a dynamic vertex buffer, alpha blended,
// with labels following the projected corners of a textured cube.
class TextOverlay final : public Sample {
public:
    const char *name() const override { return "textoverlay"; }
    bool init(Gpu &gpu) override
    {
        static const char *const sky_vertex = SCREEN_VERTEX;
        program_ = gpu.program("textoverlay", {.common = textoverlay_common, .vertex = textoverlay_vertex,
                                               .fragment = textoverlay_fragment});
        sky_ = gpu.program("textoverlay_sky", {.common = textoverlay_common, .vertex = sky_vertex,
                                               .fragment = textoverlay_sky_fragment});
        Mesh mesh = cube_mesh(2.0f);
        cube_ = gpu.upload(mesh);
        mesh.release();
        const int size = 256;
        uint32_t *texels = static_cast<uint32_t *>(allocate(size * size * 4));
        for (int y = 0; y < size; y++)
            for (int x = 0; x < size; x++) texels[y * size + x] = panel_texel(x, y, size);
        panel_ = gpu.texture(texels, size, size);
        release_memory(texels);
        sampler_ = gpu.sampler();
        constants_ = gpu.uniform_buffer(sizeof(TextOverlayFrame));
        text_.init(gpu);
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        gpu.reset_state();
        gpu.begin_frame(0, 0, 0);
        TextOverlayFrame frame = {};
        frame.world = rotate_y(time * 0.35f - 0.3f);
        const float4x4 view = rotate_x(radians(-25.0f)) * translate({0, 0, 4.5f});
        frame.view_projection = view * perspective(radians(60), gpu.aspect(), 0.1f, 256);
        frame.light = {-0.4f, 0.8f, -1.0f, 0};
        frame.eye = {0, 4.5f * sin(radians(25.0f)), -4.5f * cos(radians(25.0f)), 1};
        raster_state(false, false, false);
        frame.screen = {static_cast<float>(gpu.width), static_cast<float>(gpu.height), 0, 0};
        gpu.update(constants_, &frame, sizeof frame);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, constants_);
        glUseProgram(sky_);
        depth_state(false, false);
        gpu.draw_fullscreen();
        depth_state(true, true);
        glUseProgram(program_);
        glBindTextureUnit(0, panel_);
        glBindSampler(0, sampler_);
        cube_.draw();
        write_text(gpu, frame, view, time);
        text_.draw(gpu);
    }
    void release() override
    {
        glDeleteProgram(program_);
        glDeleteProgram(sky_);
        cube_.release();
        glDeleteTextures(1, &panel_);
        glDeleteSamplers(1, &sampler_);
        glDeleteBuffers(1, &constants_);
        text_.release();
    }

private:
    void write_text(Gpu &gpu, const TextOverlayFrame &frame, const float4x4 &view, float time)
    {
        const float w = static_cast<float>(gpu.width), h = static_cast<float>(gpu.height);
        text_.clear();
        text_.add("OpenGL Example - Text overlay", 5, 5);
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
        text_.add("Every frame rebuilds this text with glMapNamedBufferRange", 5, h - 20);
    }

    GLuint     program_ = 0, sky_ = 0;
    DrawMesh   cube_;
    GLuint     panel_ = 0;
    GLuint     sampler_ = 0;
    GLuint     constants_ = 0;
    BitmapText text_;
};

// ================================================================ debugmarker

const char *const debugmarker_common = R"(
layout(std140, binding = 0, row_major) uniform Frame { mat4 view_projection; vec4 light; };
)";

const char *const debugmarker_vertex = MESH_INPUTS R"(
out vec3 v_normal;
out vec3 v_color;
void main() {
    gl_Position = vec4(position, 1) * view_projection;
    v_normal = normal;
    v_color = color;
}
)";

const char *const debugmarker_toon_fragment = R"(
in vec3 v_normal;
in vec3 v_color;
layout(location = 0) out vec4 target;
void main() {
    float intensity = dot(normalize(v_normal), normalize(light.xyz));
    float shade = 1.0;
    shade = intensity < 0.5 ? 0.75 : shade;
    shade = intensity < 0.35 ? 0.6 : shade;
    shade = intensity < 0.25 ? 0.5 : shade;
    shade = intensity < 0.1 ? 0.25 : shade;
    target = vec4(v_color * 1.3 * shade, 1);
}
)";

const char *const debugmarker_color_fragment = R"(
in vec3 v_normal;
in vec3 v_color;
layout(location = 0) out vec4 target;
void main() { target = vec4(v_color, 1); }
)";

const char *const debugmarker_post_fragment = R"(
layout(binding = 0) uniform sampler2D glow;
in vec2 v_uv;
layout(location = 0) out vec4 target;
void main() {
    const vec2 o = vec2(0.01, 0.01);
    vec4 sum = texture(glow, v_uv + vec2(-o.x, -o.y)) + 2 * texture(glow, v_uv + vec2(0, -o.y)) +
               texture(glow, v_uv + vec2(o.x, -o.y)) + 2 * texture(glow, v_uv + vec2(-o.x, 0)) +
               4 * texture(glow, v_uv) + 2 * texture(glow, v_uv + vec2(o.x, 0)) +
               texture(glow, v_uv + vec2(-o.x, o.y)) + 2 * texture(glow, v_uv + vec2(0, o.y)) +
               texture(glow, v_uv + vec2(o.x, o.y));
    target = vec4(sum.rgb / 16.0 * 1.5, 1);
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

// The debug marker scene's named parts; the glowing ones are drawn again into the glow target.
enum ScenePart { hill, rocks, tree, stems, blue_caps, red_caps, grass, chest_box, chest_fittings, part_count };

const char *const part_events[part_count] = {"Draw \"hill\"", "Draw \"rocks\"", "Draw \"tree\"",
                                                "Draw \"mushroom stems\"", "Draw \"blue mushroom caps\"",
                                                "Draw \"red mushroom caps\"", "Draw \"grass blades\"",
                                                "Draw \"chest box\"", "Draw \"chest fittings\""};
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

// KHR_debug groups and markers around every pass and draw, object labels on
// the resources - Direct3D's user annotations and debug object names; the
// scene is toon shaded on the left, wireframe on the right (scissored), with a
// blurred glow pass rendered off-screen and added on top.
class DebugMarker final : public Sample {
public:
    const char *name() const override { return "debugmarker"; }
    bool init(Gpu &gpu) override
    {
        static const char *const post_vertex = SCREEN_VERTEX;
        toon_ = gpu.program("debugmarker_toon", {.common = debugmarker_common, .vertex = debugmarker_vertex,
                                                 .fragment = debugmarker_toon_fragment});
        color_ = gpu.program("debugmarker_color", {.common = debugmarker_common, .vertex = debugmarker_vertex,
                                                   .fragment = debugmarker_color_fragment});
        post_ = gpu.program("debugmarker_post", {.common = debugmarker_common, .vertex = post_vertex,
                                                 .fragment = debugmarker_post_fragment});
        for (int p = 0; p < part_count; p++) {
            MeshBuilder builder;
            build_scene_part(static_cast<ScenePart>(p), builder);
            Mesh mesh = builder.finish();
            parts_[p] = gpu.upload(mesh);
            mesh.release();
            name_object(GL_BUFFER, parts_[p].vertices, part_names[p]);
        }
        glow_.create(gpu, gpu.width / 4, gpu.height / 4);
        name_object(GL_TEXTURE, glow_.texture, "Off-screen glow target");
        constants_ = gpu.uniform_buffer(sizeof(DebugMarkerFrame));
        name_object(GL_BUFFER, constants_, "Scene constants");
        sampler_ = gpu.sampler(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, 1.0f);
        name_object(GL_PROGRAM, toon_, "Toon shading program");
        name_object(GL_PROGRAM, color_, "Wireframe program");
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        gpu.reset_state();
        const float a = 0.3f * sin(time * 0.4f);
        const float3 eye = {sin(a) * -10.5f, 3.4f, -cos(a) * 10.5f};
        DebugMarkerFrame frame = {};
        frame.view_projection = look_at(eye, {0.3f, 1.2f, 1.0f}, {0, 1, 0}) * perspective(radians(60), gpu.aspect(), 0.1f, 256);
        frame.light = {0.5f, 1.0f, -0.6f, 0};
        gpu.update(constants_, &frame, sizeof frame);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, constants_);

        begin("Off-screen scene rendering");
        const float black[4] = {0, 0, 0, 1};
        glBindFramebuffer(GL_FRAMEBUFFER, glow_.target);
        glClearNamedFramebufferfv(glow_.target, GL_COLOR, 0, black);
        glClearNamedFramebufferfi(glow_.target, GL_DEPTH_STENCIL, 0, 1.0f, 0);
        gpu.viewport(0, 0, static_cast<float>(glow_.width), static_cast<float>(glow_.height));
        set_scissor(0, 0, glow_.width, glow_.height);
        raster_state(false, false, true);
        glUseProgram(color_);
        draw_part(red_caps);
        draw_part(chest_fittings);
        end();

        begin("Render scene");
        gpu.begin_frame(0.02f, 0.02f, 0.025f);
        glEnable(GL_SCISSOR_TEST);
        const uint32_t half = gpu.width / 2;
        begin("Toon shading draw");
        set_scissor(0, 0, half, gpu.height);
        glUseProgram(toon_);
        for (int p = 0; p < part_count; p++) draw_part(static_cast<ScenePart>(p));
        end();
        begin("Wireframe draw");
        set_scissor(half, 0, gpu.width, gpu.height);
        raster_state(true, false, true);
        glUseProgram(color_);
        for (int p = 0; p < part_count; p++) draw_part(static_cast<ScenePart>(p));
        end();
        begin("Apply post processing");
        set_scissor(0, 0, gpu.width, gpu.height);
        raster_state(false, false, true);
        glUseProgram(post_);
        glBindTextureUnit(0, glow_.texture);
        glBindSampler(0, sampler_);
        blend_state(Blend::additive);
        depth_state(false, false);
        gpu.draw_fullscreen();
        end();
        end();
        glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0, GL_DEBUG_SEVERITY_NOTIFICATION, -1,
                             "Frame done");
    }
    void release() override
    {
        glDeleteProgram(toon_);
        glDeleteProgram(color_);
        glDeleteProgram(post_);
        for (DrawMesh &part : parts_) part.release();
        glow_.release();
        glDeleteBuffers(1, &constants_);
        glDeleteSamplers(1, &sampler_);
    }

private:
    void name_object(GLenum kind, GLuint object, const char *label) { glObjectLabel(kind, object, -1, label); }
    void begin(const char *label) { glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, label); }
    void end() { glPopDebugGroup(); }
    // Direct3D's rectangle, corners from the top left; the window's rows run
    // the same way under the upper-left clip origin.
    void set_scissor(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)
    {
        glScissor(static_cast<GLint>(x0), static_cast<GLint>(y0), static_cast<GLsizei>(x1 - x0),
                  static_cast<GLsizei>(y1 - y0));
    }
    void draw_part(ScenePart part)
    {
        begin(part_events[part]);
        parts_[part].draw();
        end();
    }

    GLuint        toon_ = 0, color_ = 0, post_ = 0;
    DrawMesh      parts_[part_count];
    RenderTexture glow_;
    GLuint        constants_ = 0;
    GLuint        sampler_ = 0;
};

// ================================================================ occlusionquery

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


const char *const occlusionquery_common = R"(
layout(std140, binding = 0, row_major) uniform Frame {
    mat4 world; mat4 view_projection;
    vec4 light; vec4 eye; vec4 params;   // x: visible, y: alpha, z: flat colour
};
)";

const char *const occlusionquery_vertex = MESH_INPUTS R"(
out vec3 v_normal;
out vec3 v_color;
out vec3 v_world_position;
void main() {
    vec4 w = vec4(position, 1) * world;
    gl_Position = w * view_projection;
    v_world_position = w.xyz;
    v_normal = (vec4(normal, 0) * world).xyz;
    v_color = color;
}
)";

const char *const occlusionquery_fragment = R"(
in vec3 v_normal;
in vec3 v_color;
in vec3 v_world_position;
layout(location = 0) out vec4 target;
void main() {
    if (params.z > 0) { target = vec4(v_color, params.y); return; }
    if (params.x <= 0) { target = vec4(0.1, 0.1, 0.1, 1); return; }
    vec3 n = normalize(v_normal), l = normalize(light.xyz - v_world_position), v = normalize(eye.xyz - v_world_position);
    vec3 r = reflect(-l, n);
    vec3 diffuse = max(dot(n, l), 0.0) * v_color;
    vec3 specular = vec3(pow(max(dot(r, v), 0.0), 8.0) * 0.75);
    target = vec4(diffuse + specular, 1);
}
)";

struct OcclusionFrame {
    float4x4 world;
    float4x4 view_projection;
    float4   light, eye, params;
};

// Two objects either side of a translucent plane; GL_SAMPLES_PASSED queries,
// run in a depth-only pass, decide whether each is shaded or drawn dark.
class OcclusionQuery final : public Sample {
public:
    const char *name() const override { return "occlusionquery"; }
    bool init(Gpu &gpu) override
    {
        program_ = gpu.program("occlusionquery", {.common = occlusionquery_common, .vertex = occlusionquery_vertex,
                                                  .fragment = occlusionquery_fragment});
        // Direct3D's depth pass binds no pixel shader: a program with no fragment stage.
        depth_only_ = gpu.program("occlusionquery_depth", {.common = occlusionquery_common,
                                                           .vertex = occlusionquery_vertex});
        Mesh teapot = teapot_mesh({1, 0, 0});
        teapot_ = gpu.upload(teapot);
        teapot.release();
        Mesh sphere = sphere_mesh(2.2f, 48, 24);
        paint(sphere, {0, 1, 0}, 0);
        sphere_ = gpu.upload(sphere);
        sphere.release();
        Mesh plane = plane_mesh(1.0f, 1, 1.0f);
        paint(plane, {0, 0, 1}, 0);
        plane_ = gpu.upload(plane);
        plane.release();
        constants_ = gpu.uniform_buffer(sizeof(OcclusionFrame));
        glCreateQueries(GL_SAMPLES_PASSED, 2, queries_);
        text_.init(gpu);
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        gpu.reset_state();
        gpu.begin_frame(0.03f, 0.03f, 0.03f);
        const float a = radians(-52.0f) + 0.5f * sin(time * 0.5f);
        eye_ = {sin(a) * 19.0f, 1.0f, -cos(a) * 19.0f};
        view_projection_ = look_at(eye_, {0, 0, 0}, {0, 1, 0}) * perspective(radians(60), gpu.aspect(), 0.1f, 256);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, constants_);
        raster_state(false, false, false);

        // The occlusion pass: depth only, the occluder first, then each object counted against it.
        glUseProgram(depth_only_);
        blend_state(Blend::no_color);
        draw(gpu, plane_, occluder_world(), {0, 0, 1, 0});
        depth_state(true, false);
        glBeginQuery(GL_SAMPLES_PASSED, queries_[0]);
        draw(gpu, teapot_, teapot_world(time), {1, 1, 0, 0});
        glEndQuery(GL_SAMPLES_PASSED);
        glBeginQuery(GL_SAMPLES_PASSED, queries_[1]);
        draw(gpu, sphere_, sphere_world(), {1, 1, 0, 0});
        glEndQuery(GL_SAMPLES_PASSED);
        // As the original: the scene shows what the previous frame counted, and
        // this frame's counts are read once all of it has been submitted.
        const GLuint64 *passed = passed_;

        glDepthMask(GL_TRUE);
        glClearNamedFramebufferfi(gpu.target, GL_DEPTH_STENCIL, 0, 1.0f, 0);
        depth_state(true, true);
        blend_state(Blend::none);
        glUseProgram(program_);
        draw(gpu, teapot_, teapot_world(time), {passed[0] > 0 ? 1.0f : 0.0f, 1, 0, 0});
        draw(gpu, sphere_, sphere_world(), {passed[1] > 0 ? 1.0f : 0.0f, 1, 0, 0});
        blend_state(Blend::alpha);
        depth_state(true, false);
        draw(gpu, plane_, occluder_world(), {0, 0.5f, 1, 0});

        text_.clear();
        text_.add("OpenGL Example - Occlusion queries", 5, 5);
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
        for (int i = 0; i < 2; i++) glGetQueryObjectui64v(queries_[i], GL_QUERY_RESULT, &passed_[i]);
    }
    void release() override
    {
        glDeleteProgram(program_);
        glDeleteProgram(depth_only_);
        teapot_.release();
        sphere_.release();
        plane_.release();
        glDeleteBuffers(1, &constants_);
        glDeleteQueries(2, queries_);
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
        gpu.update(constants_, &frame, sizeof frame);
        mesh.draw();
    }

    GLuint     program_ = 0, depth_only_ = 0;
    DrawMesh   teapot_, sphere_, plane_;
    GLuint     constants_ = 0;
    GLuint     queries_[2] = {};
    GLuint64   passed_[2] = {};
    BitmapText text_;
    float3     eye_ = {0, 0, 0};
    float4x4   view_projection_ = float4x4::identity();
};

// ================================================================ multithreading

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


const char *const multithreading_common = R"(
layout(std140, binding = 0, row_major) uniform Object { mat4 world; mat4 view_projection; vec4 tint; vec4 eye; };
)";

const char *const multithreading_vertex = MESH_INPUTS R"(
out vec3 v_normal;
out vec3 v_color;
out vec3 v_world_position;
void main() {
    vec4 w = vec4(position, 1) * world;
    gl_Position = w * view_projection;
    v_world_position = w.xyz;
    v_normal = (vec4(normal, 0) * world).xyz;
    v_color = mix(color, tint.rgb, uv.x);
}
)";

const char *const multithreading_fragment = R"(
in vec3 v_normal;
in vec3 v_color;
in vec3 v_world_position;
layout(location = 0) out vec4 target;
void main() {
    vec3 n = normalize(v_normal), l = normalize(vec3(0.3, 1.0, -0.6)), v = normalize(eye.xyz - v_world_position);
    vec3 r = reflect(-l, n);
    vec3 diffuse = (0.15 + max(dot(n, l), 0.0)) * v_color;
    vec3 specular = vec3(pow(max(dot(r, v), 0.0), 8.0) * 0.75);
    target = vec4(diffuse + specular, 1);
}
)";

const char *const multithreading_star_vertex = MESH_INPUTS R"(
out vec3 v_direction;
void main() {
    gl_Position = (vec4(position + eye.xyz, 1) * view_projection).xyww;
    v_direction = position;
}
)";

const char *const multithreading_star_fragment = R"(
in vec3 v_direction;
layout(location = 0) out vec4 target;
float hash3(ivec3 p) {
    uint h = uint(p.x) * 0x8da6b343u ^ uint(p.y) * 0xd8163841u ^ uint(p.z) * 0xcb1ab31fu;
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return float(h & 0xffffffu) / 16777216.0;
}
void main() {
    vec3 d = normalize(v_direction);
    float r = hash3(ivec3(floor(d * 350)));
    const float threshold = 0.993;
    float star = r >= threshold ? pow((r - threshold) / (1 - threshold), 6.0) : 0;
    vec3 atmosphere = clamp(vec3(0.1, 0.15, 0.4) * (-d.y - 0.08) * 14.0, 0.0, 1.0);
    target = vec4(star + atmosphere, 1);
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


// OpenGL has no command lists to record elsewhere: the worker threads compute
// their share of the saucers' constants in parallel, as the original's
// secondary command buffers are recorded, and the main thread uploads them in
// one piece and draws each through its own range of the buffer.
class Multithreading final : public Sample {
public:
    const char *name() const override { return "multithreading"; }
    bool init(Gpu &gpu) override
    {
        program_ = gpu.program("multithreading", {.common = multithreading_common, .vertex = multithreading_vertex,
                                                  .fragment = multithreading_fragment});
        star_ = gpu.program("multithreading_star", {.common = multithreading_common,
                                                    .vertex = multithreading_star_vertex,
                                                    .fragment = multithreading_star_fragment});
        Mesh ufo = ufo_mesh();
        ufo_ = gpu.upload(ufo);
        ufo.release();
        Mesh sky = sphere_mesh(100.0f, 48, 24);
        sky_ = gpu.upload(sky);
        sky.release();
        sky_constants_ = gpu.uniform_buffer(sizeof(UfoConstants));
        GLint alignment = 256;
        glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &alignment);
        stride_ = (static_cast<uint32_t>(sizeof(UfoConstants)) + static_cast<uint32_t>(alignment) - 1u) /
                  static_cast<uint32_t>(alignment) * static_cast<uint32_t>(alignment);
        objects_buffer_ = gpu.uniform_buffer(stride_ * objects);
        staging_ = static_cast<uint8_t *>(allocate(stride_ * objects));
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
            workers_[t].owner = this;
            workers_[t].first = t * (objects / threads);
            workers_[t].count = objects / threads;
        }
        // The pool lives as long as the sample, as the original's does: a
        // frame hands each worker its share and waits for it.
        for (Worker &w : workers_) spawn(w);
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        gpu.reset_state();
        time_ = time;
        const float a = radians(37.5f) + time * 0.04f;
        eye_ = {sin(a) * 30.0f, 1.2f, -cos(a) * 30.0f};
        view_projection_ = look_at(eye_, {0, 2.5f, 0}, {0, 1, 0}) * perspective(radians(60), gpu.aspect(), 0.1f, 256);
        for (Worker &w : workers_) start(w);
        gpu.begin_frame(0, 0, 0.2f);
        UfoConstants sky = {};
        sky.world = float4x4::identity();
        sky.view_projection = view_projection_;
        sky.eye = {eye_.x, eye_.y, eye_.z, 1};
        gpu.update(sky_constants_, &sky, sizeof sky);
        glUseProgram(star_);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, sky_constants_);
        glDisable(GL_CULL_FACE);
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LEQUAL);
        sky_.draw();
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        for (Worker &w : workers_) join(w);
        gpu.update(objects_buffer_, staging_, stride_ * objects);
        glUseProgram(program_);
        for (uint32_t i = 0; i < objects; i++) {
            glBindBufferRange(GL_UNIFORM_BUFFER, 0, objects_buffer_, static_cast<GLintptr>(i * stride_),
                              sizeof(UfoConstants));
            ufo_.draw();
        }
    }
    void release() override
    {
        glDeleteProgram(program_);
        glDeleteProgram(star_);
        ufo_.release();
        sky_.release();
        glDeleteBuffers(1, &sky_constants_);
        glDeleteBuffers(1, &objects_buffer_);
        for (Worker &w : workers_) retire(w);
        release_memory(staging_);
        staging_ = nullptr;
    }

private:
    static constexpr uint32_t threads = 4, objects = 256;
    struct Worker {
        Multithreading *owner = nullptr;
        uint32_t        first = 0, count = 0;
        bool            quit = false;
#ifdef _WIN32
        HANDLE thread = nullptr, go = nullptr, done = nullptr;
#else
        pthread_t       thread = {};
        bool            running = false, pending = false, finished = false;
        pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
        pthread_cond_t  wake = PTHREAD_COND_INITIALIZER;
#endif
    };

#ifdef _WIN32
    static DWORD WINAPI thread_entry(LPVOID argument)
    {
        Worker *w = static_cast<Worker *>(argument);
        for (;;) {
            WaitForSingleObject(w->go, INFINITE);
            if (w->quit) return 0;
            w->owner->record(*w);
            SetEvent(w->done);
        }
    }
    void spawn(Worker &w)
    {
        w.go = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        w.done = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (w.go != nullptr && w.done != nullptr) w.thread = CreateThread(nullptr, 256 * 1024, thread_entry, &w, 0, nullptr);
    }
    void start(Worker &w)
    {
        if (w.thread == nullptr) record(w);
        else SetEvent(w.go);
    }
    void join(Worker &w)
    {
        if (w.thread != nullptr) WaitForSingleObject(w.done, INFINITE);
    }
    void retire(Worker &w)
    {
        if (w.thread != nullptr) {
            w.quit = true;
            SetEvent(w.go);
            WaitForSingleObject(w.thread, INFINITE);
            CloseHandle(w.thread);
        }
        if (w.go != nullptr) CloseHandle(w.go);
        if (w.done != nullptr) CloseHandle(w.done);
        w.thread = w.go = w.done = nullptr;
    }
#else
    static void *thread_entry(void *argument)
    {
        Worker *w = static_cast<Worker *>(argument);
        for (;;) {
            pthread_mutex_lock(&w->lock);
            while (!w->pending) pthread_cond_wait(&w->wake, &w->lock);
            w->pending = false;
            const bool quit = w->quit;
            pthread_mutex_unlock(&w->lock);
            if (quit) return nullptr;
            w->owner->record(*w);
            pthread_mutex_lock(&w->lock);
            w->finished = true;
            pthread_cond_broadcast(&w->wake);
            pthread_mutex_unlock(&w->lock);
        }
    }
    void spawn(Worker &w) { w.running = pthread_create(&w.thread, nullptr, thread_entry, &w) == 0; }
    void start(Worker &w)
    {
        if (!w.running) {
            record(w);
            return;
        }
        pthread_mutex_lock(&w.lock);
        w.finished = false;
        w.pending = true;
        pthread_cond_broadcast(&w.wake);
        pthread_mutex_unlock(&w.lock);
    }
    void join(Worker &w)
    {
        if (!w.running) return;
        pthread_mutex_lock(&w.lock);
        while (!w.finished) pthread_cond_wait(&w.wake, &w.lock);
        pthread_mutex_unlock(&w.lock);
    }
    void retire(Worker &w)
    {
        if (!w.running) return;
        pthread_mutex_lock(&w.lock);
        w.quit = true;
        w.pending = true;
        pthread_cond_broadcast(&w.wake);
        pthread_mutex_unlock(&w.lock);
        pthread_join(w.thread, nullptr);
        w.running = false;
    }
#endif

    // On the worker's thread: its share of the saucers' constants.
    void record(Worker &w)
    {
        for (uint32_t i = w.first; i < w.first + w.count; i++) {
            const UfoState &u = ufos_[i];
            const float delta = fract(u.delta + 0.15f * time_);
            const float angle = radians(u.rotation + 2.5f * u.speed * time_) + delta * 2.0f * pi;
            const float3 position = {u.position.x, sin(delta * 2.0f * pi) * 2.5f, u.position.z};
            UfoConstants &object = *reinterpret_cast<UfoConstants *>(staging_ + i * stride_);
            object.world = scale({u.size, u.size, u.size}) * rotate_y(angle * u.direction) *
                           rotate_x(-sin(delta * 2.0f * pi) * 0.25f * u.direction) * translate(position);
            object.view_projection = view_projection_;
            object.tint = u.color;
            object.eye = {eye_.x, eye_.y, eye_.z, 1};
        }
    }

    GLuint   program_ = 0, star_ = 0;
    DrawMesh ufo_, sky_;
    GLuint   sky_constants_ = 0;
    GLuint   objects_buffer_ = 0;
    uint8_t *staging_ = nullptr;
    uint32_t stride_ = 0;
    UfoState ufos_[objects] = {};
    Worker   workers_[threads];
    float    time_ = 0;
    float3   eye_ = {0, 0, 0};
    float4x4 view_projection_ = float4x4::identity();
};

}  // namespace

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

}  // namespace samples
