#ifndef AV_COPIED_GL_SHARING_HPP
#define AV_COPIED_GL_SHARING_HPP

#include "executor/opencl/cl_gl_mirrors.hpp"
#include "executor/opencl/gl_sharing.hpp"
#include "executor/opencl/staging_bytes.hpp"

#include <stdint.h>

namespace av {

class ByteLedger;

class GlClTarget;
class GlLevel;
class GlObjectBytes;
class GlRenderbufferTexture;

class CopiedGlSharing final : public GlSharing {
public:
    CopiedGlSharing(BlockSource &blocks, GlObjectBytes &objects, GlRenderbufferTexture &textures);

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
    void use_host_ram(ByteLedger *ledger);
    void release_staging();

    uint32_t mirrors() const;

private:
    cl_mem image_of(cl_context context, cl_mem_flags flags, const GlClTarget &target, GlLevel level,
                    uint32_t object_type, uint32_t name, void *image, cl_int *errcode_ret);
    cl_mem held(cl_mem made, const GlMirror &mirror, cl_int *errcode_ret);
    cl_int judged(cl_uint count, const cl_mem *objects);
    cl_int copy_in(cl_command_queue queue, GlMirror &mirror);
    cl_int copy_out(cl_command_queue queue, GlMirror &mirror);

    GlObjectBytes         &objects_;
    GlRenderbufferTexture &textures_;
    ClGlMirrors            mirrors_;
    StagingBytes           staging_;
};

}

#endif
