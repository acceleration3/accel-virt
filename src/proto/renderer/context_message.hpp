#ifndef AV_CONTEXT_MESSAGE_HPP
#define AV_CONTEXT_MESSAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ContextMessage {
public:
    static constexpr size_t   name_bytes = 64u;
    static constexpr size_t   encoded_size = 4u + name_bytes;
    static constexpr uint32_t descriptors = 0u;

    ContextMessage();
    explicit ContextMessage(uint32_t context);
    ContextMessage(uint32_t context, const char *name);

    uint32_t    context() const;
    const char *name() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count);

private:
    uint32_t context_;
    char     name_[name_bytes];
};

}

#endif
