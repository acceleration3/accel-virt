#ifndef AV_BIND_CONTEXT_MESSAGE_HPP
#define AV_BIND_CONTEXT_MESSAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BindContextMessage {
public:
    static constexpr size_t   fixed_size = 16u;
    static constexpr uint32_t has_doorbell_flag = 1u;

    BindContextMessage();
    BindContextMessage(uint32_t context, const uint8_t *setup, uint32_t setup_length);
    BindContextMessage(uint32_t context, const uint8_t *setup, uint32_t setup_length, uint32_t doorbell);

    uint32_t       context() const;
    bool           has_doorbell() const;
    uint32_t       doorbell() const;
    const uint8_t *setup() const;
    uint32_t       setup_length() const;
    uint32_t       descriptors() const;
    size_t         encoded_length() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count);

private:
    bool fields_valid() const;

    uint32_t       context_;
    uint32_t       doorbell_;
    uint32_t       flags_;
    const uint8_t *setup_;
    uint32_t       setup_length_;
};

}

#endif
