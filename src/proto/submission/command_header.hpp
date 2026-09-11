/* The 8 bytes in front of each command in a batch (docs/protocol.md), framed as
 * render-server frames its command streams: the command id, whose top 16 bits
 * name the API, and the command's size including this header, with the call's
 * flags in the size's top byte. */
#ifndef AV_COMMAND_HEADER_HPP
#define AV_COMMAND_HEADER_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class CommandHeader {
public:
    static constexpr size_t   encoded_size   = 8u;
    static constexpr uint32_t max_size       = 0x00FFFFFFu;
    /* The producer waits for this command's answer. */
    static constexpr uint8_t  generate_reply = 1u;

    CommandHeader();
    CommandHeader(uint32_t id, uint32_t size, uint8_t flags);

    uint32_t id() const;
    /* 0 transport, 1 Vulkan, 2 OpenCL, 3 OpenGL. */
    uint16_t api() const;
    uint32_t size() const;
    bool     generates_reply() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* A size below the header or an unknown flag is malformed. Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t flags_shift = 24u;

    uint32_t id_;
    uint32_t size_;
    uint8_t  flags_;
};

}  // namespace av

#endif  // AV_COMMAND_HEADER_HPP
