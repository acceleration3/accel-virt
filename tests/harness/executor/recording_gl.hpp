#ifndef AV_RECORDING_GL_HPP
#define AV_RECORDING_GL_HPP

#include "proto/generated/opengl/gl_real_dispatch.h"

#include <cstddef>
#include <cstdint>

namespace av {

class WireArena;

class RecordingGl {
public:
    static constexpr GLint vertex_attributes = 16;
    static constexpr GLint texture_units = 2;
    static constexpr GLint compressed_formats = 48;
    static constexpr GLint block_uniforms = 40;

    RecordingGl(const std::uint8_t *block, const WireArena &arena);
    ~RecordingGl();
    RecordingGl(const RecordingGl &) = delete;
    RecordingGl &operator=(const RecordingGl &) = delete;

    const glproto::RealGl &table() const;
    void set_level_extent(GLint width, GLint height, GLint depth);
    void forget_state_queries();
    void be_core();
    std::uint32_t capability_queries() const;

    std::uint32_t reads() const;
    std::uint32_t calls() const;
    std::uint32_t strays() const;

    void pixel_store(GLenum pname, GLint value);
    void bind_buffer(GLenum target, GLuint name);
    void delete_buffers(GLsizei n, const GLuint *names);
    void integer(GLenum pname, GLint *value);
    void level_parameter(GLenum pname, GLint *value) const;
    void read_pixels(GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type,
                     void *pixels);
    void read_bounded(std::size_t bytes, void *pixels);
    void generate(GLsizei n, GLuint *names);
    void info_log(GLsizei room, GLsizei *length, GLchar *log);
    void uniform_values(GLsizei count, const GLfloat *values);
    void texture_parameter(GLenum pname, const GLint *values);
    void upload(GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels,
                bool specifies);
    void compressed(GLsizei size, const void *data);
    void clear_data(GLenum format, GLenum type, const void *data);
    void buffer_data(GLsizeiptr size, const void *data);
    void call_lists(GLsizei n, GLenum type, const void *lists);
    void shader_source(GLsizei count, const GLchar *const *strings, const GLint *lengths);
    void debug_text(GLsizei length, const GLchar *text);
    void draw(bool indexed);
    void draw_many(GLsizei drawcount, const GLsizei *counts, const void *const *indices);
    void draw_indirect();
    void attribute_pointer(GLuint index, const void *pointer);
    void attribute_enabled(GLuint index, bool enabled);
    void attribute_query(GLuint index, GLenum pname, GLint *value);
    GLboolean is_enabled(GLenum capability);
    void client_state(GLenum capability, bool enabled);
    void client_texture(GLenum unit);
    void array_pointer(GLenum array, const void *pointer);
    void uniform_block(GLenum pname, GLint *values);
    void map(GLenum target, GLint stride, GLint order, const GLfloat *points);
    void map_values(GLenum target, GLenum query, GLfloat *values);
    void map_order(GLenum target, GLenum query, GLint *values) const;
    void stipple(GLubyte *mask);
    void bitmap(GLsizei width, GLsizei height, const GLubyte *bits);
    void kept_pointer();

private:
    using Store = GLint[6];

    bool reach(const Store &store, GLsizei width, GLsizei height, GLsizei depth, GLenum format,
               GLenum type, std::uint64_t &bytes) const;
    bool bitmap_reach(const Store &store, GLsizei width, GLsizei height, std::uint64_t &bytes) const;
    void check_span(const void *bytes, std::uint64_t length);
    void check_terminated(const GLchar *text);
    bool client_array_enabled() const;
    GLuint *binding(GLenum target);

    const std::uint8_t *block_;
    const WireArena    &arena_;
    glproto::RealGl     table_;
    Store               pack_;
    Store               unpack_;
    GLint               compressed_block_[4];
    GLuint              pack_buffer_;
    GLuint              unpack_buffer_;
    GLuint              array_buffer_;
    GLuint              element_buffer_;
    GLuint              indirect_buffer_;
    GLuint              parameter_buffer_;
    bool                attribute_enabled_[vertex_attributes];
    GLuint              attribute_buffer_[vertex_attributes];
    bool                vertex_array_enabled_;
    GLuint              vertex_array_buffer_;
    bool                texture_array_enabled_[texture_units];
    GLuint              texture_array_buffer_[texture_units];
    GLint               client_unit_;
    GLenum              map_target_;
    GLint               map_order_;
    GLint               level_extent_[3];
    std::uint32_t       reads_;
    std::uint32_t       calls_;
    std::uint32_t       strays_;
    std::uint32_t       capability_queries_;
};

}

#endif
