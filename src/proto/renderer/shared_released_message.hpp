#ifndef AV_SHARED_RELEASED_MESSAGE_HPP
#define AV_SHARED_RELEASED_MESSAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class SharedReleasedMessage {
public:
    static constexpr size_t   encoded_size = 8u;
    static constexpr uint32_t descriptors = 0u;

    SharedReleasedMessage();
    explicit SharedReleasedMessage(uint64_t object_id);

    uint64_t object_id() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count);

private:
    uint64_t object_id_;
};

}

#endif
