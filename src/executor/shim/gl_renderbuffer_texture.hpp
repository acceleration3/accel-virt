#ifndef AV_GL_RENDERBUFFER_TEXTURE_HPP
#define AV_GL_RENDERBUFFER_TEXTURE_HPP

#include "executor/shim/gl_error_flags.hpp"
#include "proto/generated/opengl/gl_types.h"

#include <stdint.h>

namespace av {

class GlContext;
class GlDisplay;

class GlRenderbufferTexture {
public:
    static constexpr uint32_t texture_2d = 0x0DE1u;

    GlRenderbufferTexture();
    GlRenderbufferTexture(const GlRenderbufferTexture &) = delete;
    GlRenderbufferTexture &operator=(const GlRenderbufferTexture &) = delete;

    void configure(GlContext &context, const GlDisplay &display);
    void give_back();
    bool makes_textures() const;

    [[nodiscard]] bool make(uint32_t renderbuffer, uint32_t &texture, void *&image);
    void               destroy(uint32_t texture, void *image);

private:
    GlContext *current();

    void *(*create_image_)(void *, void *, unsigned, void *, const int32_t *);
    unsigned (*destroy_image_)(void *, void *);
    void (APIENTRY *image_target_texture_)(GLenum, void *);
    void (APIENTRY *gen_textures_)(GLsizei, GLuint *);
    void (APIENTRY *bind_texture_)(GLenum, GLuint);
    void (APIENTRY *delete_textures_)(GLsizei, const GLuint *);
    GlErrorFlags errors_;
    GlContext       *context_;
    const GlDisplay *display_;
};

}

#endif
