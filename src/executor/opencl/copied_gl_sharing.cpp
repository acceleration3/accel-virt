#include "executor/opencl/copied_gl_sharing.hpp"

#include "core/support/untestable.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/opencl/gl_cl_format.hpp"
#include "executor/opencl/gl_cl_target.hpp"
#include "executor/shim/gl_level.hpp"
#include "executor/shim/gl_object_bytes.hpp"
#include "executor/shim/gl_renderbuffer_texture.hpp"

namespace av {

namespace {

/* The one rule the three creates share: an access flag, alone. */
constexpr cl_mem_flags access_flags[] = {CL_MEM_READ_WRITE, CL_MEM_WRITE_ONLY, CL_MEM_READ_ONLY};
/* A renderbuffer's texture is 2D, and its storage level 0. */
constexpr uint32_t renderbuffer_dimensions = 2u;

}  // namespace

CopiedGlSharing::CopiedGlSharing(GlObjectBytes &objects, GlRenderbufferTexture &textures)
    : objects_(objects), textures_(textures), mirrors_(), staging_()
{
    AV_SCOPE("CopiedGlSharing::CopiedGlSharing");
}

cl_context CopiedGlSharing::make_context(const cl_context_properties *properties, cl_uint num_devices,
                                         const cl_device_id *devices, cl_int *errcode_ret)
{
    AV_SCOPE("CopiedGlSharing::make_context");
    return ::clCreateContext(properties, num_devices, devices, nullptr, nullptr, errcode_ret);
}

cl_mem CopiedGlSharing::from_buffer(cl_context context, cl_mem_flags flags, cl_GLuint buffer, cl_int *errcode_ret)
{
    AV_SCOPE("CopiedGlSharing::from_buffer");
    uint64_t bytes = 0u;
    cl_int   refused = CL_INVALID_VALUE;
    for (const cl_mem_flags access : access_flags)
        if (flags == access) refused = objects_.buffer_size(buffer, bytes) ? CL_SUCCESS : CL_INVALID_GL_OBJECT;
    cl_mem const made = refused == CL_SUCCESS
                            ? ::clCreateBuffer(context, flags, static_cast<size_t>(bytes), nullptr, &refused)
                            : nullptr;
    if (made == nullptr) {
        if (errcode_ret != nullptr) *errcode_ret = refused;
        return nullptr;
    }
    return held(made, GlMirror(made, buffer, bytes, flags != CL_MEM_READ_ONLY), errcode_ret);
}

cl_mem CopiedGlSharing::from_texture(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint level,
                                     cl_GLuint texture, cl_int *errcode_ret)
{
    AV_SCOPE("CopiedGlSharing::from_texture");
    const GlClTarget named(target);
    cl_int           refused = level < 0 ? CL_INVALID_MIP_LEVEL : CL_SUCCESS;
    if (!named.known()) refused = CL_INVALID_VALUE;
    if (refused != CL_SUCCESS) {
        if (errcode_ret != nullptr) *errcode_ret = refused;
        return nullptr;
    }
    const GlLevel place(texture, named.bind_target(), named.image_target(), named.dimensions(), level);
    return image_of(context, flags, named, place, named.object_type(), texture, nullptr, errcode_ret);
}

cl_mem CopiedGlSharing::from_renderbuffer(cl_context context, cl_mem_flags flags, cl_GLuint renderbuffer,
                                          cl_int *errcode_ret)
{
    AV_SCOPE("CopiedGlSharing::from_renderbuffer");
    uint32_t texture = 0u;
    void    *image = nullptr;
    if (!textures_.make(renderbuffer, texture, image)) {
        if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_GL_OBJECT;
        return nullptr;
    }
    const GlClTarget over(GlRenderbufferTexture::texture_2d);
    const GlLevel    place(texture, over.bind_target(), over.image_target(), renderbuffer_dimensions, 0);
    cl_mem const     made =
        image_of(context, flags, over, place, CL_GL_OBJECT_RENDERBUFFER, renderbuffer, image, errcode_ret);
    if (made == nullptr) textures_.destroy(texture, image);
    return made;
}

cl_mem CopiedGlSharing::image_of(cl_context context, cl_mem_flags flags, const GlClTarget &target, GlLevel level,
                                 uint32_t object_type, uint32_t name, void *image, cl_int *errcode_ret)
{
    AV_SCOPE("CopiedGlSharing::image_of");
    uint32_t internal = 0u, width = 0u, height = 0u, depth = 0u;
    cl_int   refused = CL_INVALID_VALUE;
    for (const cl_mem_flags access : access_flags)
        if (flags == access)
            refused = objects_.level_shape(level, internal, width, height, depth) ? CL_SUCCESS : CL_INVALID_GL_OBJECT;
    const GlClFormat format(internal);
    if (refused == CL_SUCCESS && !format.known()) refused = CL_INVALID_IMAGE_FORMAT_DESCRIPTOR;
    /* The host's GL measured the level, and a level it holds is memory it
     * holds: no product of its extent and a texel's size nears 2^64. */
    const uint64_t bytes = uint64_t{width} * height * depth * format.texel_bytes();
    cl_mem         made = nullptr;
    if (refused == CL_SUCCESS) {
        level.moves_as(format.transfer_format(), format.transfer_type(), width, height, depth);
        const cl_image_format named = {static_cast<cl_channel_order>(format.order()),
                                       static_cast<cl_channel_type>(format.type())};
        cl_image_desc         described = {};
        target.describe(width, height, depth, described);
        made = ::clCreateImage(context, flags, &named, &described, nullptr, &refused);
    }
    if (made == nullptr) {
        if (errcode_ret != nullptr) *errcode_ret = refused;
        return nullptr;
    }
    const uint32_t texture_target = object_type == CL_GL_OBJECT_RENDERBUFFER ? 0u : target.image_target();
    return held(made,
                GlMirror(made, object_type, name, texture_target, level, bytes, image, flags != CL_MEM_READ_ONLY),
                errcode_ret);
}

cl_mem CopiedGlSharing::held(cl_mem made, const GlMirror &mirror, cl_int *errcode_ret)
{
    AV_SCOPE("CopiedGlSharing::held");
    const bool remembered = mirrors_.remember(mirror);
    if (errcode_ret != nullptr) *errcode_ret = remembered ? CL_SUCCESS : CL_OUT_OF_RESOURCES;
    if (remembered) return made;
    static_cast<void>(::clReleaseMemObject(made));
    return nullptr;
}

cl_int CopiedGlSharing::judged(cl_uint count, const cl_mem *objects)
{
    AV_SCOPE("CopiedGlSharing::judged");
    if ((count == 0u) != (objects == nullptr)) return CL_INVALID_VALUE;
    uint64_t largest = 0u;
    for (cl_uint index = 0u; index < count; index++) {
        const GlMirror *const mirror = mirrors_.find(objects[index]);
        if (mirror == nullptr) return CL_INVALID_GL_OBJECT;
        if (mirror->bytes() > largest) largest = mirror->bytes();
    }
    /* Private memory as large as the largest, which every copy passes
     * through. */
    if (!staging_.reserve(static_cast<size_t>(largest))) { AV_UNTESTABLE("anonymous memory the size of a GL object the host's GPU holds"); return CL_OUT_OF_HOST_MEMORY; }
    return CL_SUCCESS;
}

cl_int CopiedGlSharing::acquire(cl_command_queue queue, cl_uint count, const cl_mem *objects, cl_uint waits,
                                const cl_event *wait_list, cl_event *event)
{
    AV_SCOPE("CopiedGlSharing::acquire");
    cl_int answer = judged(count, objects);
    /* The guest's wait list as a barrier, so the copies come after it on an
     * out-of-order queue as well. */
    if (answer == CL_SUCCESS) answer = ::clEnqueueBarrierWithWaitList(queue, waits, wait_list, nullptr);
    for (cl_uint index = 0u; index < count && answer == CL_SUCCESS; index++)
        answer = copy_in(queue, *mirrors_.find(objects[index]));
    if (answer != CL_SUCCESS || event == nullptr) return answer;
    return ::clEnqueueMarkerWithWaitList(queue, 0u, nullptr, event);
}

cl_int CopiedGlSharing::release(cl_command_queue queue, cl_uint count, const cl_mem *objects, cl_uint waits,
                                const cl_event *wait_list, cl_event *event)
{
    AV_SCOPE("CopiedGlSharing::release");
    cl_int answer = judged(count, objects);
    if (answer == CL_SUCCESS) answer = ::clEnqueueBarrierWithWaitList(queue, waits, wait_list, nullptr);
    bool written = false;
    for (cl_uint index = 0u; index < count && answer == CL_SUCCESS; index++) {
        GlMirror &mirror = *mirrors_.find(objects[index]);
        if (mirror.acquired() && mirror.written_back()) {
            answer = copy_out(queue, mirror);
            written = true;
        }
        mirror.release();
    }
    /* Finished whatever happened after: a copy that failed part-way has
     * still written what it wrote. */
    if (written) objects_.finish();
    if (answer != CL_SUCCESS || event == nullptr) return answer;
    return ::clEnqueueMarkerWithWaitList(queue, 0u, nullptr, event);
}

/* A GL object deleted since its cl_mem was made reads as nothing: the GL
 * object is what is refused, as the create would have been. */
cl_int CopiedGlSharing::copy_in(cl_command_queue queue, GlMirror &mirror)
{
    AV_SCOPE("CopiedGlSharing::copy_in");
    void *const    bytes = staging_.bytes();
    const GlLevel &level = mirror.level();
    cl_mem const   mem = static_cast<cl_mem>(const_cast<void *>(mirror.mem()));
    const bool     read = mirror.buffer() ? objects_.read_buffer(mirror.name(), mirror.bytes(), bytes)
                                          : objects_.read_level(level, bytes);
    if (!read) return CL_INVALID_GL_OBJECT;
    const size_t origin[3] = {0u, 0u, 0u};
    const size_t region[3] = {level.width(), level.height(), level.depth()};
    const cl_int written =
        mirror.buffer()
            ? ::clEnqueueWriteBuffer(queue, mem, CL_TRUE, 0u, static_cast<size_t>(mirror.bytes()), bytes, 0u, nullptr,
                                     nullptr)
            : ::clEnqueueWriteImage(queue, mem, CL_TRUE, origin, region, 0u, 0u, bytes, 0u, nullptr, nullptr);
    if (written == CL_SUCCESS) mirror.acquire();
    return written;
}

cl_int CopiedGlSharing::copy_out(cl_command_queue queue, GlMirror &mirror)
{
    AV_SCOPE("CopiedGlSharing::copy_out");
    void *const    bytes = staging_.bytes();
    const GlLevel &level = mirror.level();
    cl_mem const   mem = static_cast<cl_mem>(const_cast<void *>(mirror.mem()));
    const size_t   origin[3] = {0u, 0u, 0u};
    const size_t   region[3] = {level.width(), level.height(), level.depth()};
    const cl_int   read =
        mirror.buffer()
              ? ::clEnqueueReadBuffer(queue, mem, CL_TRUE, 0u, static_cast<size_t>(mirror.bytes()), bytes, 0u, nullptr,
                                      nullptr)
              : ::clEnqueueReadImage(queue, mem, CL_TRUE, origin, region, 0u, 0u, bytes, 0u, nullptr, nullptr);
    if (read != CL_SUCCESS) return read;
    const bool written = mirror.buffer() ? objects_.write_buffer(mirror.name(), mirror.bytes(), bytes)
                                         : objects_.write_level(level, bytes);
    return written ? CL_SUCCESS : CL_INVALID_GL_OBJECT;
}

cl_int CopiedGlSharing::object_info(cl_mem object, cl_gl_object_type *type, cl_GLuint *name)
{
    AV_SCOPE("CopiedGlSharing::object_info");
    const GlMirror *const mirror = mirrors_.find(object);
    if (mirror == nullptr) return CL_INVALID_GL_OBJECT;
    if (type != nullptr) *type = mirror->object_type();
    if (name != nullptr) *name = mirror->name();
    return CL_SUCCESS;
}

cl_int CopiedGlSharing::texture_info(cl_mem object, cl_gl_texture_info param, size_t value_bytes, void *value,
                                     size_t *value_bytes_ret)
{
    AV_SCOPE("CopiedGlSharing::texture_info");
    const GlMirror *const mirror = mirrors_.find(object);
    if (mirror == nullptr || !mirror->texture()) return CL_INVALID_GL_OBJECT;
    const cl_GLenum target = mirror->target();
    const cl_GLint  level = mirror->level().level();
    const void     *answer = nullptr;
    size_t          bytes = 0u;
    if (param == CL_GL_TEXTURE_TARGET) {
        answer = &target;
        bytes = sizeof target;
    }
    if (param == CL_GL_MIPMAP_LEVEL) {
        answer = &level;
        bytes = sizeof level;
    }
    if (answer == nullptr || (value != nullptr && value_bytes < bytes)) return CL_INVALID_VALUE;
    if (value != nullptr) __builtin_memcpy(value, answer, bytes);
    if (value_bytes_ret != nullptr) *value_bytes_ret = bytes;
    return CL_SUCCESS;
}

void CopiedGlSharing::forget(cl_mem object)
{
    AV_SCOPE("CopiedGlSharing::forget");
    GlMirror gone;
    if (mirrors_.forget(object, gone) && gone.image() != nullptr)
        textures_.destroy(gone.level().texture(), gone.image());
}

void CopiedGlSharing::forget_all()
{
    AV_SCOPE("CopiedGlSharing::forget_all");
    GlMirror gone;
    while (mirrors_.forget_any(gone))
        if (gone.image() != nullptr) textures_.destroy(gone.level().texture(), gone.image());
}

uint32_t CopiedGlSharing::mirrors() const
{
    AV_SCOPE("CopiedGlSharing::mirrors");
    return mirrors_.count();
}

}  // namespace av
