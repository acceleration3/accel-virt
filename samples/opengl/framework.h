// The OpenGL samples' framework: what a sample is, what it is handed, and the
// few services the two platforms give it. Both builds compile the same GLSL
// with the driver in front of them - the host's own, or the guest's through
// rvkgl.dll - so a difference in the pictures is a difference in rendering.
//
// The D3D11 suite (samples/d3d11) is the model: the same samples, the same
// matrices and the same frames. OpenGL is told to take them unchanged:
// glClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE) puts clip-space +y at the first
// row of memory and depth in [0, 1], as Direct3D has them, so a texture
// rendered to is addressed with the same coordinates, and the frame reads back
// top row first.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "gl.h"
#include "../d3d11/smath.h"

namespace samples {

// Declared rather than a using-directive: the host's C library has its own
// sin, cos, abs... and a directive would make every call ambiguous.
using sm::pi; using sm::sqrt; using sm::abs; using sm::floor; using sm::fract; using sm::sin; using sm::cos;
using sm::tan; using sm::radians; using sm::min; using sm::max; using sm::clamp; using sm::mix; using sm::Random;
using sm::float2; using sm::float3; using sm::float4; using sm::float4x4; using sm::dot; using sm::cross;
using sm::length; using sm::normalize; using sm::translate; using sm::scale; using sm::rotate_x; using sm::rotate_y;
using sm::rotate_z; using sm::perspective; using sm::orthographic; using sm::look_at;

// ---- platform (guest_platform.cpp / host_platform.cpp)
struct Blob {
    void  *data = nullptr;
    size_t size = 0;
};
void   log(const char *text);
void   log_number(const char *label, double value);
void  *allocate(size_t bytes);   // zeroed
void   release_memory(void *memory);
double seconds();
bool   write_file(const char *path, const void *data, size_t size);
bool   read_file(const char *path, Blob &blob);
// A context made current on this thread: OpenGL 4.5 core or better, on the
// GPU `gpu` names (a substring of its renderer; null for the platform's pick).
bool   create_context(const char *gpu);
void  *gl_proc(const char *name);

// ---- text
void  copy_text(char *to, size_t capacity, const char *from);
void  append_text(char *to, size_t capacity, const char *from);
void  append_number(char *to, size_t capacity, double value, int decimals);
bool  same_text(const char *a, const char *b);
bool  contains_text(const char *text, const char *part);

// ---- meshes
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

// ---- GPU helpers
// A mesh's buffers and the vertex array that reads them: attributes 0-3 are
// position, normal, uv and colour, from binding 0.
struct DrawMesh {
    GLuint   vertex_array = 0;
    GLuint   vertices = 0;
    GLuint   indices = 0;
    uint32_t index_count = 0;
    void     release();
    void     draw(uint32_t instances = 1) const;
    // Three-control-point patches for the tessellator.
    void     draw_patches() const;
};

// The stages of one program: `common` goes before each, after the version.
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
    GLuint   target = 0;   // the framebuffer the runner reads back
    GLuint   color = 0;
    GLuint   depth = 0;

    GLuint   program(const char *name, const Stages &stages);
    GLuint   uniform_buffer(uint32_t bytes);
    GLuint   buffer(const void *data, uint32_t bytes);
    DrawMesh upload(const Mesh &mesh);
    // The vertex array every mesh is drawn through, for a sample that binds
    // its own buffers to it.
    GLuint   mesh_layout(GLuint vertices, GLuint indices);
    void     update(GLuint buffer, const void *data, uint32_t bytes);
    // A 2D texture (or array, or cube) from 32-bit RGBA texels, with a full
    // mip chain generated.
    GLuint   texture(const uint32_t *texels, uint32_t width, uint32_t height, uint32_t layers = 1, bool cube = false);
    GLuint   sampler(GLenum min_filter = GL_LINEAR_MIPMAP_LINEAR, GLenum mag_filter = GL_LINEAR,
                     GLenum wrap = GL_REPEAT, float anisotropy = 8.0f);
    // A texture of one level to render to.
    GLuint   render_texture(GLenum format, uint32_t width, uint32_t height, uint32_t layers = 1);
    // A framebuffer of up to four colour textures and a depth one.
    GLuint   framebuffer(const GLuint *colors, uint32_t count, GLuint depth_texture);
    // Binds the frame's target, clears it, sets a full viewport.
    void     begin_frame(float r, float g, float b);
    void     viewport(float x, float y, float w, float h);
    // Direct3D 11's defaults, which every sample starts from.
    void     reset_state();
    float    aspect() const { return static_cast<float>(width) / static_cast<float>(height); }
    bool     failed = false;
};

// ---- samples
class Sample {
public:
    virtual const char *name() const = 0;
    virtual bool        init(Gpu &gpu) = 0;
    // One frame at `time` seconds into the sample's own timeline.
    virtual void        render(Gpu &gpu, float time) = 0;
    virtual void        release() = 0;

protected:
    ~Sample() = default;
};

// Every sample, in the report's order.
Sample **registry(uint32_t &count);

// The entry points gl.h names, resolved through gl_proc: false and the first
// one missing when any is.
bool load_gl(const char *&missing);
// The runner (runner.cpp), given the program's arguments as narrow text.
int  run(int argc, const char *const *argv);

}  // namespace samples
