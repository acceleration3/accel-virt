// shadowmapping, shadowmappingomni, deferred, parallaxmapping, sphericalenvmapping, skeletalanimation, vulkanscene
#include "framework.h"

namespace samples {

namespace {

// ---------------------------------------------------------------- procedural helpers

uint32_t mix_bits(uint32_t x)
{
    x ^= x >> 16u;
    x *= 0x7FEB352Du;
    x ^= x >> 15u;
    x *= 0x846CA68Bu;
    x ^= x >> 16u;
    return x;
}

float lattice(int x, int y, int z, uint32_t seed)
{
    const uint32_t h = mix_bits(static_cast<uint32_t>(x) * 0x8DA6B343u ^
                                mix_bits(static_cast<uint32_t>(y) * 0xD8163841u ^
                                         mix_bits(static_cast<uint32_t>(z) * 0xCB1AB31Fu ^ seed)));
    return static_cast<float>(h & 0xFFFFFFu) / 16777216.0f;
}

int wrap_index(int v, int period)
{
    if (period <= 0) return v;
    v %= period;
    return v < 0 ? v + period : v;
}

float saturate(float x) { return clamp(x, 0.0f, 1.0f); }
float smoothstep(float a, float b, float x)
{
    const float t = saturate((x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}
float power(float base, int exponent)
{
    float r = 1.0f;
    for (int i = 0; i < exponent; i++) r *= base;
    return r;
}
float3 mix3(float3 a, float3 b, float t) { return a + (b - a) * t; }
float3 mul3(float3 a, float3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }

// Value noise on an integer lattice; x and y repeat every `period` cells when it is positive.
float value_noise(float x, float y, float z, uint32_t seed, int period)
{
    const float fx = floor(x), fy = floor(y), fz = floor(z);
    const int ix = static_cast<int>(fx), iy = static_cast<int>(fy), iz = static_cast<int>(fz);
    const float tx = x - fx, ty = y - fy, tz = z - fz;
    const float sx = tx * tx * (3 - 2 * tx), sy = ty * ty * (3 - 2 * ty), sz = tz * tz * (3 - 2 * tz);
    const int x0 = wrap_index(ix, period), x1 = wrap_index(ix + 1, period);
    const int y0 = wrap_index(iy, period), y1 = wrap_index(iy + 1, period);
    float corners[2];
    for (int k = 0; k < 2; k++) {
        const int zz = iz + k;
        const float a = mix(lattice(x0, y0, zz, seed), lattice(x1, y0, zz, seed), sx);
        const float b = mix(lattice(x0, y1, zz, seed), lattice(x1, y1, zz, seed), sx);
        corners[k] = mix(a, b, sy);
    }
    return mix(corners[0], corners[1], sz);
}

float fbm(float x, float y, float z, uint32_t seed, int period, int octaves)
{
    float sum = 0.0f, amplitude = 0.5f, total = 0.0f;
    for (int o = 0; o < octaves; o++) {
        sum += value_noise(x, y, z, seed + static_cast<uint32_t>(o) * 131u, period) * amplitude;
        total += amplitude;
        x *= 2.0f;
        y *= 2.0f;
        z *= 2.0f;
        if (period > 0) period *= 2;
        amplitude *= 0.5f;
    }
    return sum / total;
}

uint32_t rgba(float r, float g, float b, float a = 1.0f)
{
    const uint32_t R = static_cast<uint32_t>(saturate(r) * 255.0f + 0.5f);
    const uint32_t G = static_cast<uint32_t>(saturate(g) * 255.0f + 0.5f);
    const uint32_t B = static_cast<uint32_t>(saturate(b) * 255.0f + 0.5f);
    const uint32_t A = static_cast<uint32_t>(saturate(a) * 255.0f + 0.5f);
    return (A << 24u) | (B << 16u) | (G << 8u) | R;
}

// A tiling field of rounded stones (a jittered Voronoi pattern): a height in
// [0, 1] and the stone each texel belongs to.
struct StoneField {
    float    *height = nullptr;
    uint32_t *stone = nullptr;
    uint32_t  size = 0;

    void make(uint32_t texels, int cells, uint32_t seed)
    {
        size = texels;
        height = static_cast<float *>(allocate(sizeof(float) * texels * texels));
        stone = static_cast<uint32_t *>(allocate(sizeof(uint32_t) * texels * texels));
        const float per_texel = static_cast<float>(cells) / static_cast<float>(texels);
        for (uint32_t y = 0; y < texels; y++)
            for (uint32_t x = 0; x < texels; x++) {
                const float px = (static_cast<float>(x) + 0.5f) * per_texel;
                const float py = (static_cast<float>(y) + 0.5f) * per_texel;
                const int cx = static_cast<int>(floor(px)), cy = static_cast<int>(floor(py));
                float f1 = 1e9f, f2 = 1e9f;
                uint32_t id = 0;
                for (int j = -1; j <= 1; j++)
                    for (int i = -1; i <= 1; i++) {
                        const int wx = wrap_index(cx + i, cells), wy = wrap_index(cy + j, cells);
                        const float jx = 0.15f + 0.7f * lattice(wx, wy, 0, seed);
                        const float jy = 0.15f + 0.7f * lattice(wx, wy, 1, seed);
                        const float dx = static_cast<float>(cx + i) + jx - px, dy = static_cast<float>(cy + j) + jy - py;
                        const float d = sqrt(dx * dx + dy * dy);
                        if (d < f1) {
                            f2 = f1;
                            f1 = d;
                            id = static_cast<uint32_t>(wy * cells + wx);
                        } else if (d < f2) {
                            f2 = d;
                        }
                    }
                const float edge = saturate((f2 - f1) / 0.55f);
                const float dome = 1.0f - (1.0f - edge) * (1.0f - edge);
                const float grain = fbm(px * 4.0f, py * 4.0f, 0.0f, seed + 7u, cells * 4, 4);
                const float pits = smoothstep(0.62f, 0.75f, fbm(px * 9.0f, py * 9.0f, 3.0f, seed + 3u, cells * 9, 3));
                height[y * texels + x] = saturate(dome * (0.8f + 0.25f * grain) - 0.12f * pits * dome);
                stone[y * texels + x] = id;
            }
    }

    // Tangent-space normal in rgb, height in alpha.
    uint32_t *normal_height(float strength) const
    {
        uint32_t *texels = static_cast<uint32_t *>(allocate(sizeof(uint32_t) * size * size));
        for (uint32_t y = 0; y < size; y++)
            for (uint32_t x = 0; x < size; x++) {
                const uint32_t l = (x + size - 1) % size, r = (x + 1) % size;
                const uint32_t u = (y + size - 1) % size, d = (y + 1) % size;
                const float dx = (height[y * size + r] - height[y * size + l]) * 0.5f * strength;
                const float dy = (height[d * size + x] - height[u * size + x]) * 0.5f * strength;
                const float3 n = normalize({-dx, -dy, 1.0f});
                texels[y * size + x] = rgba(n.x * 0.5f + 0.5f, n.y * 0.5f + 0.5f, n.z * 0.5f + 0.5f, height[y * size + x]);
            }
        return texels;
    }

    uint32_t *colors(float3 tint, uint32_t seed) const
    {
        uint32_t *texels = static_cast<uint32_t *>(allocate(sizeof(uint32_t) * size * size));
        for (uint32_t y = 0; y < size; y++)
            for (uint32_t x = 0; x < size; x++) {
                const uint32_t id = stone[y * size + x];
                const float shade = 0.55f + 0.45f * lattice(static_cast<int>(id), 5, 9, seed);
                const float grain = fbm(static_cast<float>(x) * 0.12f, static_cast<float>(y) * 0.12f, 1.0f, seed,
                                        static_cast<int>(size) * 12 / 100, 4);
                const float h = height[y * size + x];
                const float mortar = smoothstep(0.0f, 0.25f, h);
                const float3 stone_color = tint * (shade * (0.6f + 0.7f * grain));
                const float3 c = mix3({0.05f, 0.05f, 0.05f}, stone_color, mortar);
                texels[y * size + x] = rgba(c.x, c.y, c.z, 0.3f + 0.7f * grain);
            }
        return texels;
    }

    void release()
    {
        release_memory(height);
        release_memory(stone);
        height = nullptr;
        stone = nullptr;
    }
};

// ---------------------------------------------------------------- meshes

float3 transform_point(float3 p, const float4x4 &m)
{
    return {p.x * m.m[0][0] + p.y * m.m[1][0] + p.z * m.m[2][0] + m.m[3][0],
            p.x * m.m[0][1] + p.y * m.m[1][1] + p.z * m.m[2][1] + m.m[3][1],
            p.x * m.m[0][2] + p.y * m.m[1][2] + p.z * m.m[2][2] + m.m[3][2]};
}

// Front faces are clockwise as seen: wind every triangle to face along its vertices' normals.
void orient(Mesh &mesh)
{
    for (uint32_t t = 0; t + 2 < mesh.index_count; t += 3) {
        uint32_t *i = mesh.indices + t;
        const Vertex &a = mesh.vertices[i[0]], &b = mesh.vertices[i[1]], &c = mesh.vertices[i[2]];
        const float3 n = a.normal + b.normal + c.normal;
        if (dot(cross(b.position - a.position, c.position - a.position), n) < 0.0f) {
            const uint32_t swap = i[1];
            i[1] = i[2];
            i[2] = swap;
        }
    }
}

Mesh new_mesh(uint32_t vertices, uint32_t indices)
{
    Mesh mesh;
    mesh.vertices = static_cast<Vertex *>(allocate(sizeof(Vertex) * vertices));
    mesh.indices = static_cast<uint32_t *>(allocate(sizeof(uint32_t) * indices));
    return mesh;
}

void grid_indices(Mesh &mesh, uint32_t base, uint32_t rows, uint32_t columns)
{
    for (uint32_t r = 0; r < rows; r++)
        for (uint32_t c = 0; c < columns; c++) {
            const uint32_t a = base + r * (columns + 1) + c, b = a + columns + 1;
            uint32_t *i = mesh.indices + mesh.index_count;
            i[0] = a; i[1] = b; i[2] = b + 1;
            i[3] = a; i[4] = b + 1; i[5] = a + 1;
            mesh.index_count += 6;
        }
}

// Along y, centred on the origin, closed at both ends.
Mesh cylinder_mesh(float radius, float height, uint32_t segments)
{
    Mesh mesh = new_mesh((segments + 1) * 4 + 2, segments * 12);
    const float h = height * 0.5f;
    for (uint32_t k = 0; k < 2; k++)
        for (uint32_t s = 0; s <= segments; s++) {
            const float a = static_cast<float>(s) / static_cast<float>(segments) * 2.0f * pi;
            const float3 n = {cos(a), 0.0f, sin(a)};
            mesh.vertices[mesh.vertex_count++] =
                Vertex{{n.x * radius, k == 0 ? -h : h, n.z * radius}, n,
                       {static_cast<float>(s) / static_cast<float>(segments), static_cast<float>(k)}, {1, 1, 1}};
        }
    grid_indices(mesh, 0, 1, segments);
    for (uint32_t k = 0; k < 2; k++) {
        const float y = k == 0 ? -h : h;
        const float3 n = {0.0f, k == 0 ? -1.0f : 1.0f, 0.0f};
        const uint32_t center = mesh.vertex_count;
        mesh.vertices[mesh.vertex_count++] = Vertex{{0, y, 0}, n, {0.5f, 0.5f}, {1, 1, 1}};
        const uint32_t ring = mesh.vertex_count;
        for (uint32_t s = 0; s <= segments; s++) {
            const float a = static_cast<float>(s) / static_cast<float>(segments) * 2.0f * pi;
            mesh.vertices[mesh.vertex_count++] =
                Vertex{{cos(a) * radius, y, sin(a) * radius}, n, {0.5f + 0.5f * cos(a), 0.5f + 0.5f * sin(a)}, {1, 1, 1}};
        }
        for (uint32_t s = 0; s < segments; s++) {
            uint32_t *i = mesh.indices + mesh.index_count;
            i[0] = center; i[1] = ring + s; i[2] = ring + s + 1;
            mesh.index_count += 3;
        }
    }
    orient(mesh);
    return mesh;
}

// A (p, q) torus knot: the stand-in for the originals' scanned models.
Mesh knot_mesh(float scale_by, float tube, uint32_t p, uint32_t q, uint32_t rings, uint32_t sides)
{
    Mesh mesh = new_mesh((rings + 1) * (sides + 1), rings * sides * 6);
    auto curve = [&](float t) {
        const float r = 2.0f + cos(static_cast<float>(q) * t);
        return float3{r * cos(static_cast<float>(p) * t), r * sin(static_cast<float>(p) * t),
                      sin(static_cast<float>(q) * t)} * (scale_by / 3.0f);
    };
    const float e = 0.02f;   // a second difference any smaller drowns in sin's rounding
    for (uint32_t r = 0; r <= rings; r++) {
        const float t = static_cast<float>(r % rings) / static_cast<float>(rings) * 2.0f * pi;
        const float3 c = curve(t), before = curve(t - e), after = curve(t + e);
        const float3 tangent = normalize(after - before);
        const float3 bend = normalize(after + before - c * 2.0f);
        const float3 binormal = normalize(cross(tangent, bend));
        const float3 normal = cross(binormal, tangent);
        for (uint32_t s = 0; s <= sides; s++) {
            const float a = static_cast<float>(s) / static_cast<float>(sides) * 2.0f * pi;
            const float3 n = normal * cos(a) + binormal * sin(a);
            const float cavity = 0.75f + 0.25f * (0.5f - 0.5f * cos(a));
            mesh.vertices[mesh.vertex_count++] =
                Vertex{c + n * tube, n, {static_cast<float>(r) / static_cast<float>(rings) * 16.0f,
                                         static_cast<float>(s) / static_cast<float>(sides)}, {cavity, cavity, cavity}};
        }
    }
    grid_indices(mesh, 0, rings, sides);
    orient(mesh);
    return mesh;
}

// A capsule from a (radius r0) to b (radius r1) with round ends.
Mesh capsule_mesh(float3 a, float3 b, float r0, float r1, uint32_t body, uint32_t sides, float3 color)
{
    const uint32_t cap = 5, total = cap * 2 + body;
    Mesh mesh = new_mesh((total + 1) * (sides + 1), total * sides * 6);
    const float3 axis = normalize(b - a);
    const float3 reference = abs(axis.y) < 0.9f ? float3{0, 1, 0} : float3{1, 0, 0};
    const float3 u = normalize(cross(reference, axis)), w = cross(axis, u);
    for (uint32_t k = 0; k <= total; k++) {
        float3 center;
        float radius, along;
        if (k <= cap) {
            const float theta = -0.5f * pi + 0.5f * pi * static_cast<float>(k) / static_cast<float>(cap);
            center = a + axis * (r0 * sin(theta));
            radius = r0 * cos(theta);
            along = sin(theta);
        } else if (k >= cap + body) {
            const float theta = 0.5f * pi * static_cast<float>(k - cap - body) / static_cast<float>(cap);
            center = b + axis * (r1 * sin(theta));
            radius = r1 * cos(theta);
            along = sin(theta);
        } else {
            const float t = static_cast<float>(k - cap) / static_cast<float>(body);
            center = a + (b - a) * t;
            radius = mix(r0, r1, t);
            along = 0.0f;
        }
        const float across = sqrt(max(0.0f, 1.0f - along * along));
        for (uint32_t s = 0; s <= sides; s++) {
            const float angle = static_cast<float>(s) / static_cast<float>(sides) * 2.0f * pi;
            const float3 radial = u * cos(angle) + w * sin(angle);
            mesh.vertices[mesh.vertex_count++] =
                Vertex{center + radial * radius, normalize(radial * across + axis * along),
                       {static_cast<float>(s) / static_cast<float>(sides), static_cast<float>(k) / static_cast<float>(total)},
                       color};
        }
    }
    grid_indices(mesh, 0, total, sides);
    orient(mesh);
    return mesh;
}

// Merges transformed, recoloured parts into one mesh. Normals go through the
// cofactor matrix, which is right under any scale; `inside` turns a part inside
// out (a room seen from within). Every triangle is then wound to face along its
// normals, whatever winding the part came with.
class MeshBuilder {
public:
    MeshBuilder(uint32_t vertices, uint32_t indices) : vertex_capacity_(vertices), index_capacity_(indices)
    {
        mesh_ = new_mesh(vertices, indices);
    }
    void add(const Mesh &part, const float4x4 &t, float3 color, bool inside = false)
    {
        if (mesh_.vertex_count + part.vertex_count > vertex_capacity_ ||
            mesh_.index_count + part.index_count > index_capacity_) {
            log("lighting: MeshBuilder overflow\n");
            return;
        }
        const float3 r0 = {t.m[0][0], t.m[0][1], t.m[0][2]};
        const float3 r1 = {t.m[1][0], t.m[1][1], t.m[1][2]};
        const float3 r2 = {t.m[2][0], t.m[2][1], t.m[2][2]};
        const float3 c0 = cross(r1, r2), c1 = cross(r2, r0), c2 = cross(r0, r1);
        const float sign = (dot(r0, c0) < 0.0f ? -1.0f : 1.0f) * (inside ? -1.0f : 1.0f);
        const uint32_t base = mesh_.vertex_count;
        for (uint32_t i = 0; i < part.vertex_count; i++) {
            Vertex v = part.vertices[i];
            v.position = transform_point(v.position, t);
            v.normal = normalize((c0 * v.normal.x + c1 * v.normal.y + c2 * v.normal.z) * sign);
            v.color = color;
            mesh_.vertices[mesh_.vertex_count++] = v;
        }
        for (uint32_t i = 0; i < part.index_count; i++) {
            mesh_.indices[mesh_.index_count + i] = base + part.indices[i];
        }
        Mesh added;
        added.vertices = mesh_.vertices;
        added.indices = mesh_.indices + mesh_.index_count;
        added.index_count = part.index_count;
        orient(added);
        mesh_.index_count += part.index_count;
    }
    // Adds and releases a generated part.
    void take(Mesh part, const float4x4 &t, float3 color, bool inside = false)
    {
        add(part, t, color, inside);
        part.release();
    }
    Mesh &mesh() { return mesh_; }

private:
    Mesh     mesh_;
    uint32_t vertex_capacity_;
    uint32_t index_capacity_;
};

const float3 white = {1, 1, 1};

// ---------------------------------------------------------------- GPU helpers

#define MESH_INPUTS                                  \
    "layout(location = 0) in vec3 position;\n"      \
    "layout(location = 1) in vec3 normal;\n"        \
    "layout(location = 2) in vec2 uv;\n"            \
    "layout(location = 3) in vec3 color;\n"

// The fullscreen triangle's corners, as the D3D11 samples' quad_vs makes them.
#define QUAD_VERTEX R"(
out vec2 v_uv;
void main() {
    int id = gl_VertexID;
    v_uv = vec2((id << 1) & 2, id & 2);
    gl_Position = vec4(v_uv * vec2(2, -2) + vec2(-1, 1), 0, 1);
}
)"

void cull(bool back)
{
    if (back) glEnable(GL_CULL_FACE);
    else glDisable(GL_CULL_FACE);
}

// Direct3D's depth-stencil state: test, write, LESS_EQUAL.
void depth_state(bool test, bool write)
{
    if (test) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);
    glDepthMask(write ? GL_TRUE : GL_FALSE);
    glDepthFunc(GL_LEQUAL);
}

// Outside the shadow map counts as lit: the border compares as the far plane.
GLuint compare_sampler()
{
    GLuint made = 0;
    glCreateSamplers(1, &made);
    glSamplerParameteri(made, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glSamplerParameteri(made, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSamplerParameteri(made, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glSamplerParameteri(made, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float border[4] = {1, 1, 1, 1};
    glSamplerParameterfv(made, GL_TEXTURE_BORDER_COLOR, border);
    glSamplerParameteri(made, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glSamplerParameteri(made, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    return made;
}

// Clears a framebuffer's depth whatever the masks say, as Direct3D's does.
void clear_depth(GLuint framebuffer)
{
    glDepthMask(GL_TRUE);
    glClearNamedFramebufferfv(framebuffer, GL_DEPTH, 0, (const float[]){1.0f});
}

// ================================================================ shadowmapping

// A spot light's depth, rendered with slope-scaled bias into a 16-bit shadow
// map, compared in hardware (a shadow sampler) with a 3x3 PCF kernel.
const char *const shadowmapping_common = R"(
layout(std140, binding = 0, row_major) uniform Frame {
    mat4 view_projection;
    mat4 light_view_projection;
    vec4 light_position;
    vec4 shadow_params;   // x: one shadow texel, y/z: the light's near and far planes
};
)";

const char *const shadowmapping_depth_vertex = R"(
layout(location = 0) in vec3 position;
void main() { gl_Position = vec4(position, 1) * light_view_projection; }
)";

const char *const shadowmapping_vertex = MESH_INPUTS R"(
out vec3 v_normal;
out vec3 v_color;
out vec3 v_world;
out vec4 v_shadow;
void main() {
    gl_Position = vec4(position, 1) * view_projection;
    v_normal = normal;
    v_color = color;
    v_world = position;
    v_shadow = vec4(position, 1) * light_view_projection;
}
)";

const char *const shadowmapping_fragment = R"(
layout(binding = 0) uniform sampler2DShadow shadow_map;
in vec3 v_normal;
in vec3 v_color;
in vec3 v_world;
in vec4 v_shadow;
layout(location = 0) out vec4 target;
float lit_fraction(vec4 s) {
    if (s.w <= 0) return 1;
    vec3 c = s.xyz / s.w;
    if (c.z <= 0 || c.z >= 1) return 1;
    vec2 uv = c.xy * vec2(0.5, -0.5) + 0.5;
    float sum = 0;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++)
            sum += texture(shadow_map, vec3(uv + vec2(x, y) * shadow_params.x * 1.5, c.z));
    return sum / 9;
}
void main() {
    vec3 n = normalize(v_normal);
    vec3 l = normalize(light_position.xyz - v_world);
    float shade = mix(0.1, 1.0, lit_fraction(v_shadow));
    target = vec4(max(dot(n, l), 0.1) * v_color * shade, 1);
}
)";

const char *const shadowmapping_quad_fragment = R"(
layout(binding = 1) uniform sampler2D shadow_depth;
in vec2 v_uv;
layout(location = 0) out vec4 target;
void main() {
    float d = textureLod(shadow_depth, v_uv, 0).r;
    float zn = shadow_params.y, zf = shadow_params.z;
    float z = zn * zf / (zf - d * (zf - zn));
    float v = 1 - clamp((z - zn) / (zf - zn), 0.0, 1.0);
    target = vec4(v, v, v, 1);
}
)";

struct ShadowFrame {
    float4x4 view_projection;
    float4x4 light_view_projection;
    float4   light_position;
    float4   shadow_params;
};

class ShadowMapping final : public Sample {
public:
    const char *name() const override { return "shadowmapping"; }
    bool init(Gpu &gpu) override
    {
        static const char *const quad_vertex = QUAD_VERTEX;
        lit_ = gpu.program("shadowmapping", {.common = shadowmapping_common, .vertex = shadowmapping_vertex,
                                             .fragment = shadowmapping_fragment});
        depth_ = gpu.program("shadowmapping_depth", {.common = shadowmapping_common,
                                                     .vertex = shadowmapping_depth_vertex});
        quad_ = gpu.program("shadowmapping_quad", {.common = shadowmapping_common, .vertex = quad_vertex,
                                                   .fragment = shadowmapping_quad_fragment});
        MeshBuilder scene(40000, 200000);
        const float3 grey = {0.62f, 0.62f, 0.62f};
        Mesh cube = cube_mesh(1.0f);
        scene.add(cube, scale({16.0f, 0.6f, 12.0f}) * translate({0, -0.3f, 0}), grey);
        scene.take(knot_mesh(2.1f, 0.42f, 2, 3, 240, 20), rotate_x(0.5f * pi) * translate({0.4f, 2.7f, 1.4f}), grey);
        scene.take(sphere_mesh(1.0f, 40, 20), scale({1.1f, 0.9f, 1.1f}) * translate({0.3f, 0.9f, -2.2f}), grey);
        scene.take(cylinder_mesh(0.35f, 3.6f, 24), rotate_z(0.35f) * translate({3.6f, 1.6f, -0.6f}), grey);
        scene.add(cube, scale({1.2f, 1.2f, 1.9f}) * rotate_y(0.4f) * translate({4.3f, 3.4f, -0.4f}), grey);
        scene.take(torus_mesh(0.9f, 0.35f, 48, 20), rotate_x(0.3f) * translate({-3.6f, 1.1f, -1.0f}), grey);
        scene.take(sphere_mesh(0.8f, 32, 16), translate({-3.2f, 0.8f, 2.4f}), grey);
        scene.add(cube, scale({1.3f, 2.6f, 1.3f}) * rotate_y(0.7f) * translate({-0.3f, 1.3f, 5.0f}), grey);
        cube.release();
        mesh_ = gpu.upload(scene.mesh());
        scene.mesh().release();
        shadow_ = gpu.render_texture(GL_DEPTH_COMPONENT16, shadow_size, shadow_size);
        shadow_target_ = gpu.framebuffer(nullptr, 0, shadow_);
        compare_ = compare_sampler();
        clamp_ = gpu.sampler(GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, 1.0f);
        constants_ = gpu.uniform_buffer(sizeof(ShadowFrame));
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        const float a = time * 0.8f;
        const float3 light = {cos(a) * 11.0f, 20.0f + sin(a) * 4.0f, -9.0f + sin(a) * 3.0f};
        ShadowFrame frame;
        frame.light_view_projection = look_at(light, {0, 0, 0}, {0, 1, 0}) * perspective(radians(55), 1.0f, z_near, z_far);
        frame.view_projection =
            look_at({8.5f, 8.0f, -12.0f}, {0.5f, 1.2f, 0.8f}, {0, 1, 0}) * perspective(radians(45), gpu.aspect(), 0.5f, 100);
        frame.light_position = {light.x, light.y, light.z, 1};
        frame.shadow_params = {1.0f / static_cast<float>(shadow_size), z_near, z_far, 0};
        gpu.update(constants_, &frame, sizeof frame);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, constants_);

        glBindFramebuffer(GL_FRAMEBUFFER, shadow_target_);
        clear_depth(shadow_target_);
        gpu.viewport(0, 0, static_cast<float>(shadow_size), static_cast<float>(shadow_size));
        // Vulkan's depthBiasConstant 1.25 and slope 1.75, in units of the 16-bit map.
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.75f, 2.0f);
        glUseProgram(depth_);
        mesh_.draw();
        glDisable(GL_POLYGON_OFFSET_FILL);

        gpu.begin_frame(0.03f, 0.03f, 0.03f);
        glUseProgram(lit_);
        glBindTextureUnit(0, shadow_);
        glBindSampler(0, compare_);
        glBindTextureUnit(1, shadow_);
        glBindSampler(1, clamp_);
        mesh_.draw();

        const float inset = static_cast<float>(gpu.height) * 0.36f;
        gpu.viewport(12.0f, 12.0f, inset, inset);
        glUseProgram(quad_);
        gpu.draw_fullscreen();
    }
    void release() override
    {
        glDeleteProgram(lit_);
        glDeleteProgram(depth_);
        glDeleteProgram(quad_);
        mesh_.release();
        glDeleteFramebuffers(1, &shadow_target_);
        glDeleteTextures(1, &shadow_);
        glDeleteSamplers(1, &compare_);
        glDeleteSamplers(1, &clamp_);
        glDeleteBuffers(1, &constants_);
    }

private:
    static constexpr uint32_t shadow_size = 2048;
    static constexpr float    z_near = 1.0f;
    static constexpr float    z_far = 60.0f;
    GLuint   lit_ = 0;
    GLuint   depth_ = 0;
    GLuint   quad_ = 0;
    DrawMesh mesh_;
    GLuint   shadow_ = 0;
    GLuint   shadow_target_ = 0;
    GLuint   compare_ = 0;
    GLuint   clamp_ = 0;
    GLuint   constants_ = 0;
};

// ================================================================ shadowmappingomni

// A point light's distance to everything, drawn into the six faces of an R32F
// cube map (one framebuffer per face) and looked up by direction.
const char *const shadowmappingomni_common = R"(
layout(std140, binding = 0, row_major) uniform Frame {
    mat4 view_projection;
    mat4 faces[6];
    vec4 light_position;
};
)";

const char *const shadowmappingomni_vertex = MESH_INPUTS R"(
out vec3 v_normal;
out vec3 v_color;
out vec3 v_world;
void main() {
    gl_Position = vec4(position, 1) * view_projection;
    v_normal = normal;
    v_color = color;
    v_world = position;
}
)";

const char *const shadowmappingomni_fragment = R"(
layout(binding = 0) uniform samplerCube distances;
in vec3 v_normal;
in vec3 v_color;
in vec3 v_world;
layout(location = 0) out vec4 target;
void main() {
    vec3 to_light = light_position.xyz - v_world;
    float dist = length(to_light);
    vec3 n = normalize(v_normal);
    float diffuse = max(dot(n, to_light / dist), 0);
    float attenuation = 1.6 / (1 + 0.006 * dist * dist);
    float stored = textureLod(distances, -to_light, 0).r;
    float shadow = dist <= stored + 0.15 ? 1 : 0.3;
    target = vec4((0.05 + diffuse * attenuation) * v_color * shadow, 1);
}
)";

const char *const shadowmappingomni_depth_vertex = R"(
layout(std140, binding = 1) uniform Face { uint face; };
layout(location = 0) in vec3 position;
out vec3 v_world;
void main() {
    gl_Position = vec4(position, 1) * faces[face];
    v_world = position;
}
)";

const char *const shadowmappingomni_distance_fragment = R"(
in vec3 v_world;
layout(location = 0) out float distance_to_light;
void main() { distance_to_light = length(v_world - light_position.xyz); }
)";

struct OmniFrame {
    float4x4 view_projection;
    float4x4 faces[6];
    float4   light_position;
};

class ShadowMappingOmni final : public Sample {
public:
    const char *name() const override { return "shadowmappingomni"; }
    bool init(Gpu &gpu) override
    {
        lit_ = gpu.program("shadowmappingomni", {.common = shadowmappingomni_common,
                                                 .vertex = shadowmappingomni_vertex,
                                                 .fragment = shadowmappingomni_fragment});
        distance_ = gpu.program("shadowmappingomni_distance", {.common = shadowmappingomni_common,
                                                               .vertex = shadowmappingomni_depth_vertex,
                                                               .fragment = shadowmappingomni_distance_fragment});
        MeshBuilder scene(20000, 100000);
        Mesh cube = cube_mesh(1.0f);
        scene.add(cube, scale({24.0f, 15.0f, 24.0f}) * translate({0, 7.5f, 0}), {0.85f, 0.8f, 0.7f}, true);
        const float3 iron = {0.2f, 0.2f, 0.2f};
        const uint32_t bars = 14;
        for (uint32_t i = 0; i < bars; i++) {
            const float a = static_cast<float>(i) / static_cast<float>(bars) * 2.0f * pi;
            scene.add(cube, scale({0.16f, 5.2f, 0.16f}) * translate({cos(a) * 1.5f, 2.6f, sin(a) * 1.5f}), iron);
        }
        Mesh ring = torus_mesh(1.5f, 0.1f, 64, 10);
        const float heights[4] = {0.15f, 1.6f, 3.9f, 5.2f};
        for (float y : heights) scene.add(ring, translate({0, y, 0}), iron);
        ring.release();
        scene.take(cylinder_mesh(0.6f, 0.3f, 32), translate({0, 0.15f, 0}), iron);
        cube.release();
        mesh_ = gpu.upload(scene.mesh());
        scene.mesh().release();
        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &distances_);
        glTextureStorage2D(distances_, 1, GL_R32F, cube_size, cube_size);
        depth_ = gpu.render_texture(GL_DEPTH_COMPONENT32F, cube_size, cube_size);
        for (uint32_t f = 0; f < 6; f++) {
            const uint32_t index[4] = {f, 0, 0, 0};
            face_constants_[f] = gpu.buffer(index, sizeof index);
            glCreateFramebuffers(1, &face_targets_[f]);
            glNamedFramebufferTextureLayer(face_targets_[f], GL_COLOR_ATTACHMENT0, distances_, 0, static_cast<GLint>(f));
            glNamedFramebufferTexture(face_targets_[f], GL_DEPTH_ATTACHMENT, depth_, 0);
            if (glCheckNamedFramebufferStatus(face_targets_[f], GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                gpu.failed = true;
        }
        point_ = gpu.sampler(GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, 1.0f);
        constants_ = gpu.uniform_buffer(sizeof(OmniFrame));
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        const float a = time * 1.2f;
        const float3 light = {sin(a) * 0.35f, 2.8f, cos(a) * 0.35f};
        OmniFrame frame;
        frame.view_projection =
            look_at({-17.0f, 19.0f, -17.0f}, {0, 3.5f, 0}, {0, 1, 0}) * perspective(radians(50), gpu.aspect(), 0.5f, 200);
        const float3 directions[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        const float3 ups[6] = {{0, 1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {0, 1, 0}, {0, 1, 0}};
        const float4x4 projection = perspective(0.5f * pi, 1.0f, 0.1f, 64.0f);
        for (int f = 0; f < 6; f++) frame.faces[f] = look_at(light, light + directions[f], ups[f]) * projection;
        frame.light_position = {light.x, light.y, light.z, 1};
        gpu.update(constants_, &frame, sizeof frame);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, constants_);

        const float far_away[4] = {1000.0f, 1000.0f, 1000.0f, 1000.0f};
        gpu.viewport(0, 0, static_cast<float>(cube_size), static_cast<float>(cube_size));
        cull(false);
        glUseProgram(distance_);
        for (uint32_t f = 0; f < 6; f++) {
            glBindFramebuffer(GL_FRAMEBUFFER, face_targets_[f]);
            glClearNamedFramebufferfv(face_targets_[f], GL_COLOR, 0, far_away);
            clear_depth(face_targets_[f]);
            glBindBufferBase(GL_UNIFORM_BUFFER, 1, face_constants_[f]);
            mesh_.draw();
        }

        gpu.begin_frame(0.03f, 0.03f, 0.03f);
        cull(true);
        glUseProgram(lit_);
        glBindTextureUnit(0, distances_);
        glBindSampler(0, point_);
        mesh_.draw();
    }
    void release() override
    {
        glDeleteProgram(lit_);
        glDeleteProgram(distance_);
        glDeleteBuffers(6, face_constants_);
        glDeleteFramebuffers(6, face_targets_);
        mesh_.release();
        glDeleteTextures(1, &distances_);
        glDeleteTextures(1, &depth_);
        glDeleteSamplers(1, &point_);
        glDeleteBuffers(1, &constants_);
    }

private:
    static constexpr uint32_t cube_size = 1024;
    GLuint   face_constants_[6] = {};
    GLuint   face_targets_[6] = {};
    GLuint   lit_ = 0;
    GLuint   distance_ = 0;
    DrawMesh mesh_;
    GLuint   distances_ = 0;
    GLuint   depth_ = 0;
    GLuint   point_ = 0;
    GLuint   constants_ = 0;
};

// ================================================================ deferred

// A knight made of primitives: legs, armoured torso, outstretched arms, a
// sword in one hand and a shield in the other.
void build_knight(MeshBuilder &b)
{
    const float3 steel = {0.85f, 0.85f, 0.88f}, dark = {0.45f, 0.45f, 0.48f}, gold = {1.0f, 0.55f, 0.15f};
    Mesh cube = cube_mesh(1.0f);
    for (int side = -1; side <= 1; side += 2) {
        const float s = static_cast<float>(side);
        b.take(capsule_mesh({0.25f * s, 1.55f, 0}, {0.3f * s, 0.85f, 0}, 0.2f, 0.16f, 4, 16, white), float4x4::identity(), steel);
        b.take(capsule_mesh({0.3f * s, 0.85f, 0}, {0.32f * s, 0.2f, 0}, 0.16f, 0.15f, 4, 16, white), float4x4::identity(), steel);
        b.add(cube, scale({0.3f, 0.2f, 0.5f}) * translate({0.32f * s, 0.1f, -0.08f}), dark);
        b.take(sphere_mesh(0.27f, 20, 10), scale({1.2f, 0.8f, 1.1f}) * translate({0.6f * s, 2.9f, 0}), steel);
        b.take(capsule_mesh({0.62f * s, 2.8f, 0}, {1.15f * s, 2.55f, -0.1f}, 0.14f, 0.12f, 4, 14, white), float4x4::identity(), steel);
        b.take(capsule_mesh({1.15f * s, 2.55f, -0.1f}, {1.65f * s, 2.35f, -0.2f}, 0.12f, 0.1f, 4, 14, white), float4x4::identity(), steel);
        b.take(sphere_mesh(0.12f, 12, 8), translate({1.75f * s, 2.3f, -0.22f}), dark);
    }
    b.add(cube, scale({0.8f, 0.4f, 0.5f}) * translate({0, 1.55f, 0}), dark);
    b.take(capsule_mesh({0, 1.8f, 0}, {0, 2.7f, 0}, 0.38f, 0.46f, 6, 20, white), scale({1.0f, 1.0f, 0.75f}), steel);
    b.take(sphere_mesh(0.1f, 14, 8), translate({0, 2.35f, -0.36f}), gold);
    b.take(sphere_mesh(0.28f, 24, 12), scale({1.0f, 1.2f, 1.05f}) * translate({0, 3.3f, 0}), steel);
    b.add(cube, scale({0.05f, 0.25f, 0.55f}) * translate({0, 3.6f, 0}), gold);
    // sword, in the hand on -x, pointing up and out
    const float4x4 sword = rotate_z(0.9f) * translate({-1.75f, 2.3f, -0.22f});
    b.add(cube, scale({0.08f, 1.6f, 0.03f}) * translate({0, 0.95f, 0}) * sword, {1.0f, 1.0f, 1.0f});
    b.add(cube, scale({0.45f, 0.07f, 0.08f}) * translate({0, 0.15f, 0}) * sword, gold);
    // shield on +x, facing the viewer
    b.take(cylinder_mesh(0.7f, 0.1f, 6), scale({1.0f, 1.0f, 1.3f}) * rotate_x(0.5f * pi) * rotate_y(-0.2f) *
                                              translate({1.95f, 2.2f, -0.4f}), dark);
    b.take(sphere_mesh(0.14f, 12, 8), scale({1, 1, 0.5f}) * translate({1.92f, 2.2f, -0.49f}), gold);
    cube.release();
}

const char *const deferred_common = R"(
layout(std140, binding = 0, row_major) uniform Frame {
    mat4 view_projection;
    vec4 view_position;
    vec4 light_positions[5];   // w: radius
    vec4 light_colors[5];
};
)";

const char *const deferred_vertex = MESH_INPUTS R"(
out vec3 v_world;
out vec3 v_normal;
out vec2 v_uv;
out vec3 v_color;
void main() {
    gl_Position = vec4(position, 1) * view_projection;
    v_world = position;
    v_normal = normal;
    v_uv = uv;
    v_color = color;
}
)";

const char *const deferred_gbuffer_fragment = R"(
layout(binding = 0) uniform sampler2D albedo_map;
in vec3 v_world;
in vec3 v_normal;
in vec2 v_uv;
in vec3 v_color;
layout(location = 0) out vec4 position_target;
layout(location = 1) out vec4 normal_target;
layout(location = 2) out vec4 albedo_target;
void main() {
    vec4 a = texture(albedo_map, v_uv * vec2(2, 4));
    position_target = vec4(v_world, 1);
    normal_target = vec4(normalize(v_normal), 1);
    albedo_target = vec4(a.rgb * v_color, a.a);
}
)";

const char *const deferred_compose_fragment = R"(
layout(binding = 1) uniform sampler2D gbuffer_position;
layout(binding = 2) uniform sampler2D gbuffer_normal;
layout(binding = 3) uniform sampler2D gbuffer_albedo;
layout(location = 0) out vec4 target;
vec3 shade(vec3 position, vec3 normal, vec4 albedo) {
    vec3 color = albedo.rgb * 0.05;
    vec3 view = normalize(view_position.xyz - position);
    for (int i = 0; i < 5; i++) {
        vec3 to_light = light_positions[i].xyz - position;
        float dist = length(to_light);
        if (dist < light_positions[i].w) {
            vec3 l = to_light / dist;
            vec3 diffuse = max(dot(normal, l), 0) * albedo.rgb * light_colors[i].rgb;
            vec3 h = normalize(l + view);
            vec3 specular = light_colors[i].rgb * pow(max(dot(normal, h), 0), 16) * albedo.a * 0.6;
            color += (diffuse + specular) / (1 + 0.3 * dist + 0.1 * dist * dist);
        }
    }
    return color;
}
// The four quadrants show the G-buffer's position, normal and albedo, and the lit composition.
void main() {
    uvec2 size = uvec2(textureSize(gbuffer_position, 0));
    uvec2 half_size = size / 2;
    uvec2 at = uvec2(gl_FragCoord.xy);
    uvec2 quadrant = uvec2(at.x >= half_size.x, at.y >= half_size.y);
    ivec2 texel = ivec2((at - quadrant * half_size) * 2);
    vec4 position = texelFetch(gbuffer_position, texel, 0);
    vec4 normal = texelFetch(gbuffer_normal, texel, 0);
    vec4 albedo = texelFetch(gbuffer_albedo, texel, 0);
    if (normal.w == 0) { target = vec4(0, 0, 0, 1); return; }
    if (quadrant.y == 0) {
        target = quadrant.x == 0 ? vec4(clamp(position.xyz * vec3(0.45, 0.28, -0.8) + vec3(0.5, 0, 0.5), 0.0, 1.0), 1)
                                 : vec4(clamp(normal.xyz * vec3(1, 1, -1), 0.0, 1.0), 1);
        return;
    }
    if (quadrant.x == 0) { target = vec4(albedo.rgb, 1); return; }
    target = vec4(shade(position.xyz, normalize(normal.xyz), albedo), 1);
}
)";

struct DeferredFrame {
    float4x4 view_projection;
    float4   view_position;
    float4   light_positions[5];
    float4   light_colors[5];
};

class Deferred final : public Sample {
public:
    const char *name() const override { return "deferred"; }
    bool init(Gpu &gpu) override
    {
        static const char *const quad_vertex = QUAD_VERTEX;
        gbuffer_program_ = gpu.program("deferred_gbuffer", {.common = deferred_common, .vertex = deferred_vertex,
                                                            .fragment = deferred_gbuffer_fragment});
        compose_ = gpu.program("deferred_compose", {.common = deferred_common, .vertex = quad_vertex,
                                                    .fragment = deferred_compose_fragment});
        MeshBuilder knight(40000, 200000);
        build_knight(knight);
        mesh_ = gpu.upload(knight.mesh());
        knight.mesh().release();
        const uint32_t size = 256;
        uint32_t *texels = static_cast<uint32_t *>(allocate(size * size * 4));
        for (uint32_t y = 0; y < size; y++)
            for (uint32_t x = 0; x < size; x++) {
                const float fx = static_cast<float>(x) / static_cast<float>(size), fy = static_cast<float>(y) / static_cast<float>(size);
                const float grain = fbm(fx * 16.0f, fy * 16.0f, 0.0f, 11u, 16, 5);
                const float scratch = smoothstep(0.7f, 0.9f, fbm(fx * 64.0f, fy * 4.0f, 1.0f, 12u, 64, 2));
                const float v = 0.55f + 0.45f * grain - 0.15f * scratch;
                texels[y * size + x] = rgba(v, v, v * 0.97f, 0.4f + 0.6f * grain);
            }
        albedo_ = gpu.texture(texels, size, size);
        release_memory(texels);
        const GLenum formats[3] = {GL_RGBA16F, GL_RGBA16F, GL_RGBA8};
        for (int i = 0; i < 3; i++) gbuffer_[i] = gpu.render_texture(formats[i], gpu.width, gpu.height);
        target_ = gpu.framebuffer(gbuffer_, 3, gpu.depth);
        sampler_ = gpu.sampler();
        constants_ = gpu.uniform_buffer(sizeof(DeferredFrame));
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        DeferredFrame frame;
        const float3 eye = {0.0f, 2.0f, -4.9f};
        frame.view_projection = look_at(eye, {0, 1.9f, 0}, {0, 1, 0}) * perspective(radians(60), gpu.aspect(), 0.1f, 64);
        frame.view_position = {eye.x, eye.y, eye.z, 1};
        const float3 colors[5] = {{0.9f, 0.9f, 0.9f}, {1.8f, 0.2f, 0.1f}, {0.2f, 0.4f, 2.0f}, {0.2f, 1.6f, 0.3f}, {1.8f, 1.2f, 0.1f}};
        for (int i = 0; i < 5; i++) {
            const float a = time * (0.6f + 0.15f * static_cast<float>(i)) + static_cast<float>(i) * 1.3f;
            const float radius = i == 0 ? 1.2f : 2.4f;
            frame.light_positions[i] = {sin(a) * radius, 1.2f + static_cast<float>(i) * 0.5f, -1.2f + cos(a) * radius, 8.0f};
            frame.light_colors[i] = {colors[i].x, colors[i].y, colors[i].z, 1};
        }
        gpu.update(constants_, &frame, sizeof frame);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, constants_);

        const float clear[4] = {0, 0, 0, 0};
        glBindFramebuffer(GL_FRAMEBUFFER, target_);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        for (GLint i = 0; i < 3; i++) glClearNamedFramebufferfv(target_, GL_COLOR, i, clear);
        clear_depth(target_);
        gpu.viewport(0, 0, static_cast<float>(gpu.width), static_cast<float>(gpu.height));
        depth_state(true, true);
        glDepthFunc(GL_LESS);
        glUseProgram(gbuffer_program_);
        glBindTextureUnit(0, albedo_);
        glBindSampler(0, sampler_);
        mesh_.draw();

        // The composition has no depth buffer, as Direct3D's has none bound.
        const float black[4] = {0, 0, 0, 1};
        glBindFramebuffer(GL_FRAMEBUFFER, gpu.target);
        glClearNamedFramebufferfv(gpu.target, GL_COLOR, 0, black);
        glDisable(GL_DEPTH_TEST);
        for (GLuint i = 0; i < 3; i++) {
            glBindTextureUnit(1 + i, gbuffer_[i]);
            glBindSampler(1 + i, 0);
        }
        glUseProgram(compose_);
        gpu.draw_fullscreen();
        glEnable(GL_DEPTH_TEST);
    }
    void release() override
    {
        glDeleteProgram(gbuffer_program_);
        glDeleteProgram(compose_);
        mesh_.release();
        glDeleteTextures(1, &albedo_);
        glDeleteFramebuffers(1, &target_);
        glDeleteTextures(3, gbuffer_);
        glDeleteSamplers(1, &sampler_);
        glDeleteBuffers(1, &constants_);
    }

private:
    GLuint   gbuffer_program_ = 0;
    GLuint   compose_ = 0;
    DrawMesh mesh_;
    GLuint   albedo_ = 0;
    GLuint   gbuffer_[3] = {};
    GLuint   target_ = 0;
    GLuint   sampler_ = 0;
    GLuint   constants_ = 0;
};

// ================================================================ parallaxmapping

// Left: parallax offset mapping (the original's technique). Right: parallax
// occlusion mapping, a ray march through the height field with textureGrad.
const char *const parallaxmapping_common = R"(
layout(std140, binding = 0, row_major) uniform Frame {
    mat4 view_projection;
    vec4 eye;
    vec4 light;    // w: radius
    vec4 params;   // x: offset scale, y: offset bias, z: occlusion depth
};
)";

const char *const parallaxmapping_vertex = MESH_INPUTS R"(
out vec3 v_world;
out vec2 v_uv;
void main() {
    gl_Position = vec4(position, 1) * view_projection;
    v_world = position;
    v_uv = uv;
}
)";

#define PARALLAX_SHADE R"(
layout(binding = 0) uniform sampler2D color_map;
layout(binding = 1) uniform sampler2D normal_height_map;
in vec3 v_world;
in vec2 v_uv;
layout(location = 0) out vec4 target;
// The floor's tangent frame: u runs along world x, v along world z, the normal is world y.
vec3 to_tangent(vec3 w) { return vec3(w.x, w.z, w.y); }
vec3 shade(vec2 uv, vec3 world, vec2 dx, vec2 dy) {
    vec3 n = normalize(textureGrad(normal_height_map, uv, dx, dy).rgb * 2 - 1);
    vec3 rgb = textureGrad(color_map, uv, dx, dy).rgb;
    vec3 to_light = to_tangent(light.xyz - world);
    float dist = length(to_light);
    vec3 l = to_light / dist;
    vec3 v = normalize(to_tangent(eye.xyz - world));
    float diffuse = clamp(dot(n, l), 0.0, 1.0);
    float specular = pow(clamp(dot(reflect(-l, n), v), 0.0, 1.0), 12) * 0.25;
    float attenuation = clamp(1 - dist / light.w, 0.0, 1.0);
    return (rgb * 0.15 + rgb * diffuse * 1.5 + specular) * attenuation;
}
)"

const char *const parallaxmapping_offset = PARALLAX_SHADE R"(
void main() {
    vec3 v = normalize(to_tangent(eye.xyz - v_world));
    vec2 dx = dFdx(v_uv), dy = dFdy(v_uv);
    float h = texture(normal_height_map, v_uv).a * params.x + params.y;
    target = vec4(shade(v_uv + h * v.xy, v_world, dx, dy), 1);
}
)";

const char *const parallaxmapping_occlusion = PARALLAX_SHADE R"(
void main() {
    vec3 v = normalize(to_tangent(eye.xyz - v_world));
    vec2 dx = dFdx(v_uv), dy = dFdy(v_uv);
    const int steps = 48;
    float layer = 1.0 / steps;
    vec2 delta = v.xy / max(v.z, 0.15) * params.z / steps;
    vec2 uv = v_uv;
    float depth = 0;
    float surface = 1 - textureGrad(normal_height_map, uv, dx, dy).a;
    for (int i = 0; i < steps && depth < surface; i++) {
        uv -= delta;
        depth += layer;
        surface = 1 - textureGrad(normal_height_map, uv, dx, dy).a;
    }
    vec2 previous = uv + delta;
    float after = surface - depth;
    float before = (1 - textureGrad(normal_height_map, previous, dx, dy).a) - (depth - layer);
    float weight = after / (after - before + 1e-5);
    uv = mix(uv, previous, clamp(weight, 0.0, 1.0));
    target = vec4(shade(uv, v_world, dx, dy), 1);
}
)";

struct ParallaxFrame {
    float4x4 view_projection;
    float4   eye;
    float4   light;
    float4   params;
};

class ParallaxMapping final : public Sample {
public:
    const char *name() const override { return "parallaxmapping"; }
    bool init(Gpu &gpu) override
    {
        offset_ = gpu.program("parallaxmapping_offset", {.common = parallaxmapping_common,
                                                         .vertex = parallaxmapping_vertex,
                                                         .fragment = parallaxmapping_offset});
        occlusion_ = gpu.program("parallaxmapping_occlusion", {.common = parallaxmapping_common,
                                                               .vertex = parallaxmapping_vertex,
                                                               .fragment = parallaxmapping_occlusion});
        Mesh plane = plane_mesh(10.0f, 1, 3.0f);
        plane_ = gpu.upload(plane);
        plane.release();
        StoneField stones;
        stones.make(512, 6, 21u);
        uint32_t *normals = stones.normal_height(24.0f);
        uint32_t *colors = stones.colors({0.62f, 0.6f, 0.58f}, 22u);
        normal_height_ = gpu.texture(normals, 512, 512);
        color_ = gpu.texture(colors, 512, 512);
        release_memory(normals);
        release_memory(colors);
        stones.release();
        sampler_ = gpu.sampler();
        constants_ = gpu.uniform_buffer(sizeof(ParallaxFrame));
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        gpu.begin_frame(0.0f, 0.0f, 0.0f);
        const float half = static_cast<float>(gpu.width) * 0.5f, h = static_cast<float>(gpu.height);
        const float3 eye = {-0.6f, 1.9f, -1.5f};
        ParallaxFrame frame;
        frame.view_projection = rotate_y(time * 0.1f) * look_at(eye, {0.2f, 0, 0.5f}, {0, 1, 0}) *
                                perspective(radians(60), half / h, 0.05f, 50);
        // The camera turns with the floor: the eye is fixed in the floor's frame, so rotate it back.
        const float4x4 turn = rotate_y(-time * 0.1f);
        const float3 world_eye = transform_point(eye, turn);
        frame.eye = {world_eye.x, world_eye.y, world_eye.z, 1};
        const float a = time * 0.9f;
        frame.light = {0.4f + sin(a) * 1.4f, 0.9f, 0.2f + cos(a) * 1.1f, 4.0f};
        frame.params = {0.05f, -0.03f, 0.08f, 0};
        gpu.update(constants_, &frame, sizeof frame);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, constants_);
        glBindTextureUnit(0, color_);
        glBindTextureUnit(1, normal_height_);
        glBindSampler(0, sampler_);
        glBindSampler(1, sampler_);
        gpu.viewport(0, 0, half, h);
        glUseProgram(offset_);
        plane_.draw();
        gpu.viewport(half, 0, half, h);
        glUseProgram(occlusion_);
        plane_.draw();
    }
    void release() override
    {
        glDeleteProgram(offset_);
        glDeleteProgram(occlusion_);
        plane_.release();
        glDeleteTextures(1, &normal_height_);
        glDeleteTextures(1, &color_);
        glDeleteSamplers(1, &sampler_);
        glDeleteBuffers(1, &constants_);
    }

private:
    GLuint   offset_ = 0;
    GLuint   occlusion_ = 0;
    DrawMesh plane_;
    GLuint   normal_height_ = 0;
    GLuint   color_ = 0;
    GLuint   sampler_ = 0;
    GLuint   constants_ = 0;
};

// ================================================================ sphericalenvmapping

// Material caps: pictures of a lit sphere, looked up by the view-space
// reflection vector, one per layer of a texture array.
const char *const sphericalenvmapping_common = R"(
layout(std140, binding = 0, row_major) uniform Frame {
    mat4 world_view;
    mat4 projection;
    vec4 layer;
};
)";

const char *const sphericalenvmapping_vertex = MESH_INPUTS R"(
out vec3 v_eye;
out vec3 v_normal;
out vec3 v_color;
void main() {
    vec4 p = vec4(position, 1) * world_view;
    v_eye = p.xyz;
    v_normal = (vec4(normal, 0) * world_view).xyz;
    v_color = color;
    gl_Position = p * projection;
}
)";

const char *const sphericalenvmapping_fragment = R"(
layout(binding = 0) uniform sampler2DArray matcaps;
in vec3 v_eye;
in vec3 v_normal;
in vec3 v_color;
layout(location = 0) out vec4 target;
void main() {
    vec3 r = reflect(normalize(v_eye), normalize(v_normal));
    r.z = -r.z;   // the matcap formula is written for a view down -z
    float m = 2 * sqrt(r.x * r.x + r.y * r.y + (r.z + 1) * (r.z + 1));
    vec2 uv = vec2(r.x / m + 0.5, 0.5 - r.y / m);
    target = vec4(texture(matcaps, vec3(uv, layer.x)).rgb * clamp(v_color.r * 2, 0.0, 1.0), 1);
}
)";

struct EnvFrame {
    float4x4 world_view;
    float4x4 projection;
    float4   layer;
};

// A lit sphere as seen head-on: `kind` 0 pearl, 1 gold, 2 jade, 3 chrome.
uint32_t matcap_texel(int kind, float nx, float ny)
{
    const float r2 = nx * nx + ny * ny;
    if (r2 > 0.998f) {
        const float scale_to = sqrt(0.998f / r2);
        nx *= scale_to;
        ny *= scale_to;
    }
    const float3 n = {nx, ny, sqrt(max(0.0f, 1.0f - nx * nx - ny * ny))};
    const float3 key = normalize({-0.45f, 0.6f, 0.65f}), fill = normalize({0.7f, -0.2f, 0.5f});
    const float3 half_vector = normalize(key + float3{0, 0, 1});
    const float diffuse = max(dot(n, key), 0.0f), fill_light = max(dot(n, fill), 0.0f);
    const float specular = power(max(dot(n, half_vector), 0.0f), 48);
    const float rim = power(1.0f - n.z, 3);
    float3 c;
    if (kind == 0) {
        c = float3{0.55f, 0.46f, 0.46f} * (0.35f + 0.8f * diffuse) + float3{0.35f, 0.15f, 0.2f} * fill_light +
            float3{0.9f, 0.45f, 0.6f} * rim + white * (specular * 0.9f);
    } else if (kind == 1) {
        c = float3{0.85f, 0.55f, 0.12f} * (0.2f + 0.9f * diffuse) + float3{0.4f, 0.2f, 0.05f} * rim +
            float3{1.0f, 0.9f, 0.6f} * specular;
    } else if (kind == 2) {
        c = float3{0.15f, 0.55f, 0.3f} * (0.3f + 0.8f * diffuse) + float3{0.5f, 0.9f, 0.7f} * (rim * 0.6f) +
            white * (specular * 0.7f);
    } else {
        const float sky = smoothstep(-0.1f, 0.25f, n.y);
        c = mix3({0.25f, 0.18f, 0.12f}, {0.75f, 0.85f, 1.0f}, sky) * (0.6f + 0.4f * n.z) + white * (specular * 1.2f);
    }
    return rgba(c.x, c.y, c.z);
}


class SphericalEnvMapping final : public Sample {
public:
    const char *name() const override { return "sphericalenvmapping"; }
    bool init(Gpu &gpu) override
    {
        program_ = gpu.program("sphericalenvmapping", {.common = sphericalenvmapping_common,
                                                       .vertex = sphericalenvmapping_vertex,
                                                       .fragment = sphericalenvmapping_fragment});
        Mesh knot = knot_mesh(2.2f, 0.5f, 3, 4, 400, 28);
        mesh_ = gpu.upload(knot);
        knot.release();
        const uint32_t size = 256;
        uint32_t *texels = static_cast<uint32_t *>(allocate(size * size * 4 * layers));
        for (uint32_t k = 0; k < layers; k++)
            for (uint32_t y = 0; y < size; y++)
                for (uint32_t x = 0; x < size; x++) {
                    const float nx = (static_cast<float>(x) + 0.5f) / static_cast<float>(size) * 2.0f - 1.0f;
                    const float ny = 1.0f - (static_cast<float>(y) + 0.5f) / static_cast<float>(size) * 2.0f;
                    texels[(k * size + y) * size + x] = matcap_texel(static_cast<int>(k), nx, ny);
                }
        matcaps_ = gpu.texture(texels, size, size, layers);
        release_memory(texels);
        sampler_ = gpu.sampler(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, 1.0f);
        constants_ = gpu.uniform_buffer(sizeof(EnvFrame));
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        gpu.begin_frame(0.0f, 0.0f, 0.0f);
        EnvFrame frame;
        frame.world_view = rotate_y(time * 0.5f) * rotate_x(0.35f) * translate({0, 0, 6.5f});
        frame.projection = perspective(radians(60), gpu.aspect(), 0.1f, 100);
        frame.layer = {static_cast<float>(static_cast<uint32_t>(time / 4.0f) % layers), 0, 0, 0};
        gpu.update(constants_, &frame, sizeof frame);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, constants_);
        glUseProgram(program_);
        glBindTextureUnit(0, matcaps_);
        glBindSampler(0, sampler_);
        mesh_.draw();
    }
    void release() override
    {
        glDeleteProgram(program_);
        mesh_.release();
        glDeleteTextures(1, &matcaps_);
        glDeleteSamplers(1, &sampler_);
        glDeleteBuffers(1, &constants_);
    }

private:
    static constexpr uint32_t layers = 4;
    GLuint   program_ = 0;
    DrawMesh mesh_;
    GLuint   matcaps_ = 0;
    GLuint   sampler_ = 0;
    GLuint   constants_ = 0;
};

// ================================================================ skeletalanimation

// A goblin skinned to 16 bones: each vertex blends up to two bone matrices in
// the vertex shader; the pose is a function of time.
const char *const skeletalanimation_common = R"(
layout(std140, binding = 0, row_major) uniform Frame {
    mat4 view_projection;
    vec4 light_position;
    vec4 eye;
    mat4 bones[16];
};
)";

#define SKELETAL_OUT "out vec3 v_world;\nout vec3 v_normal;\nout vec2 v_uv;\nout vec3 v_color;\n"

const char *const skeletalanimation_skin_vertex = MESH_INPUTS SKELETAL_OUT R"(
layout(location = 4) in uvec4 bone;
layout(location = 5) in vec4 weight;
void main() {
    mat4 m = bones[bone.x] * weight.x + bones[bone.y] * weight.y +
             bones[bone.z] * weight.z + bones[bone.w] * weight.w;
    vec4 world = vec4(position, 1) * m;
    gl_Position = world * view_projection;
    v_world = world.xyz;
    v_normal = (vec4(normal, 0) * m).xyz;
    v_uv = uv;
    v_color = color;
}
)";

const char *const skeletalanimation_floor_vertex = MESH_INPUTS SKELETAL_OUT R"(
void main() {
    gl_Position = vec4(position, 1) * view_projection;
    v_world = position;
    v_normal = normal;
    v_uv = uv;
    v_color = color;
}
)";

#define SKELETAL_LIGHT R"(
layout(binding = 0) uniform sampler2D skin_map;
layout(binding = 1) uniform sampler2D floor_color;
layout(binding = 2) uniform sampler2D floor_normal;
in vec3 v_world;
in vec3 v_normal;
in vec2 v_uv;
in vec3 v_color;
layout(location = 0) out vec4 target;
// A spot light from above and in front of the goblin.
vec3 light(vec3 world, vec3 n, vec3 albedo, float shininess, float strength) {
    vec3 to_light = light_position.xyz - world;
    float d = length(to_light);
    vec3 l = to_light / d;
    vec3 v = normalize(eye.xyz - world);
    float cone = smoothstep(0.78, 0.95, dot(-l, normalize(vec3(0, 1.2, 0) - light_position.xyz)));
    float diffuse = clamp(dot(n, l), 0.0, 1.0);
    float specular = pow(clamp(dot(reflect(-l, n), v), 0.0, 1.0), shininess) * strength;
    return (albedo * (diffuse * 1.5) + specular) * cone + albedo * 0.015;
}
)"

const char *const skeletalanimation_skin_fragment = SKELETAL_LIGHT R"(
void main() {
    vec3 albedo = texture(skin_map, v_uv * vec2(3, 6)).rgb * v_color;
    target = vec4(light(v_world, normalize(v_normal), albedo, 24, 0.6), 1);
}
)";

const char *const skeletalanimation_floor_fragment = SKELETAL_LIGHT R"(
void main() {
    vec3 t = normalize(texture(floor_normal, v_uv).rgb * 2 - 1);
    vec3 n = vec3(t.x, t.z, t.y);
    vec3 albedo = texture(floor_color, v_uv).rgb;
    target = vec4(light(v_world, n, albedo, 16, 0.15), 1);
}
)";

struct SkinVertex {
    float3  position;
    float3  normal;
    float2  uv;
    float3  color;
    uint8_t bone[4];
    float   weight[4];
};

struct SkinFrame {
    float4x4 view_projection;
    float4   light_position;
    float4   eye;
    float4x4 bones[16];
};

constexpr int bone_count = 16;

class SkeletalAnimation final : public Sample {
public:
    const char *name() const override { return "skeletalanimation"; }
    bool init(Gpu &gpu) override
    {
        skin_program_ = gpu.program("skeletalanimation_skin", {.common = skeletalanimation_common,
                                                               .vertex = skeletalanimation_skin_vertex,
                                                               .fragment = skeletalanimation_skin_fragment});
        floor_program_ = gpu.program("skeletalanimation_floor", {.common = skeletalanimation_common,
                                                                 .vertex = skeletalanimation_floor_vertex,
                                                                 .fragment = skeletalanimation_floor_fragment});
        build_goblin(gpu);
        Mesh plane = plane_mesh(16.0f, 1, 5.0f);
        floor_ = gpu.upload(plane);
        plane.release();
        StoneField stones;
        stones.make(512, 5, 41u);
        uint32_t *normals = stones.normal_height(16.0f);
        uint32_t *colors = stones.colors({0.36f, 0.36f, 0.38f}, 42u);
        floor_normal_ = gpu.texture(normals, 512, 512);
        floor_color_ = gpu.texture(colors, 512, 512);
        release_memory(normals);
        release_memory(colors);
        stones.release();
        const uint32_t size = 256;
        uint32_t *texels = static_cast<uint32_t *>(allocate(size * size * 4));
        for (uint32_t y = 0; y < size; y++)
            for (uint32_t x = 0; x < size; x++) {
                const float fx = static_cast<float>(x) / static_cast<float>(size), fy = static_cast<float>(y) / static_cast<float>(size);
                const float mottle = fbm(fx * 8.0f, fy * 8.0f, 0.0f, 51u, 8, 4);
                const float scales = fbm(fx * 48.0f, fy * 48.0f, 2.0f, 52u, 48, 2);
                const float v = 0.55f + 0.5f * mottle - 0.25f * smoothstep(0.45f, 0.7f, scales);
                texels[y * size + x] = rgba(v, v, v);
            }
        skin_ = gpu.texture(texels, size, size);
        release_memory(texels);
        sampler_ = gpu.sampler();
        constants_ = gpu.uniform_buffer(sizeof(SkinFrame));
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        gpu.begin_frame(0.0f, 0.0f, 0.0f);
        SkinFrame frame;
        const float3 eye = {0.0f, 3.0f, -5.0f};
        frame.view_projection = look_at(eye, {0, 1.45f, 0}, {0, 1, 0}) * perspective(radians(55), gpu.aspect(), 0.1f, 100);
        frame.light_position = {0.0f, 7.0f, -3.0f, 1};
        frame.eye = {eye.x, eye.y, eye.z, 1};
        pose(time, frame.bones);
        gpu.update(constants_, &frame, sizeof frame);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, constants_);
        const GLuint views[3] = {skin_, floor_color_, floor_normal_};
        for (GLuint i = 0; i < 3; i++) {
            glBindTextureUnit(i, views[i]);
            glBindSampler(i, sampler_);
        }
        glUseProgram(floor_program_);
        floor_.draw();
        glUseProgram(skin_program_);
        glBindVertexArray(goblin_layout_);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(goblin_index_count_), GL_UNSIGNED_INT, nullptr);
    }
    void release() override
    {
        glDeleteProgram(skin_program_);
        glDeleteProgram(floor_program_);
        glDeleteVertexArrays(1, &goblin_layout_);
        glDeleteBuffers(1, &goblin_vertices_);
        glDeleteBuffers(1, &goblin_indices_);
        floor_.release();
        glDeleteTextures(1, &floor_normal_);
        glDeleteTextures(1, &floor_color_);
        glDeleteTextures(1, &skin_);
        glDeleteSamplers(1, &sampler_);
        glDeleteBuffers(1, &constants_);
    }

private:
private:
    // Rest-pose joints (the pivot each bone turns about) and each bone's parent.
    static constexpr float joints[bone_count][3] = {
        {0, 1.55f, 0},     {0, 2.1f, 0},       {0, 2.65f, 0},     {0, 3.05f, 0},
        {0.5f, 2.85f, 0},  {0.72f, 2.1f, 0},   {0.82f, 1.4f, 0},
        {-0.5f, 2.85f, 0}, {-0.72f, 2.1f, 0},  {-0.82f, 1.4f, 0},
        {0.28f, 1.55f, 0}, {0.34f, 0.85f, 0},  {0.34f, 0.2f, 0},
        {-0.28f, 1.55f, 0}, {-0.34f, 0.85f, 0}, {-0.34f, 0.2f, 0}};
    static constexpr int parents[bone_count] = {-1, 0, 1, 2, 2, 4, 5, 2, 7, 8, 0, 10, 11, 0, 13, 14};

    float3 joint(int i) const { return {joints[i][0], joints[i][1], joints[i][2]}; }

    // Appends a capsule on `bone`, blended half and half with `parent` at its
    // start and with `child` at its end so the joints bend smoothly.
    void add_part(SkinVertex *vertices, uint32_t *indices, uint32_t &vertex_count, uint32_t &index_count, int bone,
                  int parent, int child, float3 a, float3 b, float r0, float r1, float3 color, float boot_below)
    {
        Mesh part = capsule_mesh(a, b, r0, r1, 8, 18, color);
        const float3 axis = b - a;
        const float length_squared = dot(axis, axis);
        const uint32_t base = vertex_count;
        for (uint32_t i = 0; i < part.vertex_count; i++) {
            const Vertex &v = part.vertices[i];
            SkinVertex &s = vertices[vertex_count++];
            s.position = v.position;
            s.normal = v.normal;
            s.uv = v.uv;
            s.color = v.position.y < boot_below ? float3{0.4f, 0.22f, 0.1f} : v.color;
            const float t = clamp(dot(v.position - a, axis) / length_squared, 0.0f, 1.0f);
            s.bone[0] = static_cast<uint8_t>(bone);
            s.weight[0] = 1.0f;
            if (parent >= 0 && t < 0.3f) {
                s.bone[1] = static_cast<uint8_t>(parent);
                s.weight[1] = 0.5f - 0.5f * t / 0.3f;
            } else if (child >= 0 && t > 0.7f) {
                s.bone[1] = static_cast<uint8_t>(child);
                s.weight[1] = 0.5f * (t - 0.7f) / 0.3f;
            }
            s.weight[0] = 1.0f - s.weight[1];
        }
        for (uint32_t i = 0; i < part.index_count; i++) indices[index_count++] = base + part.indices[i];
        part.release();
    }

    void build_goblin(Gpu &gpu)
    {
        const uint32_t max_vertices = 16000, max_indices = 80000;
        SkinVertex *vertices = static_cast<SkinVertex *>(allocate(sizeof(SkinVertex) * max_vertices));
        uint32_t *indices = static_cast<uint32_t *>(allocate(sizeof(uint32_t) * max_indices));
        uint32_t vc = 0, ic = 0;
        const float3 green = {0.3f, 0.55f, 0.28f};
        const float none = -1.0f;
        add_part(vertices, indices, vc, ic, 0, -1, 1, {0, 1.5f, 0}, joint(1), 0.4f, 0.38f, green, none);
        add_part(vertices, indices, vc, ic, 1, 0, 2, joint(1), joint(2), 0.38f, 0.46f, green, none);
        add_part(vertices, indices, vc, ic, 2, 1, 3, joint(2), {0, 2.95f, 0}, 0.46f, 0.3f, green, none);
        add_part(vertices, indices, vc, ic, 3, 2, -1, {0, 3.05f, 0}, {0, 3.3f, -0.02f}, 0.15f, 0.2f, green, none);
        add_part(vertices, indices, vc, ic, 3, -1, -1, {0, 3.45f, -0.02f}, {0, 3.72f, 0.02f}, 0.28f, 0.26f, green, none);
        add_part(vertices, indices, vc, ic, 3, -1, -1, {0, 3.52f, -0.24f}, {0, 3.38f, -0.4f}, 0.07f, 0.06f, green, none);
        for (int side = -1; side <= 1; side += 2) {
            const float s = static_cast<float>(side);
            const int arm = side > 0 ? 4 : 7, leg = side > 0 ? 10 : 13;
            add_part(vertices, indices, vc, ic, 3, -1, -1, {0.24f * s, 3.55f, 0}, {0.46f * s, 3.72f, 0.06f}, 0.07f, 0.03f, green, none);
            add_part(vertices, indices, vc, ic, arm, 2, arm + 1, joint(arm), joint(arm + 1), 0.18f, 0.14f, green, none);
            add_part(vertices, indices, vc, ic, arm + 1, arm, arm + 2, joint(arm + 1), joint(arm + 2), 0.14f, 0.1f, green, none);
            add_part(vertices, indices, vc, ic, arm + 2, arm + 1, -1, joint(arm + 2), {0.88f * s, 1.0f, -0.05f}, 0.13f, 0.09f, green, none);
            add_part(vertices, indices, vc, ic, leg, 0, leg + 1, joint(leg), joint(leg + 1), 0.21f, 0.15f, green, none);
            add_part(vertices, indices, vc, ic, leg + 1, leg, leg + 2, joint(leg + 1), joint(leg + 2), 0.15f, 0.12f, green, 0.55f);
            add_part(vertices, indices, vc, ic, leg + 2, leg + 1, -1, joint(leg + 2), {0.34f * s, 0.12f, -0.35f}, 0.13f, 0.12f, green, 0.55f);
        }
        goblin_vertices_ = gpu.buffer(vertices, vc * static_cast<uint32_t>(sizeof(SkinVertex)));
        goblin_indices_ = gpu.buffer(indices, ic * 4u);
        // The mesh layout at the skinned vertex's stride, and the bones beside it.
        goblin_layout_ = gpu.mesh_layout(goblin_vertices_, goblin_indices_);
        glVertexArrayVertexBuffer(goblin_layout_, 0, goblin_vertices_, 0, sizeof(SkinVertex));
        glEnableVertexArrayAttrib(goblin_layout_, 4);
        glVertexArrayAttribIFormat(goblin_layout_, 4, 4, GL_UNSIGNED_BYTE, 44);
        glVertexArrayAttribBinding(goblin_layout_, 4, 0);
        glEnableVertexArrayAttrib(goblin_layout_, 5);
        glVertexArrayAttribFormat(goblin_layout_, 5, 4, GL_FLOAT, GL_FALSE, 48);
        glVertexArrayAttribBinding(goblin_layout_, 5, 0);
        goblin_index_count_ = ic;
        release_memory(vertices);
        release_memory(indices);
    }

    // Each bone turns about its joint in the rest pose's frame, then follows its parent.
    void pose(float time, float4x4 *bones) const
    {
        const float w = 2.0f * time;
        float4x4 local[bone_count];
        for (int i = 0; i < bone_count; i++) local[i] = float4x4::identity();
        local[0] = rotate_y(0.45f + 0.3f * sin(0.7f * time));
        local[1] = rotate_x(-0.35f - 0.04f * sin(w));
        local[2] = rotate_x(-0.15f) * rotate_z(0.06f * sin(0.7f * time));
        local[3] = rotate_x(0.45f) * rotate_y(0.5f * sin(0.9f * time));
        for (int side = -1; side <= 1; side += 2) {
            const float s = static_cast<float>(side);
            const int arm = side > 0 ? 4 : 7, leg = side > 0 ? 10 : 13;
            const float swing = sin(1.4f * time + (side > 0 ? 0.0f : pi));
            local[arm] = rotate_z(0.12f * s) * rotate_x(0.35f + 0.3f * swing);
            local[arm + 1] = rotate_x(0.5f + 0.2f * swing);
            local[arm + 2] = rotate_x(0.2f);
            local[leg] = rotate_z(-0.12f * s) * rotate_x(0.9f + 0.08f * sin(w));
            local[leg + 1] = rotate_x(-1.5f - 0.16f * sin(w));
            local[leg + 2] = rotate_x(0.6f + 0.08f * sin(w));
        }
        for (int i = 0; i < bone_count; i++) {
            const float3 p = joint(i);
            float4x4 m = translate(-p) * local[i] * translate(p);
            if (parents[i] >= 0) m = m * bones[parents[i]];
            else m = m * translate({0, -0.42f + 0.05f * sin(w), 0});
            bones[i] = m;
        }
    }

    GLuint   skin_program_ = 0;
    GLuint   floor_program_ = 0;
    GLuint   goblin_layout_ = 0;
    GLuint   goblin_vertices_ = 0;
    GLuint   goblin_indices_ = 0;
    uint32_t goblin_index_count_ = 0;
    DrawMesh floor_;
    GLuint   floor_normal_ = 0;
    GLuint   floor_color_ = 0;
    GLuint   skin_ = 0;
    GLuint   sampler_ = 0;
    GLuint   constants_ = 0;
};

// ================================================================ vulkanscene

// The demo scene: a skybox cube map of a red night sky, glossy red models
// with the original's hard-saturating "mesh" shading in front of a pale disc,
// and an emblem above and a plate below drawn with the plainer "logo" shading.
const char *const vulkanscene_common = R"(
layout(std140, binding = 0, row_major) uniform Frame {
    mat4 view_projection;
    mat4 sky_view_projection;
    vec4 eye;
    vec4 light;
};
)";

const char *const vulkanscene_sky_vertex = R"(
layout(location = 0) in vec3 position;
out vec3 v_direction;
void main() {
    gl_Position = (vec4(position, 1) * sky_view_projection).xyww;
    v_direction = position;
}
)";

const char *const vulkanscene_sky_fragment = R"(
layout(binding = 0) uniform samplerCube sky;
in vec3 v_direction;
layout(location = 0) out vec4 target;
void main() { target = texture(sky, v_direction); }
)";

const char *const vulkanscene_vertex = MESH_INPUTS R"(
out vec3 v_world;
out vec3 v_normal;
out vec3 v_color;
void main() {
    gl_Position = vec4(position, 1) * view_projection;
    v_world = position;
    v_normal = normal;
    v_color = color;
}
)";

const char *const vulkanscene_mesh_fragment = R"(
in vec3 v_world;
in vec3 v_normal;
in vec3 v_color;
layout(location = 0) out vec4 target;
void main() {
    vec3 n = normalize(v_normal);
    vec3 l = normalize(light.xyz - v_world);
    vec3 e = normalize(eye.xyz - v_world);
    vec3 h = normalize(l + e);
    float diffuse = clamp(dot(l, n), 0.0, 1.0);
    float specular = diffuse > 0 ? pow(clamp(dot(h, n), 0.0, 1.0), 64) : 0;
    float intensity = 0.1 + diffuse + specular;
    vec3 r = reflect(-l, n);
    vec3 c = (0.2 + 0.5 * diffuse) * v_color + 0.5 * pow(clamp(dot(r, e), 0.0, 1.0), 2) * 0.75;
    if (intensity > 0.95) c *= 2.25;
    if (intensity < 0.15) c = vec3(0.1);
    target = vec4(c, 1);
}
)";

const char *const vulkanscene_logo_fragment = R"(
in vec3 v_world;
in vec3 v_normal;
in vec3 v_color;
layout(location = 0) out vec4 target;
void main() {
    vec3 n = normalize(v_normal);
    vec3 l = normalize(light.xyz - v_world);
    vec3 e = normalize(eye.xyz - v_world);
    vec3 r = reflect(-l, n);
    vec3 c = v_color * (0.25 + 0.85 * max(dot(n, l), 0)) + pow(clamp(dot(r, e), 0.0, 1.0), 24) * 0.4;
    target = vec4(c, 1);
}
)";

struct SceneFrame {
    float4x4 view_projection;
    float4x4 sky_view_projection;
    float4   eye;
    float4   light;
};

// The direction a cube-map face's texel looks along (Direct3D's face order and orientation, which OpenGL's cube maps share).
float3 cube_direction(uint32_t face, float s, float t)
{
    switch (face) {
    case 0: return {1, -t, -s};
    case 1: return {-1, -t, s};
    case 2: return {s, 1, t};
    case 3: return {s, -1, -t};
    case 4: return {s, -t, 1};
    default: return {-s, -t, -1};
    }
}

float3 night_sky(float3 d)
{
    d = normalize(d);
    const float up = d.y;
    float3 c = mix3({0.12f, 0.01f, 0.01f}, {0.03f, 0.0f, 0.01f}, saturate(up * 1.5f));
    if (up < 0.0f) c = mix3({0.12f, 0.01f, 0.01f}, {0.05f, 0.0f, 0.0f}, saturate(-up * 3.0f));
    const float band = 1.0f / (1.0f + 900.0f * up * up);
    c = c + float3{0.7f, 0.05f, 0.04f} * band;
    const float clouds = fbm(d.x * 3.0f + 10.0f, d.y * 6.0f + 10.0f, d.z * 3.0f + 10.0f, 77u, 0, 5);
    const float cover = smoothstep(0.45f, 0.75f, clouds) * smoothstep(-0.05f, 0.1f, up) * (1.0f - smoothstep(0.5f, 0.9f, up));
    c = mix3(c, {0.35f, 0.22f, 0.24f}, cover * 0.7f);
    if (up > -0.02f) {
        const float grid = 90.0f;
        const float gx = d.x * grid, gy = d.y * grid, gz = d.z * grid;
        const int cx = static_cast<int>(floor(gx)), cy = static_cast<int>(floor(gy)), cz = static_cast<int>(floor(gz));
        if (lattice(cx, cy, cz, 91u) > 0.93f) {
            const float3 center = {static_cast<float>(cx) + 0.5f, static_cast<float>(cy) + 0.5f, static_cast<float>(cz) + 0.5f};
            const float3 offset = float3{gx, gy, gz} - center;
            const float star = saturate(1.0f - length(offset) * 3.5f) * (1.0f - cover);
            c = c + float3{1.0f, 0.95f, 0.9f} * (star * (0.5f + 0.5f * lattice(cx, cy, cz, 92u)));
        }
    }
    return c;
}

class VulkanScene final : public Sample {
public:
    const char *name() const override { return "vulkanscene"; }
    bool init(Gpu &gpu) override
    {
        sky_program_ = gpu.program("vulkanscene_sky", {.common = vulkanscene_common, .vertex = vulkanscene_sky_vertex,
                                                       .fragment = vulkanscene_sky_fragment});
        mesh_program_ = gpu.program("vulkanscene_mesh", {.common = vulkanscene_common, .vertex = vulkanscene_vertex,
                                                         .fragment = vulkanscene_mesh_fragment});
        logo_program_ = gpu.program("vulkanscene_logo", {.common = vulkanscene_common, .vertex = vulkanscene_vertex,
                                                         .fragment = vulkanscene_logo_fragment});
        Mesh box = cube_mesh(2.0f);
        sky_box_ = gpu.upload(box);
        box.release();
        const float3 red = {0.75f, 0.04f, 0.04f}, pale = {0.9f, 0.9f, 0.9f};
        MeshBuilder models(60000, 300000);
        models.take(sphere_mesh(2.6f, 64, 32), scale({1, 1, 0.08f}) * translate({0, 0.6f, 1.6f}), pale);
        models.take(knot_mesh(1.3f, 0.32f, 2, 5, 320, 20), rotate_y(0.3f) * translate({0, 0.5f, 0}), red);
        models.take(sphere_mesh(0.75f, 40, 20), scale({1.0f, 1.25f, 0.9f}) * translate({-2.1f, -0.7f, -0.3f}), red);
        models.take(capsule_mesh({-2.3f, 0.2f, -0.3f}, {-2.5f, 1.2f, -0.2f}, 0.12f, 0.05f, 4, 12, white), float4x4::identity(), red);
        models.take(capsule_mesh({-1.9f, 0.2f, -0.3f}, {-1.8f, 1.1f, -0.1f}, 0.12f, 0.05f, 4, 12, white), float4x4::identity(), red);
        models.take(sphere_mesh(0.7f, 40, 20), scale({1.2f, 0.8f, 1.0f}) * translate({0.0f, -1.3f, -0.9f}), red);
        models.take(torus_mesh(0.6f, 0.22f, 48, 16), rotate_x(1.1f) * rotate_z(0.4f) * translate({2.1f, -0.6f, -0.3f}), red);
        models.take(capsule_mesh({1.9f, -0.2f, -0.2f}, {2.5f, 0.9f, -0.1f}, 0.2f, 0.12f, 6, 14, white), float4x4::identity(), red);
        models_ = gpu.upload(models.mesh());
        models.mesh().release();
        MeshBuilder logos(20000, 100000);
        logos.take(torus_mesh(1.5f, 0.2f, 96, 16), rotate_x(0.5f * pi) * scale({1.75f, 0.8f, 1.0f}) * translate({0, 3.0f, 0.3f}), red);
        Mesh cube = cube_mesh(1.0f);
        logos.add(cube, scale({4.2f, 0.6f, 0.35f}) * translate({0, 2.75f, 0.1f}), red);
        logos.add(cube, scale({3.8f, 0.36f, 0.2f}) * translate({0, 2.75f, -0.05f}), pale);
        logos.add(cube, scale({6.8f, 0.95f, 0.4f}) * translate({0, -2.45f, -0.8f}), red);
        Random random(5u);
        float x = -3.1f;
        while (x < 3.0f) {
            const float wide = random.range(0.18f, 0.45f);
            logos.add(cube, scale({wide, 0.62f, 0.2f}) * translate({x + wide * 0.5f, -2.45f, -1.02f}), pale);
            x += wide + random.range(0.06f, 0.14f);
        }
        cube.release();
        logos_ = gpu.upload(logos.mesh());
        logos.mesh().release();
        const uint32_t size = 256;
        uint32_t *texels = static_cast<uint32_t *>(allocate(size * size * 6 * 4));
        for (uint32_t face = 0; face < 6; face++)
            for (uint32_t y = 0; y < size; y++)
                for (uint32_t xi = 0; xi < size; xi++) {
                    const float s = (static_cast<float>(xi) + 0.5f) / static_cast<float>(size) * 2.0f - 1.0f;
                    const float t = (static_cast<float>(y) + 0.5f) / static_cast<float>(size) * 2.0f - 1.0f;
                    const float3 c = night_sky(cube_direction(face, s, t));
                    texels[(face * size + y) * size + xi] = rgba(c.x, c.y, c.z);
                }
        sky_ = gpu.texture(texels, size, size, 6, true);
        release_memory(texels);
        sampler_ = gpu.sampler(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, 1.0f);
        constants_ = gpu.uniform_buffer(sizeof(SceneFrame));
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        gpu.begin_frame(0.0f, 0.0f, 0.0f);
        const float angle = 0.35f * sin(time * 0.4f);
        const float3 eye = {sin(angle) * 10.0f, 0.6f, -cos(angle) * 10.0f};
        const float3 target = {0, 0.3f, 0};
        const float4x4 view = look_at(eye, target, {0, 1, 0});
        const float4x4 projection = perspective(radians(60), gpu.aspect(), 0.1f, 100);
        float4x4 rotation_only = view;
        rotation_only.m[3][0] = rotation_only.m[3][1] = rotation_only.m[3][2] = 0.0f;
        SceneFrame frame;
        frame.view_projection = view * projection;
        frame.sky_view_projection = rotation_only * projection;
        frame.eye = {eye.x, eye.y, eye.z, 1};
        frame.light = {3.0f, 5.0f, -8.0f, 1};
        gpu.update(constants_, &frame, sizeof frame);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, constants_);
        depth_state(false, false);
        cull(false);
        glUseProgram(sky_program_);
        glBindTextureUnit(0, sky_);
        glBindSampler(0, sampler_);
        sky_box_.draw();
        depth_state(true, true);
        glDepthFunc(GL_LESS);
        cull(true);
        glUseProgram(mesh_program_);
        models_.draw();
        glUseProgram(logo_program_);
        logos_.draw();
    }
    void release() override
    {
        glDeleteProgram(sky_program_);
        glDeleteProgram(mesh_program_);
        glDeleteProgram(logo_program_);
        sky_box_.release();
        models_.release();
        logos_.release();
        glDeleteTextures(1, &sky_);
        glDeleteSamplers(1, &sampler_);
        glDeleteBuffers(1, &constants_);
    }

private:
    GLuint   sky_program_ = 0;
    GLuint   mesh_program_ = 0;
    GLuint   logo_program_ = 0;
    DrawMesh sky_box_;
    DrawMesh models_;
    DrawMesh logos_;
    GLuint   sky_ = 0;
    GLuint   sampler_ = 0;
    GLuint   constants_ = 0;
};

}  // namespace

void register_lighting(Sample **all, uint32_t &made)
{
    all[made++] = new ShadowMapping();
    all[made++] = new ShadowMappingOmni();
    all[made++] = new Deferred();
    all[made++] = new ParallaxMapping();
    all[made++] = new SphericalEnvMapping();
    all[made++] = new SkeletalAnimation();
    all[made++] = new VulkanScene();
}

}  // namespace samples
