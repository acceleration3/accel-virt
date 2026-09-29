/* The OpenGL errors one context's commands raised on the host, counted by
 * kind, as the word the guest reads them from (docs/protocol.md, "Error
 * word"): byte k counts GL_INVALID_ENUM + k for the seven kinds up to
 * GL_INVALID_FRAMEBUFFER_OPERATION, modulo 256, and byte 7 is the marker that
 * says the host publishes the word at all. The guest reports a kind whenever
 * its count moved since it last looked, so the word never needs clearing and
 * only the host ever writes it. */
#ifndef AV_GL_ERROR_TALLY_HPP
#define AV_GL_ERROR_TALLY_HPP

#include <stdint.h>

namespace av {

class GlErrorTally {
public:
    static constexpr uint32_t first_error = 0x0500u; /* GL_INVALID_ENUM */
    static constexpr uint32_t kinds = 7u;
    static constexpr uint64_t published = uint64_t{0x80} << 56u;

    GlErrorTally();

    /* One error the host's glGetError returned. A code outside the seven
     * kinds - GL_CONTEXT_LOST, which the guest learns of from the context
     * itself - is not counted. */
    void     record(uint32_t error);
    uint64_t word() const;

private:
    uint8_t counts_[kinds];
};

}  // namespace av

#endif  // AV_GL_ERROR_TALLY_HPP
