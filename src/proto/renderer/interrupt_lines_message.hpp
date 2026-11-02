#ifndef AV_INTERRUPT_LINES_MESSAGE_HPP
#define AV_INTERRUPT_LINES_MESSAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class InterruptLinesMessage {
public:
    static constexpr size_t   encoded_size = 16u;
    static constexpr uint32_t max_lines = 64u;

    InterruptLinesMessage();
    InterruptLinesMessage(uint32_t lines, uint64_t present);

    uint32_t lines() const;
    bool     present(uint32_t line) const;
    uint32_t descriptors() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count);

private:
    bool fields_valid() const;

    uint32_t lines_;
    uint64_t present_;
};

}

#endif
