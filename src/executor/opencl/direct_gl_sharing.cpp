#include "executor/opencl/direct_gl_sharing.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/gl_context.hpp"
#include "executor/shim/gl_display.hpp"
#include "executor/shim/gl_renderbuffer_texture.hpp"

namespace av {

DirectGlSharing::DirectGlSharing(BlockSource &blocks, const GlContext &vehicle, GlRenderbufferTexture &textures)
    : vehicle_(vehicle), textures_(textures), display_(nullptr), renderbuffers_(blocks)
{
    AV_SCOPE("DirectGlSharing::DirectGlSharing");
}

void DirectGlSharing::configure(const GlDisplay &display)
{
    AV_SCOPE("DirectGlSharing::configure");
    display_ = &display;
}

cl_context DirectGlSharing::make_context(const cl_context_properties *properties, cl_uint num_devices,
                                         const cl_device_id *devices, cl_int *errcode_ret)
{
    AV_SCOPE("DirectGlSharing::make_context");
    cl_context_properties named[max_context_properties] = {};
    uint32_t              at = 0u;
    if (properties != nullptr) {
        while (properties[at] != 0 && at + 4u < max_context_properties) {
            named[at] = properties[at];
            named[at + 1u] = properties[at + 1u];
            at += 2u;
        }
        if (properties[at] != 0) {
            if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_PROPERTY;
            return nullptr;
        }
    }
    named[at] = CL_GL_CONTEXT_KHR;
    named[at + 1u] = reinterpret_cast<cl_context_properties>(vehicle_.handle());
    named[at + 2u] = CL_EGL_DISPLAY_KHR;
    named[at + 3u] = reinterpret_cast<cl_context_properties>(display_->handle());
    named[at + 4u] = 0;
    return ::clCreateContext(named, num_devices, devices, nullptr, nullptr, errcode_ret);
}

cl_mem DirectGlSharing::from_buffer(cl_context context, cl_mem_flags flags, cl_GLuint buffer, cl_int *errcode_ret)
{
    AV_SCOPE("DirectGlSharing::from_buffer");
    return ::clCreateFromGLBuffer(context, flags, buffer, errcode_ret);
}

cl_mem DirectGlSharing::from_texture(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint level,
                                     cl_GLuint texture, cl_int *errcode_ret)
{
    AV_SCOPE("DirectGlSharing::from_texture");
    return ::clCreateFromGLTexture(context, flags, target, level, texture, errcode_ret);
}

cl_mem DirectGlSharing::from_renderbuffer(cl_context context, cl_mem_flags flags, cl_GLuint renderbuffer,
                                          cl_int *errcode_ret)
{
    AV_SCOPE("DirectGlSharing::from_renderbuffer");
    cl_int       refused = CL_SUCCESS;
    cl_mem const direct = ::clCreateFromGLRenderbuffer(context, flags, renderbuffer, &refused);
    if (errcode_ret != nullptr) *errcode_ret = refused;
    if (direct != nullptr || refused != CL_INVALID_GL_OBJECT) return direct;
    return over_texture(context, flags, renderbuffer, errcode_ret);
}

cl_mem DirectGlSharing::over_texture(cl_context context, cl_mem_flags flags, cl_GLuint renderbuffer,
                                     cl_int *errcode_ret)
{
    AV_SCOPE("DirectGlSharing::over_texture");
    uint32_t texture = 0u;
    void    *image = nullptr;
    if (!textures_.make(renderbuffer, texture, image)) return nullptr;
    cl_int       made = CL_SUCCESS;
    cl_mem const over =
        ::clCreateFromGLTexture(context, flags, GlRenderbufferTexture::texture_2d, 0, texture, &made);
    const Status held = over != nullptr ? renderbuffers_.remember(over, renderbuffer, texture, image)
                                        : Status::invalid_parameter;
    if (held == Status::ok) {
        if (errcode_ret != nullptr) *errcode_ret = CL_SUCCESS;
        return over;
    }
    if (over != nullptr) static_cast<void>(::clReleaseMemObject(over));
    textures_.destroy(texture, image);
    if (errcode_ret != nullptr && over != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
    return nullptr;
}

cl_int DirectGlSharing::acquire(cl_command_queue queue, cl_uint count, const cl_mem *objects, cl_uint waits,
                                const cl_event *wait_list, cl_event *event)
{
    AV_SCOPE("DirectGlSharing::acquire");
    return ::clEnqueueAcquireGLObjects(queue, count, objects, waits, wait_list, event);
}

cl_int DirectGlSharing::release(cl_command_queue queue, cl_uint count, const cl_mem *objects, cl_uint waits,
                                const cl_event *wait_list, cl_event *event)
{
    AV_SCOPE("DirectGlSharing::release");
    return ::clEnqueueReleaseGLObjects(queue, count, objects, waits, wait_list, event);
}

cl_int DirectGlSharing::object_info(cl_mem object, cl_gl_object_type *type, cl_GLuint *name)
{
    AV_SCOPE("DirectGlSharing::object_info");
    const uint32_t renderbuffer = renderbuffers_.renderbuffer_of(object);
    if (renderbuffer == 0u) return ::clGetGLObjectInfo(object, type, name);
    if (type != nullptr) *type = CL_GL_OBJECT_RENDERBUFFER;
    if (name != nullptr) *name = renderbuffer;
    return CL_SUCCESS;
}

cl_int DirectGlSharing::texture_info(cl_mem object, cl_gl_texture_info param, size_t value_bytes, void *value,
                                     size_t *value_bytes_ret)
{
    AV_SCOPE("DirectGlSharing::texture_info");
    if (renderbuffers_.renderbuffer_of(object) != 0u) return CL_INVALID_GL_OBJECT;
    return ::clGetGLTextureInfo(object, param, value_bytes, value, value_bytes_ret);
}

void DirectGlSharing::forget(cl_mem object)
{
    AV_SCOPE("DirectGlSharing::forget");
    uint32_t texture = 0u;
    void    *image = nullptr;
    if (renderbuffers_.forget(object, texture, image)) textures_.destroy(texture, image);
}

void DirectGlSharing::forget_all()
{
    AV_SCOPE("DirectGlSharing::forget_all");
    uint32_t texture = 0u;
    void    *image = nullptr;
    while (renderbuffers_.forget_any(texture, image)) textures_.destroy(texture, image);
}

uint32_t DirectGlSharing::renderbuffer_images() const
{
    AV_SCOPE("DirectGlSharing::renderbuffer_images");
    return renderbuffers_.count();
}

}
