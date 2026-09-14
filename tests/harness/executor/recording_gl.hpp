/* The OpenGL server dispatch's table with no OpenGL behind it.
 *
 * Fills the read-back entry points and the state they depend on - the pack
 * store, the pack buffer binding, a texture level's extent - so a test or a
 * fuzz target can drive the generated decoder over pixel reads with no GPU.
 * Every other entry stays null, which the decoder refuses after decoding.
 *
 * What it records is what the decoder must never let through: a read whose
 * destination, by GL's own addressing of the last pixel it writes, runs past
 * what the decode arena gave out, or at an address that was an offset with no
 * buffer bound to make it one (a stray either way). */
#ifndef AV_RECORDING_GL_HPP
#define AV_RECORDING_GL_HPP

#include "proto/generated/opengl/gl_real_dispatch.h"

#include <cstddef>
#include <cstdint>

namespace av {

class WireArena;

class RecordingGl {
public:
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
    /* Removes glGetIntegerv, which a pack size cannot be worked out without. */
    void forget_state_queries();

    std::uint32_t reads() const;
    std::uint32_t strays() const;

    /* The entry points, as the table calls them. */
    void pixel_store(GLenum pname, GLint value);
    void bind_buffer(GLenum target, GLuint name);
    void integer(GLenum pname, GLint *value) const;
    void level_parameter(GLenum pname, GLint *value) const;
    void read_pixels(GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type,
                     void *pixels);
    void read_bounded(std::size_t bytes, void *pixels);

private:
    /* Where GL writes the byte after the last pixel of this read, from the
     * pack store; false for a format and type this fake does not size. */
    bool reach(GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type,
               std::uint64_t &bytes) const;
    void check_span(const void *bytes, std::uint64_t length);

    const std::uint8_t *block_;
    const WireArena    &arena_;
    glproto::RealGl     table_;
    GLint               alignment_;
    GLint               skip_pixels_;
    GLint               skip_rows_;
    GLint               skip_images_;
    GLint               row_length_;
    GLint               image_height_;
    GLuint              pack_buffer_;
    GLint               level_extent_[3];
    std::uint32_t       reads_;
    std::uint32_t       strays_;
};

}  // namespace av

#endif  // AV_RECORDING_GL_HPP
