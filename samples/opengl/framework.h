#pragma once

#include <stddef.h>
#include <stdint.h>

#include "gl.h"
#include "../d3d11/smath.h"

namespace samples {
using sm::pi; using sm::sqrt; using sm::abs; using sm::floor; using sm::fract; using sm::sin; using sm::cos;
using sm::tan; using sm::radians; using sm::min; using sm::max; using sm::clamp; using sm::mix; using sm::Random;
using sm::float2; using sm::float3; using sm::float4; using sm::float4x4; using sm::dot; using sm::cross;
using sm::length; using sm::normalize; using sm::translate; using sm::scale; using sm::rotate_x; using sm::rotate_y;
using sm::rotate_z; using sm::perspective; using sm::orthographic; using sm::look_at;

struct Blob {
    void  *data = nullptr;
    size_t size = 0;
};
void   log(const char *text);
void   log_number(const char *label, double value);
void  *allocate(size_t bytes);
void   release_memory(void *memory);
double seconds();
bool   write_file(const char *path, const void *data, size_t size);
bool   read_file(const char *path, Blob &blob);
bool   create_context(const char *gpu);
void  *gl_proc(const char *name);

void  copy_text(char *to, size_t capacity, const char *from);
void  append_text(char *to, size_t capacity, const char *from);
void  append_number(char *to, size_t capacity, double value, int decimals);
bool  same_text(const char *a, const char *b);
bool  contains_text(const char *text, const char *part);

struct Vertex {
    float3 position;
    float3 normal;
    float2 uv;
    float3 color;
};
struct Mesh {
    Vertex   *vertices = nullptr;
    uint32_t *indices = nullptr;
    uint32_t  vertex_count = 0;
    uint32_t  index_count = 0;
    void      release();
};
Mesh cube_mesh(float size);
Mesh sphere_mesh(float radius, uint32_t slices, uint32_t stacks);
Mesh torus_mesh(float radius, float tube, uint32_t rings, uint32_t sides);
Mesh plane_mesh(float size, uint32_t cells, float uv_scale);
Mesh gear_mesh(float inner, float outer, float width, uint32_t teeth, float tooth_depth, float3 color);

struct DrawMesh {
    GLuint   vertex_array = 0;
    GLuint   vertices = 0;
    GLuint   indices = 0;
    uint32_t index_count = 0;
    void     release();
    void     draw(uint32_t instances = 1) const;
    void     draw_patches() const;
};

struct Stages {
    const char *common = nullptr;
    const char *vertex = nullptr;
    const char *fragment = nullptr;
    const char *geometry = nullptr;
    const char *control = nullptr;
    const char *evaluation = nullptr;
    const char *compute = nullptr;
};

class Gpu {
public:
    uint32_t width = 0;
    uint32_t height = 0;
    GLuint   target = 0;
    GLuint   color = 0;
    GLuint   depth = 0;

    GLuint   program(const char *name, const Stages &stages);
    GLuint   uniform_buffer(uint32_t bytes);
    GLuint   buffer(const void *data, uint32_t bytes);
    DrawMesh upload(const Mesh &mesh);
    GLuint   mesh_layout(GLuint vertices, GLuint indices);
    void     update(GLuint buffer, const void *data, uint32_t bytes);
    GLuint   texture(const uint32_t *texels, uint32_t width, uint32_t height, uint32_t layers = 1, bool cube = false);
    GLuint   sampler(GLenum min_filter = GL_LINEAR_MIPMAP_LINEAR, GLenum mag_filter = GL_LINEAR,
                     GLenum wrap = GL_REPEAT, float anisotropy = 8.0f);
    GLuint   render_texture(GLenum format, uint32_t width, uint32_t height, uint32_t layers = 1);
    GLuint   framebuffer(const GLuint *colors, uint32_t count, GLuint depth_texture);
    void     draw_fullscreen();
    void     draw_generated(GLenum mode, uint32_t vertices, uint32_t instances = 1);
    void     begin_frame(float r, float g, float b);
    void     viewport(float x, float y, float w, float h);
    void     reset_state();
    float    aspect() const { return static_cast<float>(width) / static_cast<float>(height); }
    bool     failed = false;

private:
    GLuint   empty_layout_ = 0;
};

class Sample {
public:
    virtual const char *name() const = 0;
    virtual bool        init(Gpu &gpu) = 0;
    virtual void        render(Gpu &gpu, float time) = 0;
    virtual void        release() = 0;

protected:
    ~Sample() = default;
};

Sample **registry(uint32_t &count);

bool load_gl(const char *&missing);
int  run(int argc, const char *const *argv);

}
