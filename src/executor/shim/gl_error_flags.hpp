/* The error flags of one host GL context the executor works in, read and
 * cleared: whether a call of its own was refused, and the context left with
 * no flag set for whatever runs in it next.
 *
 * One reading for every GL helper that asks (GlHostImport, GlSharedBuffer,
 * GlObjectBytes, GlRenderbufferTexture), because the reading has a trap: GL
 * keeps one flag per kind, so a context has at most a handful to read, but a
 * lost one answers GL_CONTEXT_LOST to every look, and a loop waiting for
 * GL_NO_ERROR never leaves it. */
#ifndef AV_GL_ERROR_FLAGS_HPP
#define AV_GL_ERROR_FLAGS_HPP

#include "proto/generated/opengl/gl_types.h"

namespace av {

class GlContext;

class GlErrorFlags {
public:
    GlErrorFlags();

    /* glGetError of `context`, which is current. */
    void resolve(const GlContext &context);
    bool resolved() const;
    /* Every flag the current context has set, read and cleared - no more
     * reads than GL has flags: true when none was set. */
    [[nodiscard]] bool clear();

private:
    using GetError = GLenum(APIENTRY *)();

    GetError get_error_;
};

}  // namespace av

#endif  // AV_GL_ERROR_FLAGS_HPP
