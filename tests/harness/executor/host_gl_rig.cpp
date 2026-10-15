#include "harness/executor/host_gl_rig.hpp"

#include "executor/opengl/gl_context_attributes.hpp"
#include "executor/shim/gl_display.hpp"
#include "executor/shim/opencl_device.hpp"

namespace av {

namespace {

using GLenum = std::uint32_t;
using GLuint = std::uint32_t;
using GLint = std::int32_t;
using GLsizei = std::int32_t;

constexpr GLenum array_buffer = 0x8892u;
constexpr GLenum static_draw = 0x88E4u;
constexpr GLenum texture_cube_map = 0x8513u;
constexpr GLenum cube_first_face = 0x8515u;
constexpr GLenum cube_last_face = 0x851Au;
constexpr GLenum texture_1d = 0x0DE0u;
constexpr GLenum texture_3d = 0x806Fu;
constexpr GLenum texture_2d_array = 0x8C1Au;
constexpr GLenum renderbuffer_target = 0x8D41u;
constexpr GLenum framebuffer_target = 0x8D40u;
constexpr GLenum color_attachment0 = 0x8CE0u;
constexpr GLenum color_buffer_bit = 0x4000u;
constexpr GLenum rgba = 0x1908u;
constexpr GLenum unsigned_byte = 0x1401u;
constexpr GLenum pack_alignment = 0x0D05u;
constexpr GLenum unpack_alignment = 0x0CF5u;

}  // namespace

HostGlRig::HostGlRig()
    : host_(), device_(nullptr), display_(nullptr), gl_(), context_(nullptr), queue_(nullptr)
{
    host_.open();
    for (std::uint32_t index = 0u; index < host_.gpu_count() && device_ == nullptr; index++) {
        const OpenClDevice *const device = host_.opencl_device(index);
        const GlDisplay *const    display = host_.gl_display(index);
        if (device == nullptr || display == nullptr || !display->open()) continue;
        const GlContextAttributes plain(nullptr);
        if (!gl_.open(*display, plain, nullptr)) continue;
        device_ = device;
        display_ = display;
    }
    if (device_ == nullptr) return;
    cl_device_id const          named = static_cast<cl_device_id>(device_->device());
    const cl_context_properties properties[] = {
        CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(device_->platform()), 0};
    cl_int made = CL_SUCCESS;
    context_ = ::clCreateContext(properties, 1u, &named, nullptr, nullptr, &made);
    if (context_ != nullptr) queue_ = ::clCreateCommandQueueWithProperties(context_, named, nullptr, &made);
    auto const pixel_store = reinterpret_cast<void (*)(GLenum, GLint)>(entry("glPixelStorei"));
    pixel_store(pack_alignment, 1);
    pixel_store(unpack_alignment, 1);
}

HostGlRig::~HostGlRig()
{
    if (queue_ != nullptr) ::clReleaseCommandQueue(queue_);
    if (context_ != nullptr) ::clReleaseContext(context_);
    gl_.close();
    host_.close();
}

bool                HostGlRig::ready() const { return queue_ != nullptr; }
const OpenClDevice &HostGlRig::device() const { return *device_; }
const GlDisplay    &HostGlRig::display() const { return *display_; }
cl_context          HostGlRig::context() const { return context_; }
cl_command_queue    HostGlRig::queue() const { return queue_; }

GlContext &HostGlRig::gl()
{
    static_cast<void>(gl_.make_current());
    return gl_;
}

void *HostGlRig::entry(const char *name)
{
    static_cast<void>(gl_.make_current());
    return gl_.resolve(name);
}

std::uint32_t HostGlRig::buffer(const void *bytes, std::uint64_t size)
{
    auto const gen = reinterpret_cast<void (*)(GLsizei, GLuint *)>(entry("glGenBuffers"));
    auto const bind = reinterpret_cast<void (*)(GLenum, GLuint)>(entry("glBindBuffer"));
    auto const data = reinterpret_cast<void (*)(GLenum, std::intptr_t, const void *, GLenum)>(entry("glBufferData"));
    GLuint name = 0u;
    gen(1, &name);
    bind(array_buffer, name);
    data(array_buffer, static_cast<std::intptr_t>(size), bytes, static_draw);
    bind(array_buffer, 0u);
    finish();
    return name;
}

void HostGlRig::read_buffer(std::uint32_t buffer, void *into, std::uint64_t size)
{
    auto const bind = reinterpret_cast<void (*)(GLenum, GLuint)>(entry("glBindBuffer"));
    auto const get =
        reinterpret_cast<void (*)(GLenum, std::intptr_t, std::intptr_t, void *)>(entry("glGetBufferSubData"));
    bind(array_buffer, buffer);
    get(array_buffer, 0, static_cast<std::intptr_t>(size), into);
    bind(array_buffer, 0u);
}

void HostGlRig::delete_buffer(std::uint32_t buffer)
{
    auto const remove = reinterpret_cast<void (*)(GLsizei, const GLuint *)>(entry("glDeleteBuffers"));
    remove(1, &buffer);
    finish();
}

std::uint32_t HostGlRig::texture(std::uint32_t target, std::int32_t level, std::uint32_t internal,
                                 std::uint32_t format, std::uint32_t type, std::uint32_t width, std::uint32_t height,
                                 std::uint32_t depth, const void *texels)
{
    auto const gen = reinterpret_cast<void (*)(GLsizei, GLuint *)>(entry("glGenTextures"));
    auto const bind = reinterpret_cast<void (*)(GLenum, GLuint)>(entry("glBindTexture"));
    auto const image1 = reinterpret_cast<void (*)(GLenum, GLint, GLint, GLsizei, GLint, GLenum, GLenum, const void *)>(
        entry("glTexImage1D"));
    auto const image2 =
        reinterpret_cast<void (*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *)>(
            entry("glTexImage2D"));
    auto const image3 = reinterpret_cast<void (*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum,
                                                  GLenum, const void *)>(entry("glTexImage3D"));
    const bool face = target >= cube_first_face && target <= cube_last_face;
    GLuint     name = 0u;
    gen(1, &name);
    bind(face ? texture_cube_map : target, name);
    const auto wide = static_cast<GLsizei>(width), high = static_cast<GLsizei>(height),
               deep = static_cast<GLsizei>(depth);
    const auto inside = static_cast<GLint>(internal);
    if (face) {
        for (GLenum each = cube_first_face; each <= cube_last_face; each++)
            image2(each, level, inside, wide, high, 0, format, type, each == target ? texels : nullptr);
    } else if (target == texture_1d) {
        image1(target, level, inside, wide, 0, format, type, texels);
    } else if (target == texture_3d || target == texture_2d_array) {
        image3(target, level, inside, wide, high, deep, 0, format, type, texels);
    } else {
        image2(target, level, inside, wide, high, 0, format, type, texels);
    }
    bind(face ? texture_cube_map : target, 0u);
    finish();
    return name;
}

void HostGlRig::read_texture(std::uint32_t target, std::uint32_t texture, std::int32_t level, std::uint32_t format,
                             std::uint32_t type, void *into)
{
    auto const bind = reinterpret_cast<void (*)(GLenum, GLuint)>(entry("glBindTexture"));
    auto const get = reinterpret_cast<void (*)(GLenum, GLint, GLenum, GLenum, void *)>(entry("glGetTexImage"));
    const bool face = target >= cube_first_face && target <= cube_last_face;
    bind(face ? texture_cube_map : target, texture);
    get(target, level, format, type, into);
    bind(face ? texture_cube_map : target, 0u);
}

void HostGlRig::delete_texture(std::uint32_t texture)
{
    auto const remove = reinterpret_cast<void (*)(GLsizei, const GLuint *)>(entry("glDeleteTextures"));
    remove(1, &texture);
    finish();
}

std::uint32_t HostGlRig::renderbuffer(std::uint32_t internal, std::uint32_t width, std::uint32_t height,
                                      const float colour[4])
{
    auto const gen = reinterpret_cast<void (*)(GLsizei, GLuint *)>(entry("glGenRenderbuffers"));
    auto const bind = reinterpret_cast<void (*)(GLenum, GLuint)>(entry("glBindRenderbuffer"));
    auto const storage =
        reinterpret_cast<void (*)(GLenum, GLenum, GLsizei, GLsizei)>(entry("glRenderbufferStorage"));
    auto const gen_framebuffers = reinterpret_cast<void (*)(GLsizei, GLuint *)>(entry("glGenFramebuffers"));
    auto const bind_framebuffer = reinterpret_cast<void (*)(GLenum, GLuint)>(entry("glBindFramebuffer"));
    auto const attach = reinterpret_cast<void (*)(GLenum, GLenum, GLenum, GLuint)>(entry("glFramebufferRenderbuffer"));
    auto const delete_framebuffers =
        reinterpret_cast<void (*)(GLsizei, const GLuint *)>(entry("glDeleteFramebuffers"));
    auto const viewport = reinterpret_cast<void (*)(GLint, GLint, GLsizei, GLsizei)>(entry("glViewport"));
    auto const clear_colour = reinterpret_cast<void (*)(float, float, float, float)>(entry("glClearColor"));
    auto const clear = reinterpret_cast<void (*)(GLenum)>(entry("glClear"));
    GLuint     name = 0u, framebuffer = 0u;
    gen(1, &name);
    bind(renderbuffer_target, name);
    storage(renderbuffer_target, internal, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    bind(renderbuffer_target, 0u);
    gen_framebuffers(1, &framebuffer);
    bind_framebuffer(framebuffer_target, framebuffer);
    attach(framebuffer_target, color_attachment0, renderbuffer_target, name);
    viewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    clear_colour(colour[0], colour[1], colour[2], colour[3]);
    clear(color_buffer_bit);
    bind_framebuffer(framebuffer_target, 0u);
    delete_framebuffers(1, &framebuffer);
    finish();
    return name;
}

void HostGlRig::read_renderbuffer(std::uint32_t renderbuffer, std::uint32_t width, std::uint32_t height, void *into)
{
    auto const gen_framebuffers = reinterpret_cast<void (*)(GLsizei, GLuint *)>(entry("glGenFramebuffers"));
    auto const bind_framebuffer = reinterpret_cast<void (*)(GLenum, GLuint)>(entry("glBindFramebuffer"));
    auto const attach = reinterpret_cast<void (*)(GLenum, GLenum, GLenum, GLuint)>(entry("glFramebufferRenderbuffer"));
    auto const delete_framebuffers =
        reinterpret_cast<void (*)(GLsizei, const GLuint *)>(entry("glDeleteFramebuffers"));
    auto const read = reinterpret_cast<void (*)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *)>(
        entry("glReadPixels"));
    GLuint framebuffer = 0u;
    gen_framebuffers(1, &framebuffer);
    bind_framebuffer(framebuffer_target, framebuffer);
    attach(framebuffer_target, color_attachment0, renderbuffer_target, renderbuffer);
    read(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), rgba, unsigned_byte, into);
    bind_framebuffer(framebuffer_target, 0u);
    delete_framebuffers(1, &framebuffer);
}

void HostGlRig::delete_renderbuffer(std::uint32_t renderbuffer)
{
    auto const remove = reinterpret_cast<void (*)(GLsizei, const GLuint *)>(entry("glDeleteRenderbuffers"));
    remove(1, &renderbuffer);
    finish();
}

void HostGlRig::finish()
{
    auto const done = reinterpret_cast<void (*)()>(entry("glFinish"));
    done();
}

}  // namespace av
