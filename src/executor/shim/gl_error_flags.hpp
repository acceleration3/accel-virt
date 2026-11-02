#ifndef AV_GL_ERROR_FLAGS_HPP
#define AV_GL_ERROR_FLAGS_HPP

#include "proto/generated/opengl/gl_types.h"

namespace av {

class GlContext;

class GlErrorFlags {
public:
    GlErrorFlags();

    void resolve(const GlContext &context);
    bool resolved() const;
    [[nodiscard]] bool clear();

private:
    using GetError = GLenum(APIENTRY *)();

    GetError get_error_;
};

}

#endif
