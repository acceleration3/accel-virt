/* The OpenGL server dispatch's table with no OpenGL behind it.
 *
 * Fills a representative of every kind of pointer the decoder hands a driver -
 * reads and uploads sized by the pixel store, count-sized outputs and inputs,
 * text, state queries whose answer is a list, offsets into bound buffers, and
 * the vertex arrays a draw reads - along with the state each depends on, so a
 * test or a fuzz target can drive the generated decoder over them with no GPU.
 * Every other entry stays null, which the decoder refuses after decoding.
 *
 * What it records is what the decoder must never let through: a pointer the
 * driver would read or write past what the decode arena gave out, a null one
 * the driver would follow, or an offset followed as an address because no
 * buffer was bound to make it one - a stray, every one. The state queries the
 * decoder itself asks go to its own locals and are answered unchecked. */
#ifndef AV_RECORDING_GL_HPP
#define AV_RECORDING_GL_HPP

#include "proto/generated/opengl/gl_real_dispatch.h"

#include <cstddef>
#include <cstdint>

namespace av {

class WireArena;

class RecordingGl {
public:
    /* How many of each this fake's driver has. */
    static constexpr GLint vertex_attributes = 16;
    static constexpr GLint texture_units = 2;
    /* GL_NUM_COMPRESSED_TEXTURE_FORMATS: more than any slack a query gets. */
    static constexpr GLint compressed_formats = 48;
    /* GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS of every block. */
    static constexpr GLint block_uniforms = 40;

    /* `block` is the start of the memory `arena` hands out from. The table's
     * entry points are GL's own signatures and carry no user pointer, so they
     * reach the one live RecordingGl through the harness's static storage: a
     * second one made while another is alive takes the table over. */
    RecordingGl(const std::uint8_t *block, const WireArena &arena);
    ~RecordingGl();
    RecordingGl(const RecordingGl &) = delete;
    RecordingGl &operator=(const RecordingGl &) = delete;

    const glproto::RealGl &table() const;
    /* What glGetTexLevelParameteriv answers for any level. */
    void set_level_extent(GLint width, GLint height, GLint depth);
    /* Removes glGetIntegerv, which no pixel store or binding can be asked
     * without. */
    void forget_state_queries();

    /* Pixel reads that reached the driver. */
    std::uint32_t reads() const;
    /* Every other call that reached the driver, state aside. */
    std::uint32_t calls() const;
    std::uint32_t strays() const;

    /* The entry points, as the table calls them. */
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
    GLboolean is_enabled(GLenum capability) const;
    void client_state(GLenum capability, bool enabled);
    void client_texture(GLenum unit);
    void array_pointer(GLenum array, const void *pointer);
    void uniform_block(GLenum pname, GLint *values);
    void map(GLenum target, GLint stride, GLint order, const GLfloat *points);
    void map_values(GLenum target, GLenum query, GLfloat *values);
    void map_order(GLenum target, GLenum query, GLint *values) const;
    void stipple(GLubyte *mask);
    void bitmap(GLsizei width, GLsizei height, const GLubyte *bits);
    /* A call whose driver keeps the pointer: never one the decoder makes. */
    void kept_pointer();

private:
    /* Alignment, skip pixels, skip rows, skip images, row length, image height. */
    using Store = GLint[6];

    /* Where GL touches the byte after the last pixel of this transfer, from
     * `store`; false for a format and type this fake does not size. */
    bool reach(const Store &store, GLsizei width, GLsizei height, GLsizei depth, GLenum format,
               GLenum type, std::uint64_t &bytes) const;
    bool bitmap_reach(const Store &store, GLsizei width, GLsizei height, std::uint64_t &bytes) const;
    void check_span(const void *bytes, std::uint64_t length);
    /* A string GL reads to its terminator. */
    void check_terminated(const GLchar *text);
    /* Whether any enabled array has no buffer behind it. */
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
};

}  // namespace av

#endif  // AV_RECORDING_GL_HPP
