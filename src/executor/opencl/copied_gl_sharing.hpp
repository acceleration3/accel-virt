/* GL objects reaching OpenCL as copies, where the host's OpenCL makes no
 * context against the vehicle: NVIDIA's, which takes no EGL context for
 * cl_khr_gl_sharing, measured on an RTX 3070 Ti under driver 580.
 *
 * The extension makes the bytes' visibility a matter of two points. GL's
 * writes are OpenCL's to read from clEnqueueAcquireGLObjects on - the
 * application has finished them with glFinish first - and OpenCL's are GL's
 * from clEnqueueReleaseGLObjects on, once the application has finished the
 * queue. So each GL object the guest opens is answered with a cl_mem of the
 * host's own, of its size or its image format and extent (GlMirror), and the
 * object's bytes are copied into it at the acquire and back out at the
 * release, through the vehicle - a GL context in the guest's share group, so
 * the guest's names are its names - and private memory. A cl_mem made
 * CL_MEM_READ_ONLY is never copied back, and one made CL_MEM_WRITE_ONLY is
 * still copied in: a kernel that writes part of it leaves the rest as GL had
 * it, as it would in the object itself.
 *
 * The copies are made on the guest's queue, behind a barrier of its wait
 * list, and blocking, as every transfer here is (OpenClBackend); the event the
 * guest asked for is a marker after them. */
#ifndef AV_COPIED_GL_SHARING_HPP
#define AV_COPIED_GL_SHARING_HPP

#include "executor/opencl/cl_gl_mirrors.hpp"
#include "executor/opencl/gl_sharing.hpp"
#include "executor/opencl/staging_bytes.hpp"

#include <stdint.h>

namespace av {

class GlClTarget;
class GlLevel;
class GlObjectBytes;
class GlRenderbufferTexture;

class CopiedGlSharing final : public GlSharing {
public:
    /* `objects` and `textures` work through the vehicle and must outlive
     * this. */
    CopiedGlSharing(GlObjectBytes &objects, GlRenderbufferTexture &textures);

    /* The guest's own: no context of this host's names the vehicle. */
    cl_context make_context(const cl_context_properties *properties, cl_uint num_devices,
                            const cl_device_id *devices, cl_int *errcode_ret) override;
    /* A buffer of `buffer`'s size. CL_INVALID_VALUE for flags other than one
     * of the three access flags; CL_INVALID_GL_OBJECT for a name that is no
     * buffer with a store. */
    cl_mem     from_buffer(cl_context context, cl_mem_flags flags, cl_GLuint buffer, cl_int *errcode_ret) override;
    /* An image of `level`'s format and extent. CL_INVALID_VALUE for a target
     * GlClTarget does not know, CL_INVALID_MIP_LEVEL for a negative level,
     * CL_INVALID_GL_OBJECT for a name that is no texture of that target with
     * texels at that level, CL_INVALID_IMAGE_FORMAT_DESCRIPTOR for an internal
     * format OpenCL has no image format for. */
    cl_mem     from_texture(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint level,
                            cl_GLuint texture, cl_int *errcode_ret) override;
    /* A 2D image of the renderbuffer's format and extent, its bytes moved
     * through a texture of its storage; CL_INVALID_GL_OBJECT where none can
     * be made. */
    cl_mem     from_renderbuffer(cl_context context, cl_mem_flags flags, cl_GLuint renderbuffer,
                                 cl_int *errcode_ret) override;
    /* Each object's bytes copied in. CL_INVALID_VALUE for objects that do
     * not match their count, CL_INVALID_GL_OBJECT for a cl_mem made from no
     * GL object - before anything is copied - or one whose GL object is gone. */
    cl_int     acquire(cl_command_queue queue, cl_uint count, const cl_mem *objects, cl_uint waits,
                       const cl_event *wait_list, cl_event *event) override;
    /* Each acquired object's bytes copied back, unless it was made read-only,
     * and GL finished; refused as the acquire is. */
    cl_int     release(cl_command_queue queue, cl_uint count, const cl_mem *objects, cl_uint waits,
                       const cl_event *wait_list, cl_event *event) override;
    cl_int     object_info(cl_mem object, cl_gl_object_type *type, cl_GLuint *name) override;
    /* The target and level the texture was named at; CL_INVALID_GL_OBJECT for
     * a cl_mem of no texture, CL_INVALID_VALUE for any other question. */
    cl_int     texture_info(cl_mem object, cl_gl_texture_info param, size_t value_bytes, void *value,
                            size_t *value_bytes_ret) override;
    /* The mirror forgotten, and a renderbuffer's texture destroyed. */
    void       forget(cl_mem object) override;
    /* Every mirror as forget does, then the table's memory given back. */
    void       forget_all() override;

    uint32_t mirrors() const;

private:
    /* The image for `level` of a texture of `target`, remembered as a mirror
     * of `object_type` and `name`. */
    cl_mem image_of(cl_context context, cl_mem_flags flags, const GlClTarget &target, GlLevel level,
                    uint32_t object_type, uint32_t name, void *image, cl_int *errcode_ret);
    /* `made`, remembered as `mirror`, or released and refused when it cannot
     * be. */
    cl_mem held(cl_mem made, const GlMirror &mirror, cl_int *errcode_ret);
    /* CL_SUCCESS when every one of `objects` is a mirror here, with the
     * private memory the largest needs reserved. */
    cl_int judged(cl_uint count, const cl_mem *objects);
    cl_int copy_in(cl_command_queue queue, GlMirror &mirror);
    cl_int copy_out(cl_command_queue queue, GlMirror &mirror);

    GlObjectBytes         &objects_;
    GlRenderbufferTexture &textures_;
    ClGlMirrors            mirrors_;
    StagingBytes           staging_;
};

}  // namespace av

#endif  // AV_COPIED_GL_SHARING_HPP
