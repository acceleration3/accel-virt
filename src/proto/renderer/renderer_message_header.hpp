/* What opens every datagram on the socket between QEMU and av-renderer
 * (docs/renderer.md): the message's kind, how many body bytes follow, and the
 * sender's sequence number, which a reply names. SOCK_SEQPACKET keeps each
 * message one datagram, so the body length must be exactly what arrived. */
#ifndef AV_RENDERER_MESSAGE_HEADER_HPP
#define AV_RENDERER_MESSAGE_HEADER_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class RendererMessageHeader {
public:
    /* Zero is no kind, so a zeroed buffer is not a message. */
    enum class Kind : uint32_t {
        hello = 1u,
        ready,
        region,
        guest_blob,
        allocate_memory,
        blob_retired,
        create_context,
        bind_context,
        destroy_context,
        interrupt_lines,
        interrupts_off,
        reply,
        export_image,
        exported_image,
        shared_released,
        allocated_memory,
        free_memory
    };

    static constexpr size_t   encoded_size = 16u;
    /* The largest body is a guest blob of GuestBlobMessage::max_extents
     * extents - a 64 MiB blob scattered page by page - so a socket's send
     * buffer must hold one datagram this long. */
    static constexpr uint32_t max_body_length = 0x40010u;

    RendererMessageHeader();
    RendererMessageHeader(Kind kind, uint32_t body_length, uint64_t sequence);

    Kind     kind() const;
    uint32_t body_length() const;
    uint64_t sequence() const;

    /* too_large for less room than encoded_size; invalid_parameter for a
     * header decode would refuse. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* `length` is the whole datagram, header and body. malformed for an
     * unknown kind, a body past max_body_length or other than what arrived,
     * and a body on interrupts_off. Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    bool fields_valid() const;

    uint32_t kind_;
    uint32_t body_length_;
    uint64_t sequence_;
};

}  // namespace av

#endif  // AV_RENDERER_MESSAGE_HEADER_HPP
