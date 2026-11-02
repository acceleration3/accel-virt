#ifndef AV_DIRECT_GL_SHARING_HPP
#define AV_DIRECT_GL_SHARING_HPP

#include "executor/opencl/cl_renderbuffer_images.hpp"
#include "executor/opencl/gl_sharing.hpp"

#include <stdint.h>

namespace av {

class GlContext;
class GlDisplay;
class GlRenderbufferTexture;
struct DirectGlSharingProbe;

class DirectGlSharing final : public GlSharing {
public:
    DirectGlSharing(BlockSource &blocks, const GlContext &vehicle, GlRenderbufferTexture &textures);

    void configure(const GlDisplay &display);

    cl_context make_context(const cl_context_properties *properties, cl_uint num_devices,
                            const cl_device_id *devices, cl_int *errcode_ret) override;
    cl_mem     from_buffer(cl_context context, cl_mem_flags flags, cl_GLuint buffer, cl_int *errcode_ret) override;
    cl_mem     from_texture(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint level,
                            cl_GLuint texture, cl_int *errcode_ret) override;
    cl_mem     from_renderbuffer(cl_context context, cl_mem_flags flags, cl_GLuint renderbuffer,
                                 cl_int *errcode_ret) override;
    cl_int     acquire(cl_command_queue queue, cl_uint count, const cl_mem *objects, cl_uint waits,
                       const cl_event *wait_list, cl_event *event) override;
    cl_int     release(cl_command_queue queue, cl_uint count, const cl_mem *objects, cl_uint waits,
                       const cl_event *wait_list, cl_event *event) override;
    cl_int     object_info(cl_mem object, cl_gl_object_type *type, cl_GLuint *name) override;
    cl_int     texture_info(cl_mem object, cl_gl_texture_info param, size_t value_bytes, void *value,
                            size_t *value_bytes_ret) override;
    void       forget(cl_mem object) override;
    void       forget_all() override;

    uint32_t renderbuffer_images() const;

private:
    friend struct DirectGlSharingProbe;

    static constexpr uint32_t max_context_properties = 32u;

    cl_mem over_texture(cl_context context, cl_mem_flags flags, cl_GLuint renderbuffer, cl_int *errcode_ret);

    const GlContext       &vehicle_;
    GlRenderbufferTexture &textures_;
    const GlDisplay       *display_;
    ClRenderbufferImages   renderbuffers_;
};

}

#endif
