/* GL objects reaching OpenCL through the host's own cl_khr_gl_sharing: the
 * host's context is made against the vehicle, a GL context in the guest's
 * share group, so a name the guest hands clCreateFromGLBuffer is the host's
 * object and nothing is copied (feature 053, T019).
 *
 * One thing is answered here rather than by the host: a renderbuffer where
 * the host's sharing takes none - Intel's compute runtime answers every
 * clCreateFromGLRenderbuffer with CL_INVALID_GL_OBJECT - is opened as a
 * texture of the renderbuffer's own storage (GlRenderbufferTexture), which
 * OpenCL reads as GL draws it, and the guest is told of the renderbuffer it
 * named. */
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
    /* `vehicle` and `textures` must outlive this. */
    DirectGlSharing(BlockSource &blocks, const GlContext &vehicle, GlRenderbufferTexture &textures);

    /* The display the vehicle is open on, which must outlive this. */
    void configure(const GlDisplay &display);

    /* The guest's list with CL_GL_CONTEXT_KHR and CL_EGL_DISPLAY_KHR naming the
     * vehicle appended before its terminator; CL_INVALID_PROPERTY for a list
     * longer than this copies, which only a guest building one to overflow it
     * sends. */
    cl_context make_context(const cl_context_properties *properties, cl_uint num_devices,
                            const cl_device_id *devices, cl_int *errcode_ret) override;
    cl_mem     from_buffer(cl_context context, cl_mem_flags flags, cl_GLuint buffer, cl_int *errcode_ret) override;
    cl_mem     from_texture(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint level,
                            cl_GLuint texture, cl_int *errcode_ret) override;
    /* The host's answer, or where it refuses the renderbuffer as no GL
     * object it takes, a cl_mem over a texture of the renderbuffer's storage. */
    cl_mem     from_renderbuffer(cl_context context, cl_mem_flags flags, cl_GLuint renderbuffer,
                                 cl_int *errcode_ret) override;
    cl_int     acquire(cl_command_queue queue, cl_uint count, const cl_mem *objects, cl_uint waits,
                       const cl_event *wait_list, cl_event *event) override;
    cl_int     release(cl_command_queue queue, cl_uint count, const cl_mem *objects, cl_uint waits,
                       const cl_event *wait_list, cl_event *event) override;
    /* A cl_mem made over a renderbuffer is CL_GL_OBJECT_RENDERBUFFER and its
     * name, as the API has it, whatever it was made through; and has no
     * texture, so its texture query is CL_INVALID_GL_OBJECT. */
    cl_int     object_info(cl_mem object, cl_gl_object_type *type, cl_GLuint *name) override;
    cl_int     texture_info(cl_mem object, cl_gl_texture_info param, size_t value_bytes, void *value,
                            size_t *value_bytes_ret) override;
    /* The texture a renderbuffer's cl_mem was made over goes with it. */
    void       forget(cl_mem object) override;
    void       forget_all() override;

    /* cl_mems standing over a renderbuffer's texture now. */
    uint32_t renderbuffer_images() const;

private:
    friend struct DirectGlSharingProbe;

    /* A context property list this copies: OpenCL defines a handful and the
     * decoder has already refused every name but one, so a list longer than
     * this is a guest building one to overflow it. Room for the two appended
     * and the terminator is inside it. */
    static constexpr uint32_t max_context_properties = 32u;

    /* A cl_mem over a texture of `renderbuffer`'s storage, remembered as the
     * renderbuffer's; null, with the reason in `errcode_ret`, where none can
     * be made. */
    cl_mem over_texture(cl_context context, cl_mem_flags flags, cl_GLuint renderbuffer, cl_int *errcode_ret);

    const GlContext       &vehicle_;
    GlRenderbufferTexture &textures_;
    const GlDisplay       *display_;
    ClRenderbufferImages   renderbuffers_;
};

}  // namespace av

#endif  // AV_DIRECT_GL_SHARING_HPP
