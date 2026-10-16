/* The OpenGL errors one context's commands raised on the host, counted by
 * kind in the one word the host publishes at the end of the context's blob
 * arena (docs/protocol.md, "Error word and published slots"): byte k counts
 * GL_INVALID_ENUM + k for the seven kinds up to
 * GL_INVALID_FRAMEBUFFER_OPERATION, modulo 256, and
 * byte 7 is the marker saying the host publishes the word at all. Only the
 * host writes it and nothing clears it: the guest reports a kind whenever its
 * count moved since it last looked. */
#ifndef AV_GL_ERROR_WORD_HPP
#define AV_GL_ERROR_WORD_HPP

#include <stdint.h>

namespace av {

class GlErrorWord {
public:
    static constexpr uint32_t first_error = 0x0500u; /* GL_INVALID_ENUM */
    static constexpr uint32_t kinds = 7u;

    /* The host's word before any error: published, nothing counted. */
    GlErrorWord();
    /* A word as the guest read it from the arena. */
    explicit GlErrorWord(uint64_t word);

    /* One error the host's glGetError returned. A code outside the seven
     * kinds - GL_CONTEXT_LOST, which the guest learns of from the context
     * itself - is not counted. */
    void     record(uint32_t error);
    /* Whether a host published the word: one without the marker is a host
     * that answers glGetError only when asked, or an arena nothing wrote. */
    bool     published() const;
    /* The count of the `kind`th error, modulo 256; 0 past the kinds. */
    uint8_t  count(uint32_t kind) const;
    uint64_t word() const;

private:
    uint64_t word_;
};

}  // namespace av

#endif  // AV_GL_ERROR_WORD_HPP
