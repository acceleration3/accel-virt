/* Which of the host's OpenGL errors this context has handed the application.
 * The host counts each kind in a word it publishes at the end of the
 * context's blob arena (docs/protocol.md, "Error word"); a kind whose count
 * moved since the last look is an error to report, once, as glGetError
 * reports a flag once. The word only ever grows, so nothing clears it and
 * only the host writes it. */
#ifndef AV_GLICD_REPORTED_GL_ERRORS_HPP
#define AV_GLICD_REPORTED_GL_ERRORS_HPP

#include <stdint.h>

namespace av::glicd {

class ReportedGlErrors {
public:
    static constexpr uint32_t first_error = 0x0500u; /* GL_INVALID_ENUM */
    static constexpr uint32_t kinds = 7u;
    static constexpr uint64_t published = uint64_t{0x80} << 56u;

    ReportedGlErrors();

    /* Whether a host published `word`: one without the marker is a host that
     * answers glGetError only when asked, or an arena nothing wrote yet. */
    bool     readable(uint64_t word) const;
    /* The lowest kind whose count in `word` moved since it was last reported,
     * now reported; GL_NO_ERROR when none moved. */
    uint32_t take(uint64_t word);

private:
    uint8_t reported_[kinds];
};

}  // namespace av::glicd

#endif  // AV_GLICD_REPORTED_GL_ERRORS_HPP
