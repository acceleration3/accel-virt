// computeparticles, computeshader, particlefire, raytracing, bloom, radialblur, offscreen
#include "framework.h"

namespace samples {

namespace {

// ---------------------------------------------------------------- shared pieces

#define MESH_INPUTS                                  \
    "layout(location = 0) in vec3 position;\n"      \
    "layout(location = 1) in vec3 normal;\n"        \
    "layout(location = 2) in vec2 uv;\n"            \
    "layout(location = 3) in vec3 color;\n"

// A full-screen triangle from gl_VertexID alone: no vertex buffer, no layout.
const char *const quad_vertex = R"(
out vec2 v_uv;
void main() {
    int id = gl_VertexID;
    v_uv = vec2((id << 1) & 2, id & 2);
    gl_Position = vec4(v_uv * vec2(2, -2) + vec2(-1, 1), 0, 1);
}
)";

enum class Blend { opaque, additive, alpha };

void blend_state(Blend mode)
{
    if (mode == Blend::opaque) {
        glDisable(GL_BLEND);
        return;
    }
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(mode == Blend::alpha ? GL_SRC_ALPHA : GL_ONE, mode == Blend::alpha ? GL_ONE_MINUS_SRC_ALPHA : GL_ONE,
                        GL_ONE, GL_ONE);
}

void depth_state(bool test, bool write)
{
    if (test) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);
    glDepthMask(write ? GL_TRUE : GL_FALSE);
    glDepthFunc(GL_LEQUAL);
}

// Direct3D's default depth state: test, write, LESS.
void default_depth()
{
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}

// An off-screen colour target, optionally with its own depth, readable as a texture.
struct RenderTexture {
    GLuint   texture = 0;
    GLuint   depth = 0;
    GLuint   target = 0;
    uint32_t width = 0;
    uint32_t height = 0;

    // `renders` false: a texture compute writes, with no framebuffer.
    void create(Gpu &gpu, uint32_t w, uint32_t h, GLenum format, bool renders, bool with_depth)
    {
        width = w;
        height = h;
        texture = gpu.render_texture(format, w, h);
        if (with_depth) depth = gpu.render_texture(GL_DEPTH24_STENCIL8, w, h);
        if (renders) target = gpu.framebuffer(&texture, 1, depth);
    }
    void begin(Gpu &gpu, float r, float g, float b) const
    {
        const float clear[4] = {r, g, b, 1.0f};
        glBindFramebuffer(GL_FRAMEBUFFER, target);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glClearNamedFramebufferfv(target, GL_COLOR, 0, clear);
        if (depth != 0) {
            glDepthMask(GL_TRUE);
            glClearNamedFramebufferfi(target, GL_DEPTH_STENCIL, 0, 1.0f, 0);
        }
        gpu.viewport(0, 0, static_cast<float>(width), static_cast<float>(height));
    }
    void release()
    {
        glDeleteFramebuffers(1, &target);
        glDeleteTextures(1, &texture);
        glDeleteTextures(1, &depth);
        target = texture = depth = 0;
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
const char *const glow_scene_common = R"(
layout(std140, binding = 0, row_major) uniform Scene {
    mat4 world;
    mat4 view_projection;
    vec4 light;   // xyz: direction to the light
    vec4 eye;     // xyz: camera position
};
)";

const char *const glow_scene_vertex = MESH_INPUTS R"(
out vec3 v_normal;
out vec3 v_world_position;
out vec3 v_color;
out float v_glow;
void main() {
    vec4 w = vec4(position, 1) * world;
    gl_Position = w * view_projection;
    v_world_position = w.xyz;
    v_normal = (vec4(normal, 0) * world).xyz;
    v_color = color;
    v_glow = uv.x;
}
)";

#define GLOW_SCENE_INPUTS "in vec3 v_normal;\nin vec3 v_world_position;\nin vec3 v_color;\nin float v_glow;\n" \
                          "layout(location = 0) out vec4 target;\n"

const char *const glow_scene_phong = GLOW_SCENE_INPUTS R"(
void main() {
    vec3 n = normalize(v_normal);
    vec3 l = normalize(light.xyz);
    vec3 v = normalize(eye.xyz - v_world_position);
    float diffuse = clamp(dot(n, l), 0.0, 1.0);
    float specular = pow(clamp(dot(reflect(-l, n), v), 0.0, 1.0), 32.0);
    vec3 lit = v_color * (0.12 + 0.88 * diffuse) + specular * 0.6;
    target = vec4(mix(lit, v_color * (0.6 + 0.4 * diffuse) + specular * 0.3, v_glow), 1);
}
)";

const char *const glow_scene_emissive = GLOW_SCENE_INPUTS R"(
void main() { target = vec4(v_color * v_glow, 1); }
)";

struct GlowScene {
    float4x4 world;
    float4x4 view_projection;
    float4   light;
    float4   eye;
};

// ---------------------------------------------------------------- computeparticles

// A compute shader moves 256K particles between two texture buffers; a
// geometry shader expands each one into a sprite, blended additively.
class ComputeParticles final : public Sample {
public:
    const char *name() const override { return "computeparticles"; }
    bool init(Gpu &gpu) override
    {
        static const char *const sim_source = R"(
// A particle is two texels: (position, velocity) and (gradient position, -, -, -).
layout(std140, binding = 0) uniform Sim { float dt; float dest_x; float dest_y; uint count; float force; float unused0, unused1, unused2; };
layout(binding = 0) uniform samplerBuffer before;
layout(binding = 0, rgba32f) uniform writeonly imageBuffer after;
layout(local_size_x = 256) in;
vec2 attraction(vec2 pos, vec2 target) {
    vec2 delta = target - pos;
    float inv = inversesqrt(dot(delta, delta) + 0.5);
    return delta * inv * inv * inv * 0.0035;
}
vec2 repulsion(vec2 pos, vec2 target) {
    vec2 delta = target - pos;
    float d = sqrt(dot(delta, delta));
    return delta * (1.0 / (d * d * d)) * -0.000035;
}
void main() {
    int i = int(gl_GlobalInvocationID.x);
    if (uint(i) >= count) return;
    vec4 motion = texelFetch(before, i * 2);
    vec4 gradient = texelFetch(before, i * 2 + 1);
    vec2 pos = motion.xy, vel = motion.zw;
    vec2 target = vec2(dest_x, dest_y);
    vel += repulsion(pos, target) * 0.05 * force;
    vec2 moved = pos + vel * dt;
    if (moved.x < -1.0 || moved.x > 1.0 || moved.y < -1.0 || moved.y > 1.0)
        vel = -vel * 0.1 + attraction(moved, target) * 12.0 * force;
    else
        pos = moved;
    gradient.x += 0.02 * dt;
    if (gradient.x > 1.0) gradient.x -= 1.0;
    imageStore(after, i * 2, vec4(pos, vel));
    imageStore(after, i * 2 + 1, gradient);
}
)";
        static const char *const draw_common = R"(
layout(std140, binding = 0) uniform Sprite { vec2 half_size; float intensity; float unused; };
)";
        static const char *const draw_vertex = R"(
layout(binding = 0) uniform samplerBuffer particles;
out float v_gradient;
void main() {
    gl_Position = vec4(texelFetch(particles, gl_VertexID * 2).xy, 0.5, 1);
    v_gradient = texelFetch(particles, gl_VertexID * 2 + 1).x;
}
)";
        static const char *const draw_geometry = R"(
layout(points) in;
layout(triangle_strip, max_vertices = 4) out;
in float v_gradient[];
out vec2 g_uv;
out float g_gradient;
void main() {
    for (uint k = 0; k < 4; k++) {
        g_uv = vec2(k & 1u, k >> 1);
        gl_Position = gl_in[0].gl_Position + vec4((g_uv * 2 - 1) * half_size, 0, 0);
        g_gradient = v_gradient[0];
        EmitVertex();
    }
}
)";
        static const char *const draw_fragment = R"(
layout(binding = 1) uniform sampler2D ramp;
in vec2 g_uv;
in float g_gradient;
layout(location = 0) out vec4 target;
void main() {
    float glow = clamp(1 - length(g_uv * 2 - 1), 0.0, 1.0);
    glow *= glow;
    target = vec4(textureLod(ramp, vec2(g_gradient, 0.5), 0).rgb * glow * intensity, 1);
}
)";
        simulate_ = gpu.program("computeparticles_simulate", {.compute = sim_source});
        draw_ = gpu.program("computeparticles_draw", {.common = draw_common, .vertex = draw_vertex,
                                                      .fragment = draw_fragment, .geometry = draw_geometry});
        if (gpu.failed) return false;

        Particle *start = static_cast<Particle *>(allocate(sizeof(Particle) * count));
        Random random(0x5EEDu);
        for (uint32_t i = 0; i < count; i++) {
            start[i].pos = {random.range(-1, 1), random.range(-1, 1)};
            start[i].gradient = {start[i].pos.x * 0.5f, 0, 0, 0};
        }
        const uint32_t bytes = count * static_cast<uint32_t>(sizeof(Particle));
        initial_ = gpu.buffer(start, bytes);
        for (int i = 0; i < 2; i++) {
            particles_[i] = gpu.buffer(start, bytes);
            glCreateTextures(GL_TEXTURE_BUFFER, 1, &views_[i]);
            glTextureBuffer(views_[i], GL_RGBA32F, particles_[i]);
        }
        release_memory(start);

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
        sampler_ = gpu.sampler(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_REPEAT, 1.0f);
        sim_constants_ = gpu.uniform_buffer(32);
        sprite_constants_ = gpu.uniform_buffer(16);
        steps_ = 0;
        current_ = 0;
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        // The state at `time` is a fixed number of fixed steps from the start,
        // so a jump back in time replays from the initial particles.
        const uint32_t target = static_cast<uint32_t>((time + preroll) * steps_per_second + 0.001f);
        if (target < steps_) {
            glCopyNamedBufferSubData(initial_, particles_[0], 0, 0, count * static_cast<GLsizeiptr>(sizeof(Particle)));
            steps_ = 0;
            current_ = 0;
        }
        glUseProgram(simulate_);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, sim_constants_);
        for (; steps_ < target; steps_++) {
            glBindTextureUnit(0, views_[current_]);
            glBindImageTexture(0, views_[1 - current_], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
            const float t = static_cast<float>(steps_) / steps_per_second - preroll;
            const SimConstants sim = {2.5f / 60.0f, sin(t * 0.04f * 2 * pi) * 0.75f, 0, count, force, {0, 0, 0}};
            gpu.update(sim_constants_, &sim, sizeof sim);
            glDispatchCompute(count / 256, 1, 1);
            // Direct3D orders a dispatch's writes before the next read of
            // them; OpenGL is told.
            glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
            current_ = 1 - current_;
        }

        const float sprite[4] = {4.0f / static_cast<float>(gpu.width) * 2.0f,
                                 4.0f / static_cast<float>(gpu.height) * 2.0f, 0.2f, 0};
        gpu.update(sprite_constants_, sprite, sizeof sprite);
        gpu.begin_frame(0, 0, 0);
        glUseProgram(draw_);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, sprite_constants_);
        glBindTextureUnit(0, views_[current_]);
        glBindTextureUnit(1, ramp_);
        glBindSampler(1, sampler_);
        blend_state(Blend::additive);
        depth_state(false, false);
        glDisable(GL_CULL_FACE);
        gpu.draw_generated(GL_POINTS, count);
        blend_state(Blend::opaque);
    }
    void release() override
    {
        glDeleteProgram(simulate_);
        glDeleteProgram(draw_);
        glDeleteBuffers(1, &initial_);
        glDeleteTextures(2, views_);
        glDeleteBuffers(2, particles_);
        glDeleteTextures(1, &ramp_);
        glDeleteSamplers(1, &sampler_);
        glDeleteBuffers(1, &sim_constants_);
        glDeleteBuffers(1, &sprite_constants_);
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

    GLuint   simulate_ = 0;
    GLuint   draw_ = 0;
    GLuint   initial_ = 0;
    // Ping-pong, as the D3D11 port: one buffer read while the other is written.
    GLuint   particles_[2] = {};
    GLuint   views_[2] = {};
    GLuint   ramp_ = 0;
    GLuint   sampler_ = 0;
    GLuint   sim_constants_ = 0;
    GLuint   sprite_constants_ = 0;
    uint32_t steps_ = 0;
    uint32_t current_ = 0;
};

// ---------------------------------------------------------------- computeshader

// A painting generated on the CPU, filtered by a compute shader into an image
// texture, shown beside the original. The filter changes every two seconds.
class ComputeShader final : public Sample {
public:
    const char *name() const override { return "computeshader"; }
    bool init(Gpu &gpu) override
    {
        static const char *const filter_common = R"(
layout(binding = 0) uniform sampler2D source;
layout(binding = 0, rgba8) uniform writeonly image2D filtered;
layout(local_size_x = 16, local_size_y = 16) in;
vec3 texel(ivec2 p) {
    ivec2 size = textureSize(source, 0);
    return texelFetch(source, clamp(p, ivec2(0, 0), size - 1), 0).rgb;
}
float grey(ivec2 p) { vec3 c = texel(p); return (c.r + c.g + c.b) / 3.0; }
)";
        static const char *const sharpen = R"(
void main() {
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    vec3 sum = texel(p) * 9.0;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++)
            if (x != 0 || y != 0) sum -= texel(p + ivec2(x, y));
    imageStore(filtered, p, vec4(clamp(sum, 0.0, 1.0), 1));
}
)";
        static const char *const edgedetect = R"(
void main() {
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    float sum = grey(p);
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++)
            if (x != 0 || y != 0) sum -= grey(p + ivec2(x, y)) / 8.0;
    imageStore(filtered, p, vec4(vec3(clamp(sum / 0.1, 0.0, 1.0)), 1));
}
)";
        static const char *const emboss = R"(
void main() {
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    float sum = -grey(p + ivec2(-1, -1)) - grey(p) + 2.0 * grey(p + ivec2(1, 1));
    imageStore(filtered, p, vec4(vec3(clamp(sum + 0.5, 0.0, 1.0)), 1));
}
)";
        static const char *const show_fragment = R"(
layout(binding = 0) uniform sampler2D image;
in vec2 v_uv;
layout(location = 0) out vec4 target;
void main() { target = texture(image, v_uv); }
)";
        filters_[0] = gpu.program("computeshader_sharpen", {.common = filter_common, .compute = sharpen});
        filters_[1] = gpu.program("computeshader_edgedetect", {.common = filter_common, .compute = edgedetect});
        filters_[2] = gpu.program("computeshader_emboss", {.common = filter_common, .compute = emboss});
        show_ = gpu.program("computeshader_show", {.vertex = quad_vertex, .fragment = show_fragment});
        if (gpu.failed) return false;
        uint32_t *texels = static_cast<uint32_t *>(allocate(size * size * 4));
        paint(texels);
        image_ = gpu.texture(texels, size, size);
        release_memory(texels);
        filtered_.create(gpu, size, size, GL_RGBA8, false, false);
        sampler_ = gpu.sampler(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, 1.0f);
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        const uint32_t which = static_cast<uint32_t>(time / 2.0f) % 3u;
        glUseProgram(filters_[which]);
        glBindTextureUnit(0, image_);
        glBindSampler(0, 0);
        glBindImageTexture(0, filtered_.texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
        glDispatchCompute(size / 16, size / 16, 1);
        glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

        gpu.begin_frame(0.03f, 0.03f, 0.03f);
        glUseProgram(show_);
        glBindSampler(0, sampler_);
        const float w = static_cast<float>(gpu.width), h = static_cast<float>(gpu.height);
        const float side = min(h * 0.86f, w * 0.48f);
        const GLuint views[2] = {image_, filtered_.texture};
        for (int i = 0; i < 2; i++) {
            gpu.viewport(w * (i == 0 ? 0.25f : 0.75f) - side * 0.5f, (h - side) * 0.5f, side, side);
            glBindTextureUnit(0, views[i]);
            gpu.draw_fullscreen();
        }
    }
    void release() override
    {
        for (GLuint filter : filters_) glDeleteProgram(filter);
        glDeleteProgram(show_);
        glDeleteTextures(1, &image_);
        filtered_.release();
        glDeleteSamplers(1, &sampler_);
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


    GLuint        filters_[3] = {};
    GLuint        show_ = 0;
    GLuint        image_ = 0;
    RenderTexture filtered_;
    GLuint        sampler_ = 0;
};

// ---------------------------------------------------------------- particlefire

// A camp fire: normal-mapped ground and logs lit by the flickering fire, and
// flame and smoke billboards whose every property is a closed-form function
// of time and the instance id, expanded from gl_VertexID in the vertex shader.
class ParticleFire final : public Sample {
public:
    const char *name() const override { return "particlefire"; }
    bool init(Gpu &gpu) override
    {
        static const char *const scene_common = R"(
layout(std140, binding = 0, row_major) uniform Object {
    mat4 world;
    mat4 view_projection;
    vec4 light;    // xyz: position, w: intensity
    vec4 v_axis;   // world direction the texture's v grows along; w: 1 for the ground
    vec4 tint;
};
)";
        static const char *const scene_vertex = MESH_INPUTS R"(
out vec3 v_world_position;
out vec3 v_normal;
out vec2 v_uv;
void main() {
    vec4 w = vec4(position, 1) * world;
    gl_Position = w * view_projection;
    v_world_position = w.xyz;
    v_normal = (vec4(normal, 0) * world).xyz;
    v_uv = uv;
}
)";
        static const char *const scene_fragment = R"(
layout(binding = 0) uniform sampler2D albedo;
layout(binding = 1) uniform sampler2D normals;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;
layout(location = 0) out vec4 target;
void main() {
    vec3 N = normalize(v_normal);
    vec3 B = v_axis.xyz - N * dot(v_axis.xyz, N);
    B = dot(B, B) > 1e-4 ? normalize(B) : vec3(1, 0, 0);
    vec3 T = cross(B, N);
    vec3 t = texture(normals, v_uv).xyz * 2 - 1;
    vec3 n = normalize(T * t.x + B * t.y + N * t.z);
    vec3 base = texture(albedo, v_uv).rgb * tint.rgb;
    float r = length(v_world_position.xz);
    float ash = smoothstep(1.35, 0.95, r) * v_axis.w;
    base = mix(base, vec3(0.22, 0.14, 0.08) * (0.5 + base.g * 2.0), ash);
    vec3 L = light.xyz - v_world_position;
    float d = length(L);
    L /= d;
    float attenuation = light.w / (1.0 + 0.9 * d * d);
    vec3 fire = vec3(1.0, 0.62, 0.3);
    target = vec4(base * fire * clamp(dot(n, L), 0.0, 1.0) * attenuation + base * 0.008, 1);
}
)";
        static const char *const particle_common = R"(
layout(std140, binding = 0, row_major) uniform Particles {
    mat4 view;
    mat4 projection;
    float time; uint kind; vec2 unused;   // kind 0: flame, 1: smoke
};
)";
        static const char *const particle_vertex = R"(
out vec2 v_uv;
out float v_alpha;
out vec3 v_color;
float hash(uint n) {
    n = (n << 13) ^ n;
    n = n * (n * n * 15731u + 789221u) + 1376312589u;
    return float(n & 0x7fffffffu) / 2147483648.0;
}
void main() {
    uint vertex = uint(gl_VertexID), instance = uint(gl_InstanceID);
    uint seed = instance * 7u + kind * 104729u;
    float h0 = hash(seed), h1 = hash(seed + 1u), h2 = hash(seed + 2u), h3 = hash(seed + 3u), h4 = hash(seed + 4u);
    float angle = h3 * 6.2831853;
    vec3 pos; float size; float alpha; vec3 color; float spin;
    if (kind == 0u) {
        float life = 0.7 + 0.6 * h0;
        float age = fract(time / life + h1);
        float r = 0.5 * sqrt(h2) * (1.0 - age * 0.75);
        pos = vec3(cos(angle) * r, 0.12 + age * life * (1.0 + 0.7 * h4), sin(angle) * r);
        size = (0.38 + 0.26 * h4) * (1.0 - 0.55 * age);
        alpha = sin(age * 3.14159) * 0.55;
        color = mix(vec3(1.0, 0.8, 0.45), vec3(1.0, 0.3, 0.06), age);
        spin = h0 * 6.28 + age * (h4 - 0.5) * 4.0;
    } else {
        float life = 3.5 + 1.5 * h0;
        float age = fract(time / life + h1);
        float r = 0.2 * sqrt(h2) + age * 0.35;
        pos = vec3(cos(angle) * r + sin(time * 0.5 + h4 * 6.0) * 0.25 * age, 1.1 + age * life * 0.65, sin(angle) * r);
        size = 0.45 + 1.1 * age;
        alpha = 0.4 * smoothstep(0.0, 0.2, age) * (1.0 - age);
        color = vec3(0.3 + 0.15 * h4);
        spin = h0 * 6.28 + age * (h4 - 0.5) * 3.0;
    }
    vec2 corner = vec2(vertex & 1u, vertex >> 1);
    vec4 center = vec4(pos, 1) * view;
    center.xy += (corner * 2 - 1) * size;
    gl_Position = center * projection;
    float s = sin(spin), c = cos(spin);
    vec2 d = corner - 0.5;
    v_uv = vec2(c * d.x - s * d.y, s * d.x + c * d.y) + 0.5;
    v_alpha = alpha;
    v_color = color;
}
)";
        static const char *const flame_fragment = R"(
layout(binding = 0) uniform sampler2D sprite;
in vec2 v_uv;
in float v_alpha;
in vec3 v_color;
layout(location = 0) out vec4 target;
void main() {
    vec4 t = texture(sprite, v_uv);
    target = vec4(t.rgb * v_color * v_alpha, 0);
}
)";
        static const char *const smoke_fragment = R"(
layout(binding = 0) uniform sampler2D sprite;
in vec2 v_uv;
in float v_alpha;
in vec3 v_color;
layout(location = 0) out vec4 target;
void main() {
    vec4 t = texture(sprite, v_uv);
    target = vec4(t.rgb * v_color, t.a * v_alpha);
}
)";
        scene_ = gpu.program("particlefire_scene", {.common = scene_common, .vertex = scene_vertex,
                                                    .fragment = scene_fragment});
        flame_program_ = gpu.program("particlefire_flame", {.common = particle_common, .vertex = particle_vertex,
                                                            .fragment = flame_fragment});
        smoke_program_ = gpu.program("particlefire_smoke", {.common = particle_common, .vertex = particle_vertex,
                                                            .fragment = smoke_fragment});
        if (gpu.failed) return false;

        Mesh ground = plane_mesh(16.0f, 1, 6.0f);
        ground_ = gpu.upload(ground);
        ground.release();
        Mesh log = frustum_mesh(24, 1.0f, 1.0f, 1.0f);
        log_ = gpu.upload(log);
        log.release();

        make_textures(gpu);
        sampler_ = gpu.sampler();
        clamp_ = gpu.sampler(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, 1.0f);
        object_constants_ = gpu.uniform_buffer(sizeof(Object));
        particle_constants_ = gpu.uniform_buffer(sizeof(Particles));
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        const float orbit = 0.25f + time * 0.08f;
        const float3 eye = {sin(orbit) * 5.8f, 2.5f, -cos(orbit) * 5.8f};
        const float4x4 view = look_at(eye, {0, 0.9f, 0}, {0, 1, 0});
        const float4x4 projection = perspective(radians(60), gpu.aspect(), 0.1f, 100);
        const float flicker = 2.4f + 0.35f * sin(time * 17.0f) + 0.2f * sin(time * 29.3f + 1.0f) +
                              0.15f * sin(time * 7.1f);

        gpu.begin_frame(0, 0, 0);
        blend_state(Blend::opaque);
        default_depth();
        glDisable(GL_CULL_FACE);
        glUseProgram(scene_);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, object_constants_);
        glBindSampler(0, sampler_);
        glBindSampler(1, sampler_);

        Object object = {};
        object.view_projection = view * projection;
        object.light = {0, 0.7f, 0, flicker};
        object.world = float4x4::identity();
        object.v_axis = {0, 0, 1, 1};
        object.tint = {1, 1, 1, 1};
        gpu.update(object_constants_, &object, sizeof object);
        glBindTextureUnit(0, grass_);
        glBindTextureUnit(1, grass_normals_);
        ground_.draw();

        glBindTextureUnit(0, bark_);
        glBindTextureUnit(1, bark_normals_);
        for (int i = 0; i < 3; i++) {
            const float4x4 rotation = rotate_z(-pi * 0.5f + 0.22f) * rotate_y(radians(40.0f + 120.0f * i));
            object.world = scale({0.16f, 1.5f, 0.16f}) * translate({0, -0.35f, 0}) * rotation *
                           translate({0, 0.2f, 0});
            const float3 axis = transform_direction({0, 1, 0}, rotation);
            object.v_axis = {axis.x, axis.y, axis.z, 0};
            object.tint = {1, 1, 1, 1};
            gpu.update(object_constants_, &object, sizeof object);
            log_.draw();
        }

        Particles particles = {};
        particles.view = view;
        particles.projection = projection;
        particles.time = time;
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, particle_constants_);
        glBindSampler(0, clamp_);
        depth_state(true, false);

        particles.kind = 1;
        gpu.update(particle_constants_, &particles, sizeof particles);
        glUseProgram(smoke_program_);
        glBindTextureUnit(0, smoke_);
        blend_state(Blend::alpha);
        gpu.draw_generated(GL_TRIANGLE_STRIP, 4, smoke_count);

        particles.kind = 0;
        gpu.update(particle_constants_, &particles, sizeof particles);
        glUseProgram(flame_program_);
        glBindTextureUnit(0, flame_);
        blend_state(Blend::additive);
        gpu.draw_generated(GL_TRIANGLE_STRIP, 4, flame_count);

        blend_state(Blend::opaque);
        default_depth();
    }
    void release() override
    {
        glDeleteProgram(scene_);
        glDeleteProgram(flame_program_);
        glDeleteProgram(smoke_program_);
        ground_.release();
        log_.release();
        const GLuint textures[6] = {grass_, grass_normals_, bark_, bark_normals_, flame_, smoke_};
        glDeleteTextures(6, textures);
        glDeleteSamplers(1, &sampler_);
        glDeleteSamplers(1, &clamp_);
        glDeleteBuffers(1, &object_constants_);
        glDeleteBuffers(1, &particle_constants_);
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


    GLuint   scene_ = 0;
    GLuint   flame_program_ = 0;
    GLuint   smoke_program_ = 0;
    DrawMesh ground_;
    DrawMesh log_;
    GLuint   grass_ = 0;
    GLuint   grass_normals_ = 0;
    GLuint   bark_ = 0;
    GLuint   bark_normals_ = 0;
    GLuint   flame_ = 0;
    GLuint   smoke_ = 0;
    GLuint   sampler_ = 0;
    GLuint   clamp_ = 0;
    GLuint   object_constants_ = 0;
    GLuint   particle_constants_ = 0;
};

// ---------------------------------------------------------------- raytracing

// A compute shader ray traces three spheres over a plane (shadows, one
// reflection bounce, fog) into an image texture that a full-screen pass shows.
class Raytracing final : public Sample {
public:
    const char *name() const override { return "raytracing"; }
    bool init(Gpu &gpu) override
    {
        static const char *const trace_source = R"(
layout(binding = 0, rgba8) uniform writeonly image2D result;
layout(std140, binding = 0) uniform Scene { vec3 light_position; float aspect; vec3 camera; float unused; uvec2 size; uvec2 unused2; };
layout(local_size_x = 16, local_size_y = 16) in;
const vec3 centers[3] = vec3[3](vec3(-2.25, 1.0, 0.0), vec3(0.0, 2.5, 0.0), vec3(2.25, 1.0, 0.0));
const vec3 colors[3] = vec3[3](vec3(1, 0, 0), vec3(0, 0, 1), vec3(0, 1, 0));
float sphere_hit(vec3 o, vec3 d, vec3 center) {
    vec3 oc = o - center;
    float b = dot(oc, d);
    float h = b * b - (dot(oc, oc) - 1.0);
    return h < 0.0 ? -1.0 : -b - sqrt(h);
}
int intersect(vec3 o, vec3 d, out float t) {
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
float shadow(vec3 o, vec3 d) {
    for (int i = 0; i < 3; i++)
        if (sphere_hit(o, d, centers[i]) > 0.0001) return 0.5;
    return 1.0;
}
vec3 shade(inout vec3 o, inout vec3 d) {
    float t;
    int id = intersect(o, d, t);
    if (id < 0) return vec3(0, 0, 0);
    vec3 p = o + t * d;
    vec3 l = normalize(light_position - p);
    vec3 n = vec3(0, 1, 0);
    vec3 color;
    if (id == 3) {
        color = vec3(clamp(dot(n, l), 0.0, 1.0));
    } else {
        n = p - centers[id];
        float diffuse = clamp(dot(n, l), 0.0, 1.0);
        float specular = pow(clamp(dot(n, normalize(l + normalize(camera))), 0.0, 1.0), 32.0);
        color = diffuse * colors[id] + specular * 2.0;
    }
    color *= shadow(p, l);
    color = mix(color, vec3(0, 0, 0), clamp(t / 20.0, 0.0, 1.0));
    d = reflect(d, n);
    o = p;
    return color;
}
void main() {
    uvec2 id = gl_GlobalInvocationID.xy;
    if (id.x >= size.x || id.y >= size.y) return;
    vec2 uv = (vec2(id) + 0.5) / vec2(size);
    vec3 o = camera;
    vec3 d = normalize(vec3((uv.x * 2 - 1) * aspect, 1 - uv.y * 2, -1));
    vec3 color = shade(o, d);
    vec3 reflected = shade(o, d);
    color = 0.6 * color + 0.4 * mix(reflected, color, 0.6);
    imageStore(result, ivec2(id), vec4(color, 1));
}
)";
        static const char *const show_fragment = R"(
layout(binding = 0) uniform sampler2D image;
layout(location = 0) out vec4 target;
void main() { target = texelFetch(image, ivec2(gl_FragCoord.xy), 0); }
)";
        trace_ = gpu.program("raytracing_trace", {.compute = trace_source});
        show_ = gpu.program("raytracing_show", {.vertex = quad_vertex, .fragment = show_fragment});
        image_.create(gpu, gpu.width, gpu.height, GL_RGBA8, false, false);
        constants_ = gpu.uniform_buffer(sizeof(TraceConstants));
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        const float a = time / 8.0f * 2 * pi;
        const TraceConstants scene = {{sin(a) * 2.0f, 5.0f, cos(a) * 2.0f}, gpu.aspect(), {0, 1.5f, 4.0f}, 0,
                                      {gpu.width, gpu.height, 0, 0}};
        gpu.update(constants_, &scene, sizeof scene);
        glUseProgram(trace_);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, constants_);
        glBindImageTexture(0, image_.texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
        glDispatchCompute((gpu.width + 15) / 16, (gpu.height + 15) / 16, 1);
        glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

        gpu.begin_frame(0, 0, 0);
        glUseProgram(show_);
        glBindTextureUnit(0, image_.texture);
        glBindSampler(0, 0);
        gpu.draw_fullscreen();
    }
    void release() override
    {
        glDeleteProgram(trace_);
        glDeleteProgram(show_);
        image_.release();
        glDeleteBuffers(1, &constants_);
    }

private:
    struct TraceConstants {
        float3   light;
        float    aspect;
        float3   camera;
        float    unused;
        uint32_t size[4];
    };
    GLuint        trace_ = 0;
    GLuint        show_ = 0;
    RenderTexture image_;
    GLuint        constants_ = 0;
};

// ---------------------------------------------------------------- bloom

const char *const blur_fragment = R"(
layout(binding = 0) uniform sampler2D image;
layout(std140, binding = 0) uniform Blur { vec2 texel_step; float strength; float unused; };
in vec2 v_uv;
layout(location = 0) out vec4 target;
void main() {
    const float weight[5] = float[5](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    vec3 sum = texture(image, v_uv).rgb * weight[0];
    for (int i = 1; i < 5; i++) {
        sum += texture(image, v_uv + texel_step * i).rgb * weight[i] * strength;
        sum += texture(image, v_uv - texel_step * i).rgb * weight[i] * strength;
    }
    target = vec4(sum, 1);
}
)";

// A flying saucer whose glowing parts are drawn again into a small target,
// blurred vertically there, then horizontally onto the frame, additively.
class Bloom final : public Sample {
public:
    const char *name() const override { return "bloom"; }
    bool init(Gpu &gpu) override
    {
        static const char *const sky_fragment = R"(
layout(std140, binding = 0) uniform Sky { vec4 right; vec4 up; vec4 forward; };
in vec2 v_uv;
layout(location = 0) out vec4 target;
float hash(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}
void main() {
    vec2 ndc = vec2(v_uv.x * 2 - 1, 1 - v_uv.y * 2);
    vec3 d = normalize(forward.xyz + ndc.x * right.xyz + ndc.y * up.xyz);
    vec3 cell = floor(d * 260.0);
    float star = step(0.9965, hash(cell)) * (0.3 + 0.7 * hash(cell + 7.0));
    float y = abs(d.y + 0.05);
    vec3 glow = vec3(0.35, 0.55, 1.0) * (exp(-y * 5.0) * 0.35 + exp(-y * 30.0) * 0.6) + exp(-y * 220.0) * 0.9;
    target = vec4(glow + star * 0.6, 1);
}
)";
        phong_ = gpu.program("bloom_phong", {.common = glow_scene_common, .vertex = glow_scene_vertex,
                                             .fragment = glow_scene_phong});
        glow_program_ = gpu.program("bloom_glow", {.common = glow_scene_common, .vertex = glow_scene_vertex,
                                                   .fragment = glow_scene_emissive});
        blur_ = gpu.program("bloom_blur", {.vertex = quad_vertex, .fragment = blur_fragment});
        sky_ = gpu.program("bloom_sky", {.vertex = quad_vertex, .fragment = sky_fragment});
        if (gpu.failed) return false;
        build_saucer(gpu);
        glow_.create(gpu, glow_size, glow_size, GL_RGBA16F, true, true);
        blurred_.create(gpu, glow_size, glow_size, GL_RGBA16F, true, false);
        scene_constants_ = gpu.uniform_buffer(sizeof(GlowScene));
        sky_constants_ = gpu.uniform_buffer(48);
        blur_constants_ = gpu.uniform_buffer(16);
        sampler_ = gpu.sampler(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, 1.0f);
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
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
        gpu.update(scene_constants_, &scene, sizeof scene);
        glDisable(GL_CULL_FACE);
        blend_state(Blend::opaque);
        default_depth();

        glow_.begin(gpu, 0, 0, 0);
        draw_saucer(glow_program_);

        const float vertical[4] = {0, 1.0f / glow_size, 1.5f, 0};
        gpu.update(blur_constants_, vertical, sizeof vertical);
        blurred_.begin(gpu, 0, 0, 0);
        depth_state(false, false);
        glUseProgram(blur_);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, blur_constants_);
        glBindTextureUnit(0, glow_.texture);
        glBindSampler(0, sampler_);
        gpu.draw_fullscreen();

        gpu.begin_frame(0, 0, 0);
        const float t = tan(fov * 0.5f);
        const float3 forward = normalize(at - eye), right = normalize(cross({0, 1, 0}, forward));
        const float3 up = cross(forward, right);
        const float sky[12] = {right.x * t * gpu.aspect(), right.y * t * gpu.aspect(), right.z * t * gpu.aspect(), 0,
                               up.x * t, up.y * t, up.z * t, 0, forward.x, forward.y, forward.z, 0};
        gpu.update(sky_constants_, sky, sizeof sky);
        depth_state(false, false);
        glUseProgram(sky_);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, sky_constants_);
        gpu.draw_fullscreen();
        default_depth();
        draw_saucer(phong_);

        const float horizontal[4] = {1.0f / glow_size, 0, 1.5f, 0};
        gpu.update(blur_constants_, horizontal, sizeof horizontal);
        depth_state(false, false);
        blend_state(Blend::additive);
        glUseProgram(blur_);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, blur_constants_);
        glBindTextureUnit(0, blurred_.texture);
        glBindSampler(0, sampler_);
        gpu.draw_fullscreen();
        blend_state(Blend::opaque);
        default_depth();
    }
    void release() override
    {
        glDeleteProgram(phong_);
        glDeleteProgram(glow_program_);
        glDeleteProgram(blur_);
        glDeleteProgram(sky_);
        saucer_.release();
        glow_.release();
        blurred_.release();
        glDeleteBuffers(1, &scene_constants_);
        glDeleteBuffers(1, &sky_constants_);
        glDeleteBuffers(1, &blur_constants_);
        glDeleteSamplers(1, &sampler_);
    }

private:
    static constexpr uint32_t glow_size = 256;

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
        saucer_ = gpu.upload(builder.mesh);
        builder.mesh.release();
        sphere.release();
        ring.release();
        rod.release();
        cone.release();
    }
    void draw_saucer(GLuint program)
    {
        glUseProgram(program);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, scene_constants_);
        saucer_.draw();
    }

    GLuint        phong_ = 0;
    GLuint        glow_program_ = 0;
    GLuint        blur_ = 0;
    GLuint        sky_ = 0;
    DrawMesh      saucer_;
    RenderTexture glow_;
    RenderTexture blurred_;
    GLuint        scene_constants_ = 0;
    GLuint        sky_constants_ = 0;
    GLuint        blur_constants_ = 0;
    GLuint        sampler_ = 0;
};

// ---------------------------------------------------------------- radialblur

// A caged glowing sphere: its glowing faces drawn into an off-screen target,
// then blurred radially from the centre and added over the lit scene.
class RadialBlur final : public Sample {
public:
    const char *name() const override { return "radialblur"; }
    bool init(Gpu &gpu) override
    {
        static const char *const radial_fragment = R"(
layout(binding = 0) uniform sampler2D image;
layout(std140, binding = 0) uniform Radial { vec2 texel; float blur_scale; float strength; vec2 origin; vec2 unused; };
in vec2 v_uv;
layout(location = 0) out vec4 target;
void main() {
    vec2 uv = v_uv + texel * 0.5 - origin;
    vec4 color = vec4(0);
    for (int i = 0; i < 16; i++) {
        float s = 1.0 - blur_scale * (i / 15.0);
        color += texture(image, uv * s + origin);
    }
    target = color / 16.0 * strength;
}
)";
        phong_ = gpu.program("radialblur_phong", {.common = glow_scene_common, .vertex = glow_scene_vertex,
                                                  .fragment = glow_scene_phong});
        glow_program_ = gpu.program("radialblur_glow", {.common = glow_scene_common, .vertex = glow_scene_vertex,
                                                        .fragment = glow_scene_emissive});
        radial_ = gpu.program("radialblur_radial", {.vertex = quad_vertex, .fragment = radial_fragment});
        if (gpu.failed) return false;
        build_cage(gpu);
        glow_.create(gpu, glow_size, glow_size, GL_RGBA8, true, true);
        scene_constants_ = gpu.uniform_buffer(sizeof(GlowScene));
        radial_constants_ = gpu.uniform_buffer(32);
        sampler_ = gpu.sampler(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, 1.0f);
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        const float3 eye = {0, 0, -12};
        GlowScene scene = {};
        scene.world = rotate_y(time / 8.0f * 2 * pi) * rotate_y(radians(-28.75f)) *
                      rotate_x(radians(-16.25f));
        scene.view_projection = look_at(eye, {0, 0, 0}, {0, 1, 0}) * perspective(radians(45), gpu.aspect(), 0.1f, 100);
        scene.light = {0.3f, 0.8f, -1.0f, 0};
        scene.eye = {eye.x, eye.y, eye.z, 1};
        gpu.update(scene_constants_, &scene, sizeof scene);
        glDisable(GL_CULL_FACE);
        blend_state(Blend::opaque);
        default_depth();

        glow_.begin(gpu, 0, 0, 0);
        draw_cage(glow_program_);

        gpu.begin_frame(0, 0, 0);
        draw_cage(phong_);

        const float radial[8] = {1.0f / glow_size, 1.0f / glow_size, 0.25f, 0.75f, 0.5f, 0.5f, 0, 0};
        gpu.update(radial_constants_, radial, sizeof radial);
        depth_state(false, false);
        blend_state(Blend::additive);
        glUseProgram(radial_);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, radial_constants_);
        glBindTextureUnit(0, glow_.texture);
        glBindSampler(0, sampler_);
        gpu.draw_fullscreen();
        blend_state(Blend::opaque);
        default_depth();
    }
    void release() override
    {
        glDeleteProgram(phong_);
        glDeleteProgram(glow_program_);
        glDeleteProgram(radial_);
        cage_.release();
        glow_.release();
        glDeleteBuffers(1, &scene_constants_);
        glDeleteBuffers(1, &radial_constants_);
        glDeleteSamplers(1, &sampler_);
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
        cage_ = gpu.upload(builder.mesh);
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
    void draw_cage(GLuint program)
    {
        glUseProgram(program);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, scene_constants_);
        cage_.draw();
    }

    GLuint        phong_ = 0;
    GLuint        glow_program_ = 0;
    GLuint        radial_ = 0;
    DrawMesh      cage_;
    RenderTexture glow_;
    GLuint        scene_constants_ = 0;
    GLuint        radial_constants_ = 0;
    GLuint        sampler_ = 0;
};

// ---------------------------------------------------------------- offscreen

// A mirror floor: the scene, reflected and clipped at the floor plane, is rendered into a texture that the floor samples
// with projected coordinates.
class Offscreen final : public Sample {
public:
    const char *name() const override { return "offscreen"; }
    bool init(Gpu &gpu) override
    {
        static const char *const phong_common = R"(
layout(std140, binding = 0, row_major) uniform Object {
    mat4 world;
    mat4 view_projection;
    vec4 light;    // xyz: position
    vec4 eye;
    vec4 material;    // rgb: colour, w: specular strength
    vec4 clip_plane;   // the world position must be on its positive side
};
)";
        static const char *const phong_vertex = MESH_INPUTS R"(
out vec3 v_normal;
out vec3 v_world_position;
void main() {
    vec4 w = vec4(position, 1) * world;
    gl_Position = w * view_projection;
    v_world_position = w.xyz;
    v_normal = (vec4(normal, 0) * world).xyz;
}
)";
        static const char *const phong_fragment = R"(
in vec3 v_normal;
in vec3 v_world_position;
layout(location = 0) out vec4 target;
void main() {
    if (dot(vec4(v_world_position, 1), clip_plane) < 0) discard;
    vec3 n = normalize(v_normal);
    vec3 l = normalize(light.xyz - v_world_position);
    vec3 v = normalize(eye.xyz - v_world_position);
    float diffuse = clamp(dot(n, l), 0.0, 1.0);
    float specular = pow(clamp(dot(reflect(-l, n), v), 0.0, 1.0), 24.0) * material.w;
    target = vec4(material.rgb * (0.08 + 0.92 * diffuse) + specular, 1);
}
)";
        static const char *const mirror_vertex = MESH_INPUTS R"(
out vec2 v_uv;
out vec4 v_projected;
void main() {
    gl_Position = (vec4(position, 1) * world) * view_projection;
    v_projected = gl_Position;
    v_uv = uv;
}
)";
        static const char *const mirror_fragment = R"(
layout(binding = 0) uniform sampler2D reflection;
layout(binding = 1) uniform sampler2D plate;
in vec2 v_uv;
in vec4 v_projected;
layout(location = 0) out vec4 target;
void main() {
    vec2 ndc = v_projected.xy / v_projected.w;
    vec2 uv = vec2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
    const float blur = 1.0 / 512.0;
    vec4 mirrored = vec4(0);
    for (int x = -3; x <= 3; x++)
        for (int y = -3; y <= 3; y++)
            mirrored += texture(reflection, uv + vec2(x, y) * blur) / 49.0;
    vec4 color = texture(plate, v_uv);
    target = vec4(color.rgb * 0.25 + mirrored.rgb * 1.5 * color.r, 1);
}
)";
        phong_ = gpu.program("offscreen_phong", {.common = phong_common, .vertex = phong_vertex,
                                                 .fragment = phong_fragment});
        mirror_program_ = gpu.program("offscreen_mirror", {.common = phong_common, .vertex = mirror_vertex,
                                                           .fragment = mirror_fragment});
        if (gpu.failed) return false;
        Mesh knot = knot_mesh(384, 20);
        knot_ = gpu.upload(knot);
        knot.release();
        Mesh ball = sphere_mesh(0.3f, 32, 16);
        ball_ = gpu.upload(ball);
        ball.release();
        Mesh floor = plane_mesh(8.0f, 1, 2.0f);
        floor_ = gpu.upload(floor);
        floor.release();
        make_plate(gpu);
        mirror_.create(gpu, mirror_size, mirror_size, GL_RGBA8, true, true);
        object_constants_ = gpu.uniform_buffer(sizeof(Object));
        sampler_ = gpu.sampler();
        clamp_ = gpu.sampler(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, 1.0f);
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        const float3 eye = {0, 2.6f, -6.2f};
        const float4x4 view_projection =
            look_at(eye, {0, 0.6f, 0}, {0, 1, 0}) * perspective(radians(60), gpu.aspect(), 0.1f, 100);
        const float4x4 turn = rotate_y(0.6f + time * 0.25f);
        const float3 light = {1.5f, 4.0f, -2.5f};
        glDisable(GL_CULL_FACE);
        blend_state(Blend::opaque);
        default_depth();

        mirror_.begin(gpu, 0, 0, 0);
        draw_objects(gpu, turn * scale({1, -1, 1}), view_projection, {light.x, -light.y, light.z}, eye,
                     {0, -1, 0, 0}, time);

        gpu.begin_frame(0, 0, 0);
        Object floor = {};
        floor.world = turn;
        floor.view_projection = view_projection;
        gpu.update(object_constants_, &floor, sizeof floor);
        glUseProgram(mirror_program_);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, object_constants_);
        glBindTextureUnit(0, mirror_.texture);
        glBindTextureUnit(1, plate_);
        glBindSampler(0, clamp_);
        glBindSampler(1, sampler_);
        floor_.draw();

        draw_objects(gpu, turn, view_projection, light, eye, {0, 0, 0, 1}, time);
    }
    void release() override
    {
        glDeleteProgram(phong_);
        glDeleteProgram(mirror_program_);
        knot_.release();
        ball_.release();
        floor_.release();
        glDeleteTextures(1, &plate_);
        mirror_.release();
        glDeleteBuffers(1, &object_constants_);
        glDeleteSamplers(1, &sampler_);
        glDeleteSamplers(1, &clamp_);
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

    void draw_objects(Gpu &gpu, const float4x4 &placement, const float4x4 &view_projection, float3 light, float3 eye,
                      float4 clip, float time)
    {
        glUseProgram(phong_);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, object_constants_);
        Object object = {};
        object.view_projection = view_projection;
        object.light = {light.x, light.y, light.z, 1};
        object.eye = {eye.x, eye.y, eye.z, 1};
        object.clip = clip;
        object.world = rotate_y(time * 0.5f) * translate({0, 1.22f, 0.3f}) * placement;
        object.color = {0.85f, 0.02f, 0.02f, 0.8f};
        gpu.update(object_constants_, &object, sizeof object);
        knot_.draw();
        object.world = translate({0.2f, 0.3f, -1.15f}) * placement;
        object.color = {0.06f, 0.06f, 0.07f, 1.0f};
        gpu.update(object_constants_, &object, sizeof object);
        ball_.draw();
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


    GLuint        phong_ = 0;
    GLuint        mirror_program_ = 0;
    DrawMesh      knot_;
    DrawMesh      ball_;
    DrawMesh      floor_;
    GLuint        plate_ = 0;
    RenderTexture mirror_;
    GLuint        object_constants_ = 0;
    GLuint        sampler_ = 0;
    GLuint        clamp_ = 0;
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
