#include "framework.h"

namespace samples {

// ---------------------------------------------------------------- text

void copy_text(char *to, size_t capacity, const char *from)
{
    size_t i = 0;
    for (; i + 1 < capacity && from[i] != '\0'; i++) to[i] = from[i];
    if (capacity != 0) to[i] = '\0';
}

void append_text(char *to, size_t capacity, const char *from)
{
    size_t at = 0;
    while (at < capacity && to[at] != '\0') at++;
    if (at < capacity) copy_text(to + at, capacity - at, from);
}

void append_number(char *to, size_t capacity, double value, int decimals)
{
    char digits[48] = {};
    int  n = 0;
    if (value < 0) {
        digits[n++] = '-';
        value = -value;
    }
    double scale = 1.0;
    for (int d = 0; d < decimals; d++) scale *= 10.0;
    unsigned long long whole = static_cast<unsigned long long>(value * scale + 0.5);
    char reversed[32] = {};
    int  r = 0;
    for (int d = 0; d < decimals; d++) {
        reversed[r++] = static_cast<char>('0' + whole % 10u);
        whole /= 10u;
    }
    if (decimals > 0) reversed[r++] = '.';
    do {
        reversed[r++] = static_cast<char>('0' + whole % 10u);
        whole /= 10u;
    } while (whole != 0u && r < 30);
    while (r > 0) digits[n++] = reversed[--r];
    append_text(to, capacity, digits);
}

bool same_text(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

bool contains_text(const char *text, const char *part)
{
    for (; *text != '\0'; text++) {
        size_t i = 0;
        while (part[i] != '\0' && text[i] == part[i]) i++;
        if (part[i] == '\0') return true;
    }
    return *part == '\0';
}

// ---------------------------------------------------------------- meshes

void Mesh::release()
{
    release_memory(vertices);
    release_memory(indices);
    vertices = nullptr;
    indices = nullptr;
}

namespace {

Mesh allocate_mesh(uint32_t vertices, uint32_t indices)
{
    Mesh mesh;
    mesh.vertices = static_cast<Vertex *>(allocate(sizeof(Vertex) * vertices));
    mesh.indices = static_cast<uint32_t *>(allocate(sizeof(uint32_t) * indices));
    return mesh;
}

void push_vertex(Mesh &mesh, float3 p, float3 n, float2 uv, float3 color)
{
    mesh.vertices[mesh.vertex_count++] = Vertex{p, n, uv, color};
}

void push_quad(Mesh &mesh, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    // Front faces are clockwise as seen: every generator winds its quads to
    // face the way its normals point, whatever order it listed them in.
    const Vertex *v = mesh.vertices;
    const float3 facing = cross(v[b].position - v[a].position, v[c].position - v[a].position);
    if (dot(facing, v[a].normal + v[b].normal + v[c].normal + v[d].normal) < 0) {
        const uint32_t swap = b;
        b = d;
        d = swap;
    }
    uint32_t *i = mesh.indices + mesh.index_count;
    i[0] = a; i[1] = b; i[2] = c;
    i[3] = a; i[4] = c; i[5] = d;
    mesh.index_count += 6;
}

}  // namespace

Mesh cube_mesh(float size)
{
    Mesh mesh = allocate_mesh(24, 36);
    const float h = size * 0.5f;
    const float3 normals[6] = {{0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
    const float3 colors[6] = {{1, 0.3f, 0.3f}, {0.3f, 1, 0.3f}, {0.3f, 0.3f, 1}, {1, 1, 0.3f}, {0.3f, 1, 1}, {1, 0.3f, 1}};
    for (int f = 0; f < 6; f++) {
        const float3 n = normals[f];
        const float3 u = abs(n.y) > 0.5f ? float3{1, 0, 0} : normalize(cross({0, 1, 0}, n));
        const float3 v = cross(n, u);
        const uint32_t base = mesh.vertex_count;
        const float2 uvs[4] = {{0, 1}, {0, 0}, {1, 0}, {1, 1}};
        const float su[4] = {-1, -1, 1, 1}, sv[4] = {-1, 1, 1, -1};
        for (int k = 0; k < 4; k++)
            push_vertex(mesh, (n + u * su[k] + v * sv[k]) * h, n, uvs[k], colors[f]);
        push_quad(mesh, base, base + 1, base + 2, base + 3);
    }
    return mesh;
}

Mesh sphere_mesh(float radius, uint32_t slices, uint32_t stacks)
{
    Mesh mesh = allocate_mesh((slices + 1) * (stacks + 1), slices * stacks * 6);
    for (uint32_t j = 0; j <= stacks; j++) {
        const float v = static_cast<float>(j) / static_cast<float>(stacks);
        const float phi = v * pi;
        for (uint32_t i = 0; i <= slices; i++) {
            const float u = static_cast<float>(i) / static_cast<float>(slices);
            const float theta = u * 2.0f * pi;
            const float3 n = {sin(phi) * cos(theta), cos(phi), sin(phi) * sin(theta)};
            push_vertex(mesh, n * radius, n, {u, v}, {0.5f + 0.5f * n.x, 0.5f + 0.5f * n.y, 0.5f + 0.5f * n.z});
        }
    }
    for (uint32_t j = 0; j < stacks; j++)
        for (uint32_t i = 0; i < slices; i++) {
            const uint32_t a = j * (slices + 1) + i, b = a + slices + 1;
            push_quad(mesh, a, a + 1, b + 1, b);
        }
    return mesh;
}

Mesh torus_mesh(float radius, float tube, uint32_t rings, uint32_t sides)
{
    Mesh mesh = allocate_mesh((rings + 1) * (sides + 1), rings * sides * 6);
    for (uint32_t r = 0; r <= rings; r++) {
        const float u = static_cast<float>(r) / static_cast<float>(rings), a = u * 2.0f * pi;
        const float3 center = {cos(a) * radius, 0, sin(a) * radius};
        for (uint32_t s = 0; s <= sides; s++) {
            const float v = static_cast<float>(s) / static_cast<float>(sides), b = v * 2.0f * pi;
            const float3 n = {cos(a) * cos(b), sin(b), sin(a) * cos(b)};
            push_vertex(mesh, center + n * tube, n, {u * 4.0f, v}, {0.8f, 0.6f + 0.4f * v, 0.3f});
        }
    }
    for (uint32_t r = 0; r < rings; r++)
        for (uint32_t s = 0; s < sides; s++) {
            const uint32_t a = r * (sides + 1) + s, b = a + sides + 1;
            push_quad(mesh, a, b, b + 1, a + 1);
        }
    return mesh;
}

Mesh plane_mesh(float size, uint32_t cells, float uv_scale)
{
    Mesh mesh = allocate_mesh((cells + 1) * (cells + 1), cells * cells * 6);
    for (uint32_t j = 0; j <= cells; j++)
        for (uint32_t i = 0; i <= cells; i++) {
            const float u = static_cast<float>(i) / static_cast<float>(cells);
            const float v = static_cast<float>(j) / static_cast<float>(cells);
            push_vertex(mesh, {(u - 0.5f) * size, 0, (v - 0.5f) * size}, {0, 1, 0}, {u * uv_scale, v * uv_scale},
                        {0.7f, 0.7f, 0.7f});
        }
    for (uint32_t j = 0; j < cells; j++)
        for (uint32_t i = 0; i < cells; i++) {
            const uint32_t a = j * (cells + 1) + i, b = a + cells + 1;
            push_quad(mesh, a, b, b + 1, a + 1);
        }
    return mesh;
}

// The gears sample's gear: a toothed ring with front, back, outer and inner
// faces, flat shaded, as the old OpenGL gears demo built it.
Mesh gear_mesh(float inner, float outer, float width, uint32_t teeth, float tooth_depth, float3 color)
{
    Mesh mesh = allocate_mesh(teeth * 40, teeth * 60);
    const float r0 = inner, r1 = outer - tooth_depth * 0.5f, r2 = outer + tooth_depth * 0.5f;
    const float da = 2.0f * pi / static_cast<float>(teeth) / 4.0f;
    const float hw = width * 0.5f;
    auto quad = [&](float3 a, float3 b, float3 c, float3 d, float3 n) {
        // Front faces are clockwise as seen: wind every quad to face along its normal.
        if (dot(cross(b - a, c - a), n) < 0) {
            const float3 swap = b;
            b = d;
            d = swap;
        }
        const uint32_t base = mesh.vertex_count;
        push_vertex(mesh, a, n, {0, 0}, color);
        push_vertex(mesh, b, n, {1, 0}, color);
        push_vertex(mesh, c, n, {1, 1}, color);
        push_vertex(mesh, d, n, {0, 1}, color);
        push_quad(mesh, base, base + 1, base + 2, base + 3);
    };
    for (uint32_t i = 0; i < teeth; i++) {
        const float a = static_cast<float>(i) * 2.0f * pi / static_cast<float>(teeth);
        const float c0 = cos(a), s0 = sin(a), c1 = cos(a + da), s1 = sin(a + da);
        const float c2 = cos(a + 2 * da), s2 = sin(a + 2 * da), c3 = cos(a + 3 * da), s3 = sin(a + 3 * da);
        const float c4 = cos(a + 4 * da), s4 = sin(a + 4 * da);
        for (int side = 0; side < 2; side++) {
            const float z = side == 0 ? hw : -hw;
            const float3 n = {0, 0, side == 0 ? 1.0f : -1.0f};
            // ring face between the teeth's root and the hole
            quad({r0 * c0, r0 * s0, z}, {r1 * c0, r1 * s0, z}, {r1 * c4, r1 * s4, z}, {r0 * c4, r0 * s4, z}, n);
            // the tooth's face
            quad({r1 * c0, r1 * s0, z}, {r2 * c1, r2 * s1, z}, {r2 * c2, r2 * s2, z}, {r1 * c3, r1 * s3, z}, n);
        }
        // outward faces: rising flank, top, falling flank, root
        const float3 p[5] = {{r1 * c0, r1 * s0, 0}, {r2 * c1, r2 * s1, 0}, {r2 * c2, r2 * s2, 0},
                             {r1 * c3, r1 * s3, 0}, {r1 * c4, r1 * s4, 0}};
        for (int k = 0; k < 4; k++) {
            const float3 e = p[k + 1] - p[k];
            const float3 n = normalize({e.y, -e.x, 0});
            quad({p[k].x, p[k].y, hw}, {p[k].x, p[k].y, -hw}, {p[k + 1].x, p[k + 1].y, -hw},
                 {p[k + 1].x, p[k + 1].y, hw}, n);
        }
        // the hole
        quad({r0 * c4, r0 * s4, hw}, {r0 * c4, r0 * s4, -hw}, {r0 * c0, r0 * s0, -hw}, {r0 * c0, r0 * s0, hw},
             {-c0, -s0, 0});
    }
    return mesh;
}


// ---------------------------------------------------------------- GPU helpers

#define SAMPLES_GL_DEFINE(returned, name, params) returned(SAMPLES_GL_API *name) params = nullptr;
SAMPLES_GL_COMMANDS(SAMPLES_GL_DEFINE)
#undef SAMPLES_GL_DEFINE

bool load_gl(const char *&missing)
{
#define SAMPLES_GL_LOAD(returned, name, params)                                   \
    name = reinterpret_cast<returned(SAMPLES_GL_API *) params>(gl_proc(#name)); \
    if (name == nullptr) {                                                        \
        missing = #name;                                                          \
        return false;                                                             \
    }
    SAMPLES_GL_COMMANDS(SAMPLES_GL_LOAD)
#undef SAMPLES_GL_LOAD
    return true;
}

namespace {

const char *const version_line = "#version 450 core\n";

GLuint compile(const char *name, GLenum stage, const char *common, const char *body, bool &failed)
{
    const GLuint shader = glCreateShader(stage);
    const char *parts[3] = {version_line, common != nullptr ? common : "", body};
    glShaderSource(shader, 3, parts, nullptr);
    glCompileShader(shader);
    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == 0) {
        char text[4096] = {};
        glGetShaderInfoLog(shader, sizeof text, nullptr, text);
        log(name);
        log(": ");
        log(text);
        log("\n");
        failed = true;
    }
    return shader;
}

}  // namespace

void DrawMesh::release()
{
    glDeleteVertexArrays(1, &vertex_array);
    glDeleteBuffers(1, &vertices);
    glDeleteBuffers(1, &indices);
    vertex_array = vertices = indices = 0;
}

void DrawMesh::draw(uint32_t instances) const
{
    glBindVertexArray(vertex_array);
    if (instances == 1) glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(index_count), GL_UNSIGNED_INT, nullptr);
    else glDrawElementsInstanced(GL_TRIANGLES, static_cast<GLsizei>(index_count), GL_UNSIGNED_INT, nullptr,
                                 static_cast<GLsizei>(instances));
}

void DrawMesh::draw_patches() const
{
    glBindVertexArray(vertex_array);
    glPatchParameteri(GL_PATCH_VERTICES, 3);
    glDrawElements(GL_PATCHES, static_cast<GLsizei>(index_count), GL_UNSIGNED_INT, nullptr);
}

GLuint Gpu::program(const char *name, const Stages &stages)
{
    struct Part {
        GLenum      stage;
        const char *body;
    };
    const Part parts[6] = {{GL_VERTEX_SHADER, stages.vertex},         {GL_TESS_CONTROL_SHADER, stages.control},
                           {GL_TESS_EVALUATION_SHADER, stages.evaluation}, {GL_GEOMETRY_SHADER, stages.geometry},
                           {GL_FRAGMENT_SHADER, stages.fragment},     {GL_COMPUTE_SHADER, stages.compute}};
    const GLuint made = glCreateProgram();
    GLuint shaders[6] = {};
    uint32_t count = 0;
    for (const Part &part : parts) {
        if (part.body == nullptr) continue;
        shaders[count] = compile(name, part.stage, stages.common, part.body, failed);
        glAttachShader(made, shaders[count++]);
    }
    glLinkProgram(made);
    for (uint32_t i = 0; i < count; i++) {
        glDetachShader(made, shaders[i]);
        glDeleteShader(shaders[i]);
    }
    GLint linked = 0;
    glGetProgramiv(made, GL_LINK_STATUS, &linked);
    if (linked == 0) {
        char text[4096] = {};
        glGetProgramInfoLog(made, sizeof text, nullptr, text);
        log(name);
        log(" (link): ");
        log(text);
        log("\n");
        failed = true;
    }
    return made;
}

GLuint Gpu::uniform_buffer(uint32_t bytes)
{
    GLuint made = 0;
    glCreateBuffers(1, &made);
    glNamedBufferData(made, (bytes + 15u) & ~15u, nullptr, GL_DYNAMIC_DRAW);
    return made;
}

GLuint Gpu::buffer(const void *data, uint32_t bytes)
{
    GLuint made = 0;
    glCreateBuffers(1, &made);
    glNamedBufferData(made, bytes, data, GL_STATIC_DRAW);
    return made;
}

GLuint Gpu::mesh_layout(GLuint vertex_buffer, GLuint index_buffer)
{
    GLuint made = 0;
    glCreateVertexArrays(1, &made);
    const GLuint offsets[4] = {0, 12, 24, 32};
    const GLint sizes[4] = {3, 3, 2, 3};
    for (GLuint a = 0; a < 4; a++) {
        glEnableVertexArrayAttrib(made, a);
        glVertexArrayAttribFormat(made, a, sizes[a], GL_FLOAT, GL_FALSE, offsets[a]);
        glVertexArrayAttribBinding(made, a, 0);
    }
    glVertexArrayVertexBuffer(made, 0, vertex_buffer, 0, sizeof(Vertex));
    if (index_buffer != 0) glVertexArrayElementBuffer(made, index_buffer);
    return made;
}

DrawMesh Gpu::upload(const Mesh &mesh)
{
    DrawMesh drawn;
    drawn.vertices = buffer(mesh.vertices, mesh.vertex_count * static_cast<uint32_t>(sizeof(Vertex)));
    drawn.indices = buffer(mesh.indices, mesh.index_count * 4u);
    drawn.index_count = mesh.index_count;
    drawn.vertex_array = mesh_layout(drawn.vertices, drawn.indices);
    return drawn;
}

void Gpu::update(GLuint target_buffer, const void *data, uint32_t bytes)
{
    glNamedBufferSubData(target_buffer, 0, bytes, data);
}

GLuint Gpu::texture(const uint32_t *texels, uint32_t w, uint32_t h, uint32_t layers, bool cube)
{
    GLsizei mips = 1;
    while ((w >> mips) > 0 || (h >> mips) > 0) mips++;
    const GLenum kind = cube ? (layers > 6 ? GL_TEXTURE_CUBE_MAP_ARRAY : GL_TEXTURE_CUBE_MAP)
                             : (layers > 1 ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D);
    GLuint made = 0;
    glCreateTextures(kind, 1, &made);
    if (kind == GL_TEXTURE_2D || kind == GL_TEXTURE_CUBE_MAP) {
        glTextureStorage2D(made, mips, GL_RGBA8, static_cast<GLsizei>(w), static_cast<GLsizei>(h));
    } else {
        glTextureStorage3D(made, mips, GL_RGBA8, static_cast<GLsizei>(w), static_cast<GLsizei>(h),
                           static_cast<GLsizei>(layers));
    }
    if (kind == GL_TEXTURE_2D)
        glTextureSubImage2D(made, 0, 0, 0, static_cast<GLsizei>(w), static_cast<GLsizei>(h), GL_RGBA,
                            GL_UNSIGNED_BYTE, texels);
    else
        glTextureSubImage3D(made, 0, 0, 0, 0, static_cast<GLsizei>(w), static_cast<GLsizei>(h),
                            static_cast<GLsizei>(layers), GL_RGBA, GL_UNSIGNED_BYTE, texels);
    glGenerateTextureMipmap(made);
    return made;
}

GLuint Gpu::sampler(GLenum min_filter, GLenum mag_filter, GLenum wrap, float anisotropy)
{
    GLuint made = 0;
    glCreateSamplers(1, &made);
    glSamplerParameteri(made, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(min_filter));
    glSamplerParameteri(made, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(mag_filter));
    glSamplerParameteri(made, GL_TEXTURE_WRAP_S, static_cast<GLint>(wrap));
    glSamplerParameteri(made, GL_TEXTURE_WRAP_T, static_cast<GLint>(wrap));
    glSamplerParameteri(made, GL_TEXTURE_WRAP_R, static_cast<GLint>(wrap));
    if (anisotropy > 1.0f) glSamplerParameterf(made, GL_TEXTURE_MAX_ANISOTROPY, anisotropy);
    return made;
}

GLuint Gpu::render_texture(GLenum format, uint32_t w, uint32_t h, uint32_t layers)
{
    GLuint made = 0;
    glCreateTextures(layers > 1 ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D, 1, &made);
    if (layers > 1)
        glTextureStorage3D(made, 1, format, static_cast<GLsizei>(w), static_cast<GLsizei>(h),
                           static_cast<GLsizei>(layers));
    else
        glTextureStorage2D(made, 1, format, static_cast<GLsizei>(w), static_cast<GLsizei>(h));
    return made;
}

GLuint Gpu::framebuffer(const GLuint *colors, uint32_t count, GLuint depth_texture)
{
    GLuint made = 0;
    glCreateFramebuffers(1, &made);
    GLenum draw[4] = {};
    for (uint32_t i = 0; i < count && i < 4; i++) {
        glNamedFramebufferTexture(made, GL_COLOR_ATTACHMENT0 + i, colors[i], 0);
        draw[i] = GL_COLOR_ATTACHMENT0 + i;
    }
    if (count == 0) glNamedFramebufferDrawBuffer(made, GL_NONE);
    else glNamedFramebufferDrawBuffers(made, static_cast<GLsizei>(count), draw);
    if (depth_texture != 0) glNamedFramebufferTexture(made, GL_DEPTH_ATTACHMENT, depth_texture, 0);
    if (glCheckNamedFramebufferStatus(made, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) failed = true;
    return made;
}

void Gpu::draw_fullscreen()
{
    if (empty_layout_ == 0) glCreateVertexArrays(1, &empty_layout_);
    glBindVertexArray(empty_layout_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void Gpu::begin_frame(float r, float g, float b)
{
    // A Direct3D clear ignores the masks and the scissor; glClear does not.
    glBindFramebuffer(GL_FRAMEBUFFER, target);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glStencilMask(0xFFu);
    const float clear[4] = {r, g, b, 1.0f};
    glClearBufferfv(GL_COLOR, 0, clear);
    glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);
    viewport(0, 0, static_cast<float>(width), static_cast<float>(height));
}

void Gpu::viewport(float x, float y, float w, float h)
{
    glViewportIndexedf(0, x, y, w, h);
}

void Gpu::reset_state()
{
    glUseProgram(0);
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, target);
    glClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CW);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_POLYGON_OFFSET_LINE);
    glDisable(GL_DEPTH_CLAMP);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_RASTERIZER_DISCARD);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ZERO);
}

}  // namespace samples
