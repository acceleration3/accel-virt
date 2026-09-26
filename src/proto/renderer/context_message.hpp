/* QEMU -> renderer: the body of create_context and destroy_context, which name
 * nothing but the device's id for the context; its ring and doorbell arrive
 * with bind_context. */
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

    /* too_large for less room than encoded_size; invalid_parameter for
     * context 0, which the device never gives. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for any other length, any descriptor, or context 0. Unchanged
     * unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count);

private:
    uint32_t context_;
};

}  // namespace av

#endif  // AV_CONTEXT_MESSAGE_HPP
