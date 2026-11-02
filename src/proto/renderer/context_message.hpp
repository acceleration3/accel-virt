#ifndef AV_CONTEXT_MESSAGE_HPP
#define AV_CONTEXT_MESSAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ContextMessage {
public:
    static constexpr size_t   encoded_size = 4u;
    static constexpr uint32_t descriptors = 0u;

    ContextMessage();
    explicit ContextMessage(uint32_t context);

    uint32_t context() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count);

private:
    uint32_t context_;
};

}

#endif
