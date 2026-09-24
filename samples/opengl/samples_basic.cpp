// triangle, pushconstants, gears, texture
#include "framework.h"

namespace samples {

namespace {

const char *const lit_frame = R"(
layout(std140, binding = 0, row_major) uniform Frame {
    mat4 world;
    mat4 view_projection;
    vec4 light;      // xyz: direction to the light
    vec4 tint;       // multiplies the vertex colour
};
)";

const char *const lit_vertex = R"(
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec3 color;
out vec3 v_normal;
out vec3 v_color;
out vec2 v_uv;
void main() {
    vec4 w = vec4(position, 1) * world;
    gl_Position = w * view_projection;
    v_normal = (vec4(normal, 0) * world).xyz;
    v_color = color * tint.rgb;
    v_uv = uv;
}
)";

const char *const lit_fragment = R"(
in vec3 v_normal;
in vec3 v_color;
in vec2 v_uv;
layout(location = 0) out vec4 target;
void main() {
    vec3 n = normalize(v_normal);
    float diffuse = clamp(dot(n, normalize(light.xyz)), 0.0, 1.0);
    float specular = pow(clamp(dot(reflect(-normalize(light.xyz), n), vec3(0, 0, -1)), 0.0, 1.0), 24.0) * 0.5;
    target = vec4(v_color * (0.2 + 0.8 * diffuse) + specular, 1);
}
)";

struct LitFrame {
    float4x4 world;
    float4x4 view_projection;
    float4   light;
    float4   tint;
};

class Triangle final : public Sample {
public:
    const char *name() const override { return "triangle"; }
    bool init(Gpu &gpu) override
    {
        program_ = gpu.program("triangle", {.common = R"(
layout(std140, binding = 0) uniform Frame { float angle; float aspect; vec2 unused; };
)",
                                            .vertex = R"(
layout(location = 0) in vec2 position;
layout(location = 1) in vec3 color;
out vec3 v_color;
void main() {
    float s = sin(angle), c = cos(angle);
    gl_Position = vec4((position.x * c - position.y * s) / aspect, position.x * s + position.y * c, 0.5, 1);
    v_color = color;
}
)",
                                            .fragment = R"(
in vec3 v_color;
layout(location = 0) out vec4 target;
void main() { target = vec4(v_color, 1); }
)"});
        const float vertices[15] = {0.0f, 0.8f, 1, 0, 0, 0.7f, -0.5f, 0, 1, 0, -0.7f, -0.5f, 0, 0, 1};
        vertices_ = gpu.buffer(vertices, sizeof vertices);
        glCreateVertexArrays(1, &layout_);
        glEnableVertexArrayAttrib(layout_, 0);
        glVertexArrayAttribFormat(layout_, 0, 2, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribBinding(layout_, 0, 0);
        glEnableVertexArrayAttrib(layout_, 1);
        glVertexArrayAttribFormat(layout_, 1, 3, GL_FLOAT, GL_FALSE, 8);
        glVertexArrayAttribBinding(layout_, 1, 0);
        glVertexArrayVertexBuffer(layout_, 0, vertices_, 0, 20);
        constants_ = gpu.uniform_buffer(16);
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        const float frame[4] = {time, gpu.aspect(), 0, 0};
        gpu.update(constants_, frame, sizeof frame);
        gpu.begin_frame(0.05f, 0.05f, 0.1f);
        glUseProgram(program_);
        glBindVertexArray(layout_);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, constants_);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    void release() override
    {
        glDeleteProgram(program_);
        glDeleteVertexArrays(1, &layout_);
        glDeleteBuffers(1, &vertices_);
        glDeleteBuffers(1, &constants_);
    }

private:
    GLuint program_ = 0;
    GLuint layout_ = 0;
    GLuint vertices_ = 0;
    GLuint constants_ = 0;
};

// Vulkan's push constants become a small uniform buffer rewritten per draw:
// a ring of spheres, each with its own colour and offset.
class PushConstants final : public Sample {
public:
    const char *name() const override { return "pushconstants"; }
    bool init(Gpu &gpu) override
    {
        program_ = gpu.program("lit", {.common = lit_frame, .vertex = lit_vertex, .fragment = lit_fragment});
        Mesh mesh = sphere_mesh(0.35f, 32, 16);
        sphere_ = gpu.upload(mesh);
        mesh.release();
        constants_ = gpu.uniform_buffer(sizeof(LitFrame));
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        gpu.begin_frame(0.08f, 0.08f, 0.12f);
        glUseProgram(program_);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, constants_);
        const float4x4 vp = look_at({0, 2.5f, -5}, {0, 0, 0}, {0, 1, 0}) * perspective(radians(60), gpu.aspect(), 0.1f, 100);
        for (int i = 0; i < 16; i++) {
            const float a = static_cast<float>(i) / 16.0f * 2 * pi + time;
            LitFrame frame;
            frame.world = translate({cos(a) * 2.2f, sin(time * 2 + i) * 0.4f, sin(a) * 2.2f});
            frame.view_projection = vp;
            frame.light = {0.5f, 1, -0.5f, 0};
            frame.tint = {0.5f + 0.5f * sin(a), 0.5f + 0.5f * sin(a + 2), 0.5f + 0.5f * sin(a + 4), 1};
            gpu.update(constants_, &frame, sizeof frame);
            sphere_.draw();
        }
    }
    void release() override
    {
        glDeleteProgram(program_);
        sphere_.release();
        glDeleteBuffers(1, &constants_);
    }

private:
    GLuint   program_ = 0;
    DrawMesh sphere_;
    GLuint   constants_ = 0;
};

class Gears final : public Sample {
public:
    const char *name() const override { return "gears"; }
    bool init(Gpu &gpu) override
    {
        program_ = gpu.program("lit", {.common = lit_frame, .vertex = lit_vertex, .fragment = lit_fragment});
        Mesh a = gear_mesh(1.0f, 4.0f, 1.0f, 20, 0.7f, {1.0f, 0.1f, 0.1f});
        Mesh b = gear_mesh(0.5f, 2.0f, 2.0f, 10, 0.7f, {0.1f, 1.0f, 0.2f});
        Mesh d = gear_mesh(1.3f, 2.0f, 0.5f, 10, 0.7f, {0.2f, 0.2f, 1.0f});
        gears_[0] = gpu.upload(a);
        gears_[1] = gpu.upload(b);
        gears_[2] = gpu.upload(d);
        a.release();
        b.release();
        d.release();
        constants_ = gpu.uniform_buffer(sizeof(LitFrame));
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        gpu.begin_frame(0.0f, 0.0f, 0.0f);
        glUseProgram(program_);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, constants_);
        const float4x4 view = rotate_y(time * 0.3f) * rotate_x(0.35f) * translate({0, 0, 16});
        const float4x4 vp = view * perspective(radians(60), gpu.aspect(), 0.1f, 100);
        const float angle = time * 2.0f;
        const float3 positions[3] = {{-3.0f, 0.0f, 0.0f}, {3.1f, 0.0f, 0.0f}, {-3.1f, -6.2f, 0.0f}};
        const float rotations[3] = {angle, -2.0f * angle - radians(9), -2.0f * angle - radians(30)};
        for (int i = 0; i < 3; i++) {
            LitFrame frame;
            frame.world = rotate_z(rotations[i]) * translate(positions[i]);
            frame.view_projection = vp;
            frame.light = {0.3f, 0.6f, -1, 0};
            frame.tint = {1, 1, 1, 1};
            gpu.update(constants_, &frame, sizeof frame);
            gears_[i].draw();
        }
    }
    void release() override
    {
        glDeleteProgram(program_);
        for (DrawMesh &gear : gears_) gear.release();
        glDeleteBuffers(1, &constants_);
    }

private:
    GLuint   program_ = 0;
    DrawMesh gears_[3];
    GLuint   constants_ = 0;
};

// A mipmapped, anisotropically filtered texture on a receding quad.
class Texture final : public Sample {
public:
    const char *name() const override { return "texture"; }
    bool init(Gpu &gpu) override
    {
        program_ = gpu.program("texture", {.common = lit_frame,
                                           .vertex = R"(
layout(location = 0) in vec3 position;
layout(location = 2) in vec2 uv;
out vec2 v_uv;
void main() { gl_Position = (vec4(position, 1) * world) * view_projection; v_uv = uv; }
)",
                                           .fragment = R"(
layout(binding = 0) uniform sampler2D image;
in vec2 v_uv;
layout(location = 0) out vec4 target;
void main() { target = texture(image, v_uv); }
)"});
        Mesh mesh = plane_mesh(8.0f, 1, 4.0f);
        plane_ = gpu.upload(mesh);
        mesh.release();
        const uint32_t size = 256;
        uint32_t *texels = static_cast<uint32_t *>(allocate(size * size * 4));
        for (uint32_t y = 0; y < size; y++)
            for (uint32_t x = 0; x < size; x++) {
                const bool check = ((x / 32) + (y / 32)) % 2 == 0;
                const uint32_t r = check ? 230u : 40u, g = check ? 200u : 90u + (x % 32) * 4u, b = check ? 120u : 200u;
                texels[y * size + x] = 0xFF000000u | (b << 16u) | (g << 8u) | r;
            }
        texture_ = gpu.texture(texels, size, size);
        release_memory(texels);
        sampler_ = gpu.sampler();
        constants_ = gpu.uniform_buffer(sizeof(LitFrame));
        return !gpu.failed;
    }
    void render(Gpu &gpu, float time) override
    {
        gpu.begin_frame(0.1f, 0.1f, 0.1f);
        LitFrame frame = {};
        frame.world = rotate_y(time * 0.4f);
        frame.view_projection = look_at({0, 1.5f, -4}, {0, 0, 1}, {0, 1, 0}) * perspective(radians(60), gpu.aspect(), 0.1f, 100);
        gpu.update(constants_, &frame, sizeof frame);
        glUseProgram(program_);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, constants_);
        glBindTextureUnit(0, texture_);
        glBindSampler(0, sampler_);
        plane_.draw();
    }
    void release() override
    {
        glDeleteProgram(program_);
        plane_.release();
        glDeleteTextures(1, &texture_);
        glDeleteSamplers(1, &sampler_);
        glDeleteBuffers(1, &constants_);
    }

private:
    GLuint   program_ = 0;
    DrawMesh plane_;
    GLuint   texture_ = 0;
    GLuint   sampler_ = 0;
    GLuint   constants_ = 0;
};

}  // namespace

Sample *make_triangle() { return new Triangle(); }
Sample *make_pushconstants() { return new PushConstants(); }
Sample *make_gears() { return new Gears(); }
Sample *make_texture() { return new Texture(); }

}  // namespace samples
