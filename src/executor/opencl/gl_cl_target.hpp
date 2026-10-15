/* A texture target clCreateFromGLTexture names, as the rest of the copy path
 * needs it (CopiedGlSharing): the target the texture is bound at - a cube
 * map's for each of its faces - the image OpenCL makes for it, the GL object
 * type clGetGLObjectInfo answers, and how many dimensions a glTexSubImage of
 * one of its levels takes. Only the targets cl_khr_gl_sharing lists that hold
 * texels of their own; a buffer texture's are its buffer's, and a
 * multisampled one is another extension's. */
#ifndef AV_GL_CL_TARGET_HPP
#define AV_GL_CL_TARGET_HPP

#include <CL/cl.h>

#include <stdint.h>

namespace av {

class GlClTarget {
public:
    explicit GlClTarget(uint32_t target);

    bool     known() const;
    /* The target as named, which a level's texels are read and written at. */
    uint32_t image_target() const;
    /* Where the texture is bound to reach it; 0 for an unknown target. */
    uint32_t bind_target() const;
    /* cl_mem_object_type and cl_gl_object_type; 0 for an unknown target. */
    uint32_t image_type() const;
    uint32_t object_type() const;
    /* 1, 2 or 3; 0 for an unknown target. A 1D array's layers are its
     * second, a 2D array's its third. */
    uint32_t dimensions() const;
    /* The image OpenCL makes for a level `width` x `height` x `depth` texels
     * as GL counts them - a 1D array's layers its height, a 2D array's its
     * depth - written over all of `desc`. */
    void     describe(uint32_t width, uint32_t height, uint32_t depth, cl_image_desc &desc) const;

private:
    uint32_t image_target_;
    uint32_t bind_target_;
    uint32_t image_type_;
    uint32_t object_type_;
    uint32_t dimensions_;
};

}  // namespace av

#endif  // AV_GL_CL_TARGET_HPP
