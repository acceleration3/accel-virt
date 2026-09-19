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
    static constexpr uint16_t transport_api  = 0u;
    /* API 0's commands, which the device answers itself. A NOP lets a guest
     * time a round trip through the transport alone (feature 044); the blob
     * pair hands a context's guest pages to its executor and back (045). */
    static constexpr uint32_t nop_id          = 0x00000001u;
    static constexpr uint32_t import_blob_id  = 0x00000002u;
    static constexpr uint32_t release_blob_id = 0x00000003u;
    /* The shared-object trio, which gives one host resource an identity the
     * whole device agrees on rather than one context (feature 053). */
    static constexpr uint32_t share_object_id          = 0x00000004u;
    static constexpr uint32_t open_shared_object_id    = 0x00000005u;
    static constexpr uint32_t release_shared_object_id = 0x00000006u;

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
