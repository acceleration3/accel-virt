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

const char *const geometry_frame = R"(
layout(std140, binding = 0, row_major) uniform Frame {
    mat4 world; mat4 view_projection;
    vec4 light; vec4 eye; vec4 params;
};
)";

#define MESH_INPUTS                                  \
    "layout(location = 0) in vec3 position;\n"      \
    "layout(location = 1) in vec3 normal;\n"        \
    "layout(location = 2) in vec2 uv;\n"            \
    "layout(location = 3) in vec3 color;\n"

void raster(bool wireframe, bool cull, int depth_bias = 0, float slope_bias = 0.0f)
{
    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
    if (cull) glEnable(GL_CULL_FACE);
    else glDisable(GL_CULL_FACE);
    const GLenum offset = wireframe ? GL_POLYGON_OFFSET_LINE : GL_POLYGON_OFFSET_FILL;
    glDisable(GL_POLYGON_OFFSET_LINE);
    glDisable(GL_POLYGON_OFFSET_FILL);
    if (depth_bias != 0 || slope_bias != 0.0f) {
        glEnable(offset);
        glPolygonOffset(slope_bias, static_cast<float>(depth_bias));
    }
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
        static const char *const vertex = MESH_INPUTS R"(
out vec3 v_normal;
out vec3 v_color;
out vec3 v_world_position;
void main() {
    vec4 w = vec4(position, 1) * world;
    gl_Position = w * view_projection;
    v_normal = (vec4(normal, 0) * world).xyz;
    v_color = color;
    v_world_position = w.xyz;
}
)";
        lit_ = gpu.program("geometryshader", {.common = geometry_frame, .vertex = vertex, .fragment = R"(
in vec3 v_normal;
in vec3 v_color;
in vec3 v_world_position;
layout(location = 0) out vec4 target;
void main() {
    vec3 n = normalize(v_normal);
    vec3 l = normalize(light.xyz - v_world_position);
    vec3 v = normalize(eye.xyz - v_world_position);
    vec3 r = reflect(-l, n);
    float diffuse = max(dot(n, l), 0.0);
    float specular = pow(max(dot(r, v), 0.0), 16.0) * 0.35;
    target = vec4(v_color * (0.2 + 0.6 * diffuse) + specular, 1);
}
)"});
        static const char *const normals_vertex = MESH_INPUTS R"(
out vec3 v_position;
out vec3 v_normal;
void main() { v_position = position; v_normal = normal; }
)";
        normals_ = gpu.program("geometryshader_normals", {.common = geometry_frame, .vertex = normals_vertex,
                                                           .fragment = R"(
in vec3 g_color;
layout(location = 0) out vec4 target;
void main() { target = vec4(g_color, 1); }
)",
                                                           .geometry = R"(
layout(triangles) in;
layout(line_strip, max_vertices = 6) out;
in vec3 v_position[];
in vec3 v_normal[];
out vec3 g_color;
void main() {
    for (int i = 0; i < 3; i++) {
        gl_Position = (vec4(v_position[i], 1) * world) * view_projection;
        g_color = vec3(1, 0, 0);
        EmitVertex();
        gl_Position = (vec4(v_position[i] + v_normal[i] * params.x, 1) * world) * view_projection;
        g_color = vec3(0, 0, 1);
        EmitVertex();
        EndPrimitive();
    }
}
)"});
        Mesh mesh = head_mesh();
        head_ = gpu.upload(mesh);
        mesh.release();
        constants_ = gpu.uniform_buffer(sizeof(Frame));
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
        gpu.update(constants_, &frame, sizeof frame);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, constants_);
        glUseProgram(lit_);
        head_.draw();
        glUseProgram(normals_);
        head_.draw();
    }
    void release() override
    {
        glDeleteProgram(lit_);
        glDeleteProgram(normals_);
        head_.release();
        glDeleteBuffers(1, &constants_);
    }

private:
    GLuint   lit_ = 0;
    GLuint   normals_ = 0;
    DrawMesh head_;
    GLuint   constants_ = 0;
};

const char *const tessellation_vertex = R"(
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 3) in vec3 color;
out vec3 v_position;
out vec3 v_normal;
out vec3 v_color;
void main() { v_position = position; v_normal = normalize(normal); v_color = color; }
)";

const char *const tessellation_flat_control = R"(
layout(vertices = 3) out;
in vec3 v_position[];
in vec3 v_normal[];
in vec3 v_color[];
out vec3 c_position[];
out vec3 c_normal[];
out vec3 c_color[];
void main() {
    c_position[gl_InvocationID] = v_position[gl_InvocationID];
    c_normal[gl_InvocationID] = v_normal[gl_InvocationID];
    c_color[gl_InvocationID] = v_color[gl_InvocationID];
    if (gl_InvocationID == 0) {
        gl_TessLevelOuter[0] = gl_TessLevelOuter[1] = gl_TessLevelOuter[2] = params.x;
        gl_TessLevelInner[0] = params.x;
    }
}
)";

const char *const tessellation_pn_control = R"(
layout(vertices = 3) out;
in vec3 v_position[];
in vec3 v_normal[];
in vec3 v_color[];
out vec3 c_position[];
out vec3 c_normal[];
out vec3 c_color[];
patch out vec3 b210, b120, b021, b012, b102, b201, b111;
patch out vec3 n110, n011, n101;
vec3 edge_point(vec3 a, vec3 b, vec3 na) { return (2 * a + b - dot(b - a, na) * na) / 3; }
vec3 edge_normal(vec3 a, vec3 b, vec3 na, vec3 nb) {
    vec3 d = b - a;
    return normalize(na + nb - 2 * dot(d, na + nb) / dot(d, d) * d);
}
void main() {
    c_position[gl_InvocationID] = v_position[gl_InvocationID];
    c_normal[gl_InvocationID] = v_normal[gl_InvocationID];
    c_color[gl_InvocationID] = v_color[gl_InvocationID];
    if (gl_InvocationID == 0) {
        gl_TessLevelOuter[0] = gl_TessLevelOuter[1] = gl_TessLevelOuter[2] = params.x;
        gl_TessLevelInner[0] = params.x;
        vec3 p0 = v_position[0], p1 = v_position[1], p2 = v_position[2];
        vec3 n0 = v_normal[0], n1 = v_normal[1], n2 = v_normal[2];
        b210 = edge_point(p0, p1, n0); b120 = edge_point(p1, p0, n1);
        b021 = edge_point(p1, p2, n1); b012 = edge_point(p2, p1, n2);
        b102 = edge_point(p2, p0, n2); b201 = edge_point(p0, p2, n0);
        vec3 e = (b210 + b120 + b021 + b012 + b102 + b201) / 6;
        vec3 v = (p0 + p1 + p2) / 3;
        b111 = e + (e - v) * 0.5;
        n110 = edge_normal(p0, p1, n0, n1);
        n011 = edge_normal(p1, p2, n1, n2);
        n101 = edge_normal(p2, p0, n2, n0);
    }
}
)";

#define TESSELLATION_SHADE R"(
in vec3 c_position[];
in vec3 c_normal[];
in vec3 c_color[];
out vec3 e_normal;
out vec3 e_color;
out vec3 e_world_position;
void shade(vec3 position, vec3 normal, vec3 color) {
    vec4 w = vec4(position, 1) * world;
    gl_Position = w * view_projection;
    e_normal = (vec4(normal, 0) * world).xyz;
    e_color = color;
    e_world_position = w.xyz;
}
)"

const char *const tessellation_flat_evaluation = TESSELLATION_SHADE R"(
// Direct3D's triangle_cw: its domain's orientation is the mirror of OpenGL's.
layout(triangles, fractional_odd_spacing, ccw) in;
void main() {
    vec3 uvw = gl_TessCoord;
    shade(uvw.x * c_position[0] + uvw.y * c_position[1] + uvw.z * c_position[2],
          uvw.x * c_normal[0] + uvw.y * c_normal[1] + uvw.z * c_normal[2],
          uvw.x * c_color[0] + uvw.y * c_color[1] + uvw.z * c_color[2]);
}
)";

const char *const tessellation_pn_evaluation = TESSELLATION_SHADE R"(
// Direct3D's triangle_cw: its domain's orientation is the mirror of OpenGL's.
layout(triangles, fractional_odd_spacing, ccw) in;
patch in vec3 b210, b120, b021, b012, b102, b201, b111;
patch in vec3 n110, n011, n101;
void main() {
    float u = gl_TessCoord.x, v = gl_TessCoord.y, w = gl_TessCoord.z;
    vec3 flat_position = u * c_position[0] + v * c_position[1] + w * c_position[2];
    vec3 flat_normal = u * c_normal[0] + v * c_normal[1] + w * c_normal[2];
    vec3 pn_position = c_position[0] * u * u * u + c_position[1] * v * v * v + c_position[2] * w * w * w
        + b210 * 3 * u * u * v + b120 * 3 * u * v * v + b201 * 3 * u * u * w
        + b102 * 3 * u * w * w + b021 * 3 * v * v * w + b012 * 3 * v * w * w
        + b111 * 6 * u * v * w;
    vec3 pn_normal = c_normal[0] * u * u + c_normal[1] * v * v + c_normal[2] * w * w
        + n110 * u * v + n011 * v * w + n101 * u * w;
    float alpha = params.y;
    shade(mix(flat_position, pn_position, alpha), mix(flat_normal, pn_normal, alpha),
          u * c_color[0] + v * c_color[1] + w * c_color[2]);
}
)";

const char *const tessellation_fragment = R"(
in vec3 e_normal;
in vec3 e_color;
in vec3 e_world_position;
layout(location = 0) out vec4 target;
void main() {
    vec3 n = normalize(e_normal);
    vec3 l = normalize(light.xyz - e_world_position);
    vec3 v = normalize(eye.xyz - e_world_position);
    float diffuse = max(dot(n, l), 0.0);
    float specular = pow(max(dot(reflect(-l, n), v), 0.0), 24.0) * 0.4;
    target = vec4(e_color * (0.15 + 0.85 * diffuse) + specular, 1);
}
)";

const char *const tessellation_wire_fragment = R"(
layout(location = 0) out vec4 target;
void main() { target = vec4(0.05, 0.05, 0.08, 1); }
)";

class Tessellation final : public Sample {
public:
    const char *name() const override { return "tessellation"; }
    bool init(Gpu &gpu) override
    {
        const char *const controls[2] = {tessellation_flat_control, tessellation_pn_control};
        const char *const evaluations[2] = {tessellation_flat_evaluation, tessellation_pn_evaluation};
        const char *const fragments[2] = {tessellation_fragment, tessellation_wire_fragment};
        for (int side = 0; side < 2; side++)
            for (int wire = 0; wire < 2; wire++)
                programs_[side][wire] = gpu.program("tessellation", {.common = geometry_frame,
                                                                     .vertex = tessellation_vertex,
                                                                     .fragment = fragments[wire],
                                                                     .control = controls[side],
                                                                     .evaluation = evaluations[side]});
        Mesh mesh = torus_mesh(1.0f, 0.5f, 10, 6);
        for (uint32_t i = 0; i < mesh.vertex_count; i++) {
            const float up = mesh.vertices[i].normal.y;
            mesh.vertices[i].color = {0.8f, 0.5f + 0.15f * up, 0.25f + 0.1f * up};
        }
        torus_ = gpu.upload(mesh);
        mesh.release();
        constants_ = gpu.uniform_buffer(sizeof(Frame));
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
        gpu.update(constants_, &frame, sizeof frame);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, constants_);
        for (int side = 0; side < 2; side++) {
            gpu.viewport(half * static_cast<float>(side), 0, half, static_cast<float>(gpu.height));
            raster(false, true);
            glUseProgram(programs_[side][0]);
            torus_.draw_patches();
            raster(true, true, -200, -1.0f);
            glUseProgram(programs_[side][1]);
            torus_.draw_patches();
        }
        raster(false, true);
    }
    void release() override
    {
        for (auto &side : programs_)
            for (GLuint program : side) glDeleteProgram(program);
        torus_.release();
        glDeleteBuffers(1, &constants_);
    }

private:
    GLuint   programs_[2][2] = {};
    DrawMesh torus_;
    GLuint   constants_ = 0;
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
        static const char *const vertex = R"(
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
out vec3 v_position;
out vec3 v_normal;
out vec2 v_uv;
void main() { v_position = position; v_normal = normal; v_uv = uv; }
)";
        static const char *const control = R"(
layout(vertices = 3) out;
in vec3 v_position[];
in vec3 v_normal[];
in vec2 v_uv[];
out vec3 c_position[];
out vec3 c_normal[];
out vec2 c_uv[];
void main() {
    c_position[gl_InvocationID] = v_position[gl_InvocationID];
    c_normal[gl_InvocationID] = v_normal[gl_InvocationID];
    c_uv[gl_InvocationID] = v_uv[gl_InvocationID];
    if (gl_InvocationID == 0) {
        gl_TessLevelOuter[0] = gl_TessLevelOuter[1] = gl_TessLevelOuter[2] = params.x;
        gl_TessLevelInner[0] = params.x;
    }
}
)";
        static const char *const evaluation = R"(
// Direct3D's triangle_cw, as for the tessellation sample.
layout(triangles, equal_spacing, ccw) in;
layout(binding = 0) uniform sampler2D bricks;
in vec3 c_position[];
in vec3 c_normal[];
in vec2 c_uv[];
out vec3 e_normal;
out vec2 e_uv;
out vec3 e_world_position;
void main() {
    vec3 uvw = gl_TessCoord;
    vec3 position = uvw.x * c_position[0] + uvw.y * c_position[1] + uvw.z * c_position[2];
    vec3 normal = normalize(uvw.x * c_normal[0] + uvw.y * c_normal[1] + uvw.z * c_normal[2]);
    e_uv = uvw.x * c_uv[0] + uvw.y * c_uv[1] + uvw.z * c_uv[2];
    position += normal * (max(textureLod(bricks, e_uv, 0).a, 0.2) * params.y);
    vec4 w = vec4(position, 1) * world;
    gl_Position = w * view_projection;
    e_normal = (vec4(normal, 0) * world).xyz;
    e_world_position = w.xyz;
}
)";
        static const char *const fragment = R"(
layout(binding = 0) uniform sampler2D bricks;
in vec3 e_normal;
in vec2 e_uv;
in vec3 e_world_position;
layout(location = 0) out vec4 target;
void main() {
    vec3 n = normalize(e_normal);
    if (params.z > 0) {
        // The displaced surface's own facet normal, so the raised stones shade.
        vec3 facet = normalize(cross(dFdy(e_world_position), dFdx(e_world_position)));
        n = normalize(dot(facet, n) < 0 ? -facet : facet);
    }
    vec3 to_light = light.xyz - e_world_position;
    vec3 l = normalize(to_light);
    float falloff = clamp(1.6 - length(to_light) * 0.16, 0.0, 1.0);
    float diffuse = max(dot(n, l), 0.0) * falloff;
    target = vec4(texture(bricks, e_uv).rgb * (0.03 + 1.3 * diffuse), 1);
}
)";
        program_ = gpu.program("displacement", {.common = geometry_frame, .vertex = vertex, .fragment = fragment,
                                                .control = control, .evaluation = evaluation});
        Mesh mesh = torus_mesh(1.0f, 0.5f, 48, 24);
        torus_ = gpu.upload(mesh);
        mesh.release();
        const uint32_t size = 256;
        uint32_t *texels = brick_texels(size);
        bricks_ = gpu.texture(texels, size, size);
        release_memory(texels);
        sampler_ = gpu.sampler(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_REPEAT, 1.0f);
        constants_ = gpu.uniform_buffer(sizeof(Frame));
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
        glUseProgram(program_);
        glBindTextureUnit(0, bricks_);
        glBindSampler(0, sampler_);
        for (int side = 0; side < 2; side++) {
            frame.params = {8.0f, side == 0 ? 0.0f : 0.1f, static_cast<float>(side), 0};
            gpu.update(constants_, &frame, sizeof frame);
            glBindBufferBase(GL_UNIFORM_BUFFER, 0, constants_);
            gpu.viewport(half * static_cast<float>(side), 0, half, static_cast<float>(gpu.height));
            torus_.draw_patches();
        }
    }
    void release() override
    {
        glDeleteProgram(program_);
        torus_.release();
        glDeleteTextures(1, &bricks_);
        glDeleteSamplers(1, &sampler_);
        glDeleteBuffers(1, &constants_);
    }

private:
    GLuint   program_ = 0;
    DrawMesh torus_;
    GLuint   bricks_ = 0;
    GLuint   sampler_ = 0;
    GLuint   constants_ = 0;
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
        static const char *const common = R"(
layout(std140, binding = 0, row_major) uniform Frame { mat4 view_projection; vec4 eye; vec4 params; };
)";
        static const char *const vertex = R"(
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 4) in vec3 instance_position;
layout(location = 5) in vec3 instance_rotation;
layout(location = 6) in float instance_scale;
layout(location = 7) in uint instance_layer;
out vec3 v_normal;
out vec3 v_uv;
out vec3 v_world_position;
// Rows as Direct3D's float3x3 takes them, so v * m is its mul(v, m).
mat3 rows(float a, float b, float c, float d, float e, float f, float g, float h, float i) {
    return transpose(mat3(a, b, c, d, e, f, g, h, i));
}
mat3 rotation(vec3 a) {
    float sx = sin(a.x), cx = cos(a.x), sy = sin(a.y), cy = cos(a.y), sz = sin(a.z), cz = cos(a.z);
    mat3 rx = rows(1, 0, 0, 0, cx, sx, 0, -sx, cx);
    mat3 ry = rows(cy, 0, -sy, 0, 1, 0, sy, 0, cy);
    mat3 rz = rows(cz, sz, 0, -sz, cz, 0, 0, 0, 1);
    return rx * ry * rz;
}
void main() {
    // The whole field turns: each rock's offset is rotated with it.
    mat3 r = rotation(instance_rotation + vec3(0, params.x, 0));
    vec3 w = (position * instance_scale + instance_position) * r;
    gl_Position = vec4(w, 1) * view_projection;
    v_normal = normal * r;
    v_uv = vec3(uv, instance_layer);
    v_world_position = w;
}
)";
        static const char *const fragment = R"(
layout(binding = 0) uniform sampler2DArray rocks;
in vec3 v_normal;
in vec3 v_uv;
in vec3 v_world_position;
layout(location = 0) out vec4 target;
void main() {
    vec4 color = texture(rocks, v_uv);
    vec3 n = normalize(v_normal);
    vec3 l = normalize(eye.xyz - v_world_position);
    vec3 r = reflect(-l, n);
    float diffuse = max(dot(n, l), 0.0);
    float specular = pow(max(dot(r, l), 0.0), 32.0) * 0.375;
    target = vec4(color.rgb * (0.1 + diffuse) + specular, 1);
}
)";
        program_ = gpu.program("instancing", {.common = common, .vertex = vertex, .fragment = fragment});
        Mesh mesh = rock_mesh(5u);
        rock_ = gpu.upload(mesh);
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
        instances_ = gpu.buffer(instances, sizeof(RockInstance) * instance_count);
        release_memory(instances);
        layout_ = gpu.mesh_layout(rock_.vertices, rock_.indices);
        const GLuint offsets[3] = {0, 12, 24};
        const GLint sizes[3] = {3, 3, 1};
        for (GLuint a = 0; a < 3; a++) {
            glEnableVertexArrayAttrib(layout_, 4 + a);
            glVertexArrayAttribFormat(layout_, 4 + a, sizes[a], GL_FLOAT, GL_FALSE, offsets[a]);
            glVertexArrayAttribBinding(layout_, 4 + a, 1);
        }
        glEnableVertexArrayAttrib(layout_, 7);
        glVertexArrayAttribIFormat(layout_, 7, 1, GL_UNSIGNED_INT, 28);
        glVertexArrayAttribBinding(layout_, 7, 1);
        glVertexArrayVertexBuffer(layout_, 1, instances_, 0, sizeof(RockInstance));
        glVertexArrayBindingDivisor(layout_, 1, 1);
        const uint32_t size = 256;
        uint32_t *texels = rock_texels(size, layers);
        rocks_ = gpu.texture(texels, size, size, layers);
        release_memory(texels);
        sampler_ = gpu.sampler();
        constants_ = gpu.uniform_buffer(sizeof(Constants));
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
        gpu.update(constants_, &frame, sizeof frame);
        glUseProgram(program_);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, constants_);
        glBindTextureUnit(0, rocks_);
        glBindSampler(0, sampler_);
        glBindVertexArray(layout_);
        glDrawElementsInstanced(GL_TRIANGLES, static_cast<GLsizei>(rock_.index_count), GL_UNSIGNED_INT, nullptr,
                                instance_count);
    }
    void release() override
    {
        glDeleteProgram(program_);
        glDeleteVertexArrays(1, &layout_);
        rock_.release();
        glDeleteBuffers(1, &instances_);
        glDeleteTextures(1, &rocks_);
        glDeleteSamplers(1, &sampler_);
        glDeleteBuffers(1, &constants_);
    }

private:
    struct Constants {
        float4x4 view_projection;
        float4   eye;
        float4   params;
    };
    static constexpr uint32_t instance_count = 2048;
    static constexpr uint32_t layers = 5;
    GLuint   program_ = 0;
    GLuint   layout_ = 0;
    DrawMesh rock_;
    GLuint   instances_ = 0;
    GLuint   rocks_ = 0;
    GLuint   sampler_ = 0;
    GLuint   constants_ = 0;
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

#define PROBE_VERTEX MESH_INPUTS R"(
out vec3 v_normal;
out vec3 v_color;
out vec2 v_uv;
out vec3 v_world_position;
void main() {
    vec4 w = vec4(position, 1) * world;
    gl_Position = w * view_projection;
    v_normal = (vec4(normal, 0) * world).xyz;
    v_color = color;
    v_uv = uv * vec2(4, 1);
    v_world_position = w.xyz;
}
)"

const char *const probe_fragment = R"(
layout(binding = 0) uniform sampler2D panel;
in vec3 v_normal;
in vec3 v_color;
in vec2 v_uv;
in vec3 v_world_position;
layout(location = 0) out vec4 target;
void main() {
    vec3 color = texture(panel, v_uv).rgb * v_color;
    vec3 n = normalize(v_normal);
    vec3 l = normalize(light.xyz - v_world_position);
    vec3 v = normalize(eye.xyz - v_world_position);
    vec3 r = reflect(-l, n);
    float diffuse = max(dot(n, l), 0.0);
    float specular = pow(max(dot(r, v), 0.0), 16.0) * 0.75;
    target = vec4(color * (params.x + diffuse) + specular * params.y, 1);
}
)";

struct Probe {
    GLuint   program = 0;
    DrawMesh mesh;
    GLuint   panel = 0;
    GLuint   sampler = 0;
    GLuint   constants = 0;

    void init(Gpu &gpu, const char *name)
    {
        static const char *const vertex = PROBE_VERTEX;
        program = gpu.program(name, {.common = geometry_frame, .vertex = vertex, .fragment = probe_fragment});
        Mesh model = probe_mesh();
        mesh = gpu.upload(model);
        model.release();
        const uint32_t size = 256;
        uint32_t *texels = panel_texels(size);
        panel = gpu.texture(texels, size, size);
        release_memory(texels);
        sampler = gpu.sampler();
        constants = gpu.uniform_buffer(sizeof(Frame));
    }
    void draw(Gpu &gpu, const Frame &frame)
    {
        gpu.update(constants, &frame, sizeof frame);
        glUseProgram(program);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, constants);
        glBindTextureUnit(0, panel);
        glBindSampler(0, sampler);
        mesh.draw();
    }
    void release()
    {
        glDeleteProgram(program);
        mesh.release();
        glDeleteTextures(1, &panel);
        glDeleteSamplers(1, &sampler);
        glDeleteBuffers(1, &constants);
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
        probe_.init(gpu, "mesh");
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
        probe_.init(gpu, "multisampling");
        GLint samples = 0;
        glGetIntegerv(GL_MAX_SAMPLES, &samples);
        if (samples < static_cast<GLint>(sample_count)) return false;
        glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &color_);
        glTextureStorage2DMultisample(color_, sample_count, GL_RGBA8, static_cast<GLsizei>(gpu.width),
                                      static_cast<GLsizei>(gpu.height), GL_TRUE);
        glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &depth_);
        glTextureStorage2DMultisample(depth_, sample_count, GL_DEPTH24_STENCIL8, static_cast<GLsizei>(gpu.width),
                                      static_cast<GLsizei>(gpu.height), GL_TRUE);
        target_ = gpu.framebuffer(&color_, 1, depth_);
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        const float clear[4] = {1, 1, 1, 1};
        glBindFramebuffer(GL_FRAMEBUFFER, target_);
        glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);
        glClearBufferfv(GL_COLOR, 0, clear);
        glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);
        gpu.viewport(0, 0, static_cast<float>(gpu.width), static_cast<float>(gpu.height));
        glEnable(GL_MULTISAMPLE);
        probe_.draw(gpu, probe_frame(gpu, time, {1.2f, 0.9f, -5.0f}, {0.2f, 0.9f, 0}, 0.25f));
        const GLint w = static_cast<GLint>(gpu.width), h = static_cast<GLint>(gpu.height);
        glBlitNamedFramebuffer(target_, gpu.target, 0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, gpu.target);
    }
    void release() override
    {
        probe_.release();
        glDeleteFramebuffers(1, &target_);
        glDeleteTextures(1, &color_);
        glDeleteTextures(1, &depth_);
    }

private:
    static constexpr GLsizei sample_count = 4;
    Probe                    probe_;
    GLuint                   color_ = 0;
    GLuint                   depth_ = 0;
    GLuint                   target_ = 0;
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
        static const char *const vertex = MESH_INPUTS R"(
out vec3 v_normal;
out vec3 v_color;
out vec3 v_view;
out vec3 v_to_light;
void main() {
    vec4 w = vec4(position, 1) * world;
    gl_Position = w * view_projection;
    v_normal = (vec4(normal, 0) * world).xyz;
    v_color = color;
    v_view = eye.xyz - w.xyz;
    v_to_light = light.xyz - w.xyz;
}
)";
        static const char *const phong = R"(
in vec3 v_normal;
in vec3 v_color;
in vec3 v_view;
in vec3 v_to_light;
layout(location = 0) out vec4 target;
void main() {
    vec3 color = mix(v_color, vec3(dot(vec3(0.2126, 0.7152, 0.0722), v_color)), 0.65);
    vec3 n = normalize(v_normal), l = normalize(v_to_light), v = normalize(v_view);
    vec3 r = reflect(-l, n);
    vec3 diffuse = max(dot(n, l), 0.0) * color;
    vec3 specular = vec3(pow(max(dot(r, v), 0.0), 32.0) * 0.35);
    target = vec4(color + diffuse * 1.75 + specular, 1);
}
)";
        static const char *const toon = R"(
in vec3 v_normal;
in vec3 v_color;
in vec3 v_view;
in vec3 v_to_light;
layout(location = 0) out vec4 target;
void main() {
    vec3 n = normalize(v_normal), l = normalize(v_to_light);
    float intensity = dot(n, l);
    float shade = 1.0;
    shade = intensity < 0.5 ? 0.75 : shade;
    shade = intensity < 0.35 ? 0.6 : shade;
    shade = intensity < 0.25 ? 0.5 : shade;
    shade = intensity < 0.1 ? 0.25 : shade;
    target = vec4(v_color * 3.0 * shade, 1);
}
)";
        static const char *const wire = R"(
in vec3 v_normal;
in vec3 v_color;
in vec3 v_view;
in vec3 v_to_light;
layout(location = 0) out vec4 target;
void main() { target = vec4(v_color * 1.5, 1); }
)";
        const char *const fragments[3] = {phong, toon, wire};
        for (int i = 0; i < 3; i++)
            programs_[i] = gpu.program("pipelines", {.common = geometry_frame, .vertex = vertex, .fragment = fragments[i]});
        Mesh mesh = glade_mesh();
        glade_ = gpu.upload(mesh);
        mesh.release();
        constants_ = gpu.uniform_buffer(sizeof(Frame));
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
        gpu.update(constants_, &frame, sizeof frame);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, constants_);
        for (int i = 0; i < 3; i++) {
            gpu.viewport(third * static_cast<float>(i), 0, third, static_cast<float>(gpu.height));
            raster(i == 2, true);
            glUseProgram(programs_[i]);
            glade_.draw();
        }
        raster(false, true);
    }
    void release() override
    {
        for (GLuint program : programs_) glDeleteProgram(program);
        glade_.release();
        glDeleteBuffers(1, &constants_);
    }

private:
    GLuint   programs_[3] = {};
    DrawMesh glade_;
    GLuint   constants_ = 0;
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
