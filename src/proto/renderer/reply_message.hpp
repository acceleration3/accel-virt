#ifndef AV_REPLY_MESSAGE_HPP
#define AV_REPLY_MESSAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ReplyMessage {
public:
    static constexpr size_t   fixed_size = 16u;
    static constexpr uint32_t max_payload_length = 1024u;
    static constexpr uint32_t descriptors = 0u;

    ReplyMessage();
    ReplyMessage(uint64_t sequence, Status status, const uint8_t *payload, uint32_t payload_length);

    uint64_t       sequence() const;
    Status         status() const;
    const uint8_t *payload() const;
    uint32_t       payload_length() const;
    size_t         encoded_length() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count);

private:
    bool fields_valid() const;

    uint64_t       sequence_;
    uint32_t       status_;
    const uint8_t *payload_;
    uint32_t       payload_length_;
};

}

#endif
