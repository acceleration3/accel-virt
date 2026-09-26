/* Either way: the answer to the request whose header carried `sequence`, as
 * the av::Status the other side's operation returned and whatever small result
 * it has. A renderer that misses a request's deadline is restarted
 * (docs/renderer.md, Lifecycle), so no reply means no answer. The payload
 * is a view into the bytes decoded or the storage given, which must outlive
 * the message. */
#ifndef AV_REPLY_MESSAGE_HPP
#define AV_REPLY_MESSAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ReplyMessage {
public:
    static constexpr size_t   fixed_size = 16u;
    /* Answers are a handful of words; anything bigger travels as a blob. */
    static constexpr uint32_t max_payload_length = 1024u;
    static constexpr uint32_t descriptors = 0u;

    ReplyMessage();
    /* A null payload is an empty one. */
    ReplyMessage(uint64_t sequence, Status status, const uint8_t *payload, uint32_t payload_length);

    uint64_t       sequence() const;
    Status         status() const;
    const uint8_t *payload() const;
    uint32_t       payload_length() const;
    size_t         encoded_length() const;

    /* too_large for less room than encoded_length(); invalid_parameter for a
     * message decode would refuse. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for any descriptor, a status av::Status does not have, a
     * payload past max_payload_length, or a length other than the fields
     * give. Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count);

private:
    bool fields_valid() const;

    uint64_t       sequence_;
    uint32_t       status_;
    const uint8_t *payload_;
    uint32_t       payload_length_;
};

}  // namespace av

#endif  // AV_REPLY_MESSAGE_HPP
