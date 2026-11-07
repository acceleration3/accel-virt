#ifndef AV_GL_EXTENSIONS_HPP
#define AV_GL_EXTENSIONS_HPP

#include "proto/generated/opengl/gl_types.h"

namespace av {

class GlContext;

class GlExtensions {
public:
    GlExtensions();

    void configure(GlContext &context);
    bool lists(const char *extension) const;

private:
    using IntegerQuery = void(APIENTRY *)(GLenum, GLint *);
    using StringQuery = const GLubyte *(APIENTRY *)(GLenum, GLuint);

    IntegerQuery get_integerv_;
    StringQuery  get_stringi_;
};

}

#endif
