#ifndef AV_CLOCK_PAGE_HEADER_HPP
#define AV_CLOCK_PAGE_HEADER_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ClockPageHeader {
public:
    static constexpr size_t   page_size = 4096u;
    static constexpr size_t   encoded_size = 32u;
    static constexpr size_t   record_size = 64u;
    static constexpr uint32_t max_heads = 16u;

    ClockPageHeader();
    explicit ClockPageHeader(uint32_t heads);

    uint32_t heads() const;
    size_t   record_offset(uint32_t head) const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t magic = 0x00435641u;
    static constexpr uint32_t version = 1u;
    static constexpr size_t   reserved_bytes = 20u;

    uint32_t heads_;
};

}

#endif
