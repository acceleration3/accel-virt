#ifndef AV_GL_TEXTURE_SLICES_HPP
#define AV_GL_TEXTURE_SLICES_HPP

#include "executor/opengl/gl_texture_targets.hpp"
#include "proto/generated/opengl/gl_real_dispatch.h"

#include <stdint.h>

namespace av {

class BlockSource;
class GlDmaBufImages;
class GlErrorQueue;
class GlMemoryObjects;
class GlTextureShape;

class GlTextureSlices {
public:
    GlTextureSlices(BlockSource &blocks, glproto::RealGl &gl, GlMemoryObjects &memories, GlDmaBufImages &images, GlErrorQueue &errors);
    GlTextureSlices(const GlTextureSlices &) = delete;
    GlTextureSlices &operator=(const GlTextureSlices &) = delete;

    void install();

    bool on_tex_storage_mem_1d(GLenum target, GLsizei levels, GLenum format, GLsizei width, GLuint memory,
                               GLuint64 offset);
    bool on_tex_storage_mem_2d(GLenum target, GLsizei levels, GLenum format, GLsizei width, GLsizei height,
                               GLuint memory, GLuint64 offset);
    bool on_tex_storage_mem_2d_multisample(GLenum target, GLsizei samples, GLenum format, GLsizei width,
                                           GLsizei height, GLboolean fixed, GLuint memory, GLuint64 offset);
    bool on_tex_storage_mem_3d(GLenum target, GLsizei levels, GLenum format, GLsizei width, GLsizei height,
                               GLsizei depth, GLuint memory, GLuint64 offset);
    bool on_tex_storage_mem_3d_multisample(GLenum target, GLsizei samples, GLenum format, GLsizei width,
                                           GLsizei height, GLsizei depth, GLboolean fixed, GLuint memory,
                                           GLuint64 offset);
    bool on_texture_storage_mem_1d(GLuint texture, GLsizei levels, GLenum format, GLsizei width, GLuint memory,
                                   GLuint64 offset);
    bool on_texture_storage_mem_2d(GLuint texture, GLsizei levels, GLenum format, GLsizei width, GLsizei height,
                                   GLuint memory, GLuint64 offset);
    bool on_texture_storage_mem_2d_multisample(GLuint texture, GLsizei samples, GLenum format, GLsizei width,
                                               GLsizei height, GLboolean fixed, GLuint memory, GLuint64 offset);
    bool on_texture_storage_mem_3d(GLuint texture, GLsizei levels, GLenum format, GLsizei width, GLsizei height,
                                   GLsizei depth, GLuint memory, GLuint64 offset);
    bool on_texture_storage_mem_3d_multisample(GLuint texture, GLsizei samples, GLenum format, GLsizei width,
                                               GLsizei height, GLsizei depth, GLboolean fixed, GLuint memory,
                                               GLuint64 offset);
    bool on_delete_textures(GLsizei count, const GLuint *textures);
    void on_textures_created(GLenum target, GLsizei count, const GLuint *textures);
    void on_textures_generated(GLsizei count, const GLuint *textures);
    void on_texture_bound(GLenum target, GLuint texture);
    void on_texture_imaged(GLuint texture, GLenum target);
    bool on_texture_view(GLuint texture, GLenum target, GLuint original, GLenum format, GLuint min_level,
                         GLuint levels, GLuint min_layer, GLuint layers);

private:
    enum class Entry { one_d, two_d, two_d_multisample, three_d, three_d_multisample };

    uint32_t bound_to(uint32_t binding);
    GlTextureShape shape_of(uint32_t texture, Entry entry, GLenum format, int64_t levels, int64_t samples,
                            int64_t width, int64_t height, int64_t depth) const;
    uint32_t vet(uint32_t texture, const GlTextureShape &shape, uint32_t memory, uint64_t offset);
    bool     settle(uint32_t texture, const GlTextureShape &shape, uint32_t memory, uint64_t offset);
    bool     over_image(uint32_t texture, const GlTextureShape &shape, uint32_t memory, uint64_t offset,
                        GLsizei width, GLsizei height, GLenum format);
    bool     refuse(uint32_t error);

    glproto::RealGl &gl_;
    GlMemoryObjects &memories_;
    GlDmaBufImages  &images_;
    GlErrorQueue    &errors_;
    GlTextureTargets targets_;
};

}

#endif
