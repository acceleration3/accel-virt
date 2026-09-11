/* One batch's commands (docs/protocol.md), read from the staged copy of its
 * payload: each header is checked against what is left of the copy before its
 * payload is handed on. Once malformed, it stays malformed. Any thread. */
#ifndef AV_COMMAND_STREAM_HPP
#define AV_COMMAND_STREAM_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class CommandHeader;

class CommandStream {
public:
    CommandStream(const uint8_t *bytes, size_t length);

    /* ok with the next command's header and the bytes after it; no_space at
     * the end; malformed for a header that does not decode, a command running
     * past the payload, or trailing bytes too short for a header. */
    [[nodiscard]] Status next(CommandHeader &header, const uint8_t *&payload);
    size_t               offset() const;

    bool invariants_hold() const;

private:
    const uint8_t *bytes_;
    size_t         length_;
    size_t         offset_;
    bool           malformed_;
};

}  // namespace av

#endif  // AV_COMMAND_STREAM_HPP
