/* One cl_mem standing in for a GL object of the guest's, where the host's
 * OpenCL cannot name the object itself (CopiedGlSharing): the object, as
 * clGetGLObjectInfo and clGetGLTextureInfo answer it; how its bytes move -
 * a buffer's whole store, or one texture level at its transfer format, a
 * renderbuffer's through a texture of its storage; and whether OpenCL holds
 * it, between an acquire and a release. */
#ifndef AV_GL_MIRROR_HPP
#define AV_GL_MIRROR_HPP

#include "executor/shim/gl_level.hpp"

#include <stdint.h>

namespace av {

class GlMirror {
public:
    GlMirror();
    /* `bytes` of the buffer `name`. */
    GlMirror(const void *mem, uint32_t name, uint64_t bytes, bool written_back);
    /* A texture's or renderbuffer's (`object_type`) `level`, `bytes` of it,
     * named `name` and, a texture, `target`; `image` is the image a
     * renderbuffer's texture is made over, or null. */
    GlMirror(const void *mem, uint32_t object_type, uint32_t name, uint32_t target, const GlLevel &level,
             uint64_t bytes, void *image, bool written_back);

    const void    *mem() const;
    /* cl_gl_object_type. */
    uint32_t       object_type() const;
    bool           buffer() const;
    /* A texture's, not a renderbuffer's or a buffer's. */
    bool           texture() const;
    uint32_t       name() const;
    uint32_t       target() const;
    const GlLevel &level() const;
    uint64_t       bytes() const;
    void          *image() const;
    /* Whether a release copies OpenCL's bytes back: not for a cl_mem made
     * CL_MEM_READ_ONLY, which OpenCL cannot have changed. */
    bool           written_back() const;
    bool           acquired() const;
    void           acquire();
    void           release();

    /* A mirror of something: a mem, a name, bytes, and a type. */
    bool described() const;

private:
    const void *mem_;
    uint32_t    object_type_;
    uint32_t    name_;
    uint32_t    target_;
    GlLevel     level_;
    uint64_t    bytes_;
    void       *image_;
    bool        written_back_;
    bool        acquired_;
};

}  // namespace av

#endif  // AV_GL_MIRROR_HPP
