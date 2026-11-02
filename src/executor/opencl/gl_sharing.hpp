#ifndef AV_GL_SHARING_HPP
#define AV_GL_SHARING_HPP

#include <CL/cl_gl.h>

namespace av {

class GlSharing {
public:
    virtual cl_context make_context(const cl_context_properties *properties, cl_uint num_devices,
                                    const cl_device_id *devices, cl_int *errcode_ret) = 0;
    virtual cl_mem     from_buffer(cl_context context, cl_mem_flags flags, cl_GLuint buffer, cl_int *errcode_ret) = 0;
    virtual cl_mem     from_texture(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint level,
                                    cl_GLuint texture, cl_int *errcode_ret) = 0;
    virtual cl_mem     from_renderbuffer(cl_context context, cl_mem_flags flags, cl_GLuint renderbuffer,
                                         cl_int *errcode_ret) = 0;
    virtual cl_int     acquire(cl_command_queue queue, cl_uint count, const cl_mem *objects, cl_uint waits,
                               const cl_event *wait_list, cl_event *event) = 0;
    virtual cl_int     release(cl_command_queue queue, cl_uint count, const cl_mem *objects, cl_uint waits,
                               const cl_event *wait_list, cl_event *event) = 0;
    virtual cl_int     object_info(cl_mem object, cl_gl_object_type *type, cl_GLuint *name) = 0;
    virtual cl_int     texture_info(cl_mem object, cl_gl_texture_info param, size_t value_bytes, void *value,
                                    size_t *value_bytes_ret) = 0;
    virtual void       forget(cl_mem object) = 0;
    virtual void       forget_all() = 0;

protected:
    GlSharing() = default;
    ~GlSharing() = default;
    GlSharing(const GlSharing &) = delete;
    GlSharing &operator=(const GlSharing &) = delete;
};

}

#endif
