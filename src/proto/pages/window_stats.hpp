#ifndef AV_WINDOW_STATS_HPP
#define AV_WINDOW_STATS_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WindowStats {
public:
    static constexpr size_t   page_size = 4096u;
    static constexpr size_t   encoded_size = 64u;
    static constexpr size_t   sequence_at = 8u;
    static constexpr size_t   fields_at = 16u;
    static constexpr uint32_t field_count = 6u;

    WindowStats();
    WindowStats(uint64_t total_pages, uint64_t pages_in_use, uint64_t peak_pages_in_use, uint64_t refusals,
                uint64_t refused_bytes, uint64_t largest_free_run_at_refusal);

    uint64_t total_pages() const;
    uint64_t pages_in_use() const;
    uint64_t peak_pages_in_use() const;
    uint64_t refusals() const;
    uint64_t refused_bytes() const;
    uint64_t largest_free_run_at_refusal() const;
    uint64_t field(uint32_t index) const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t magic = 0x00535057u;
    static constexpr uint32_t version = 1u;

    uint64_t fields_[field_count];
};

}

#endif
