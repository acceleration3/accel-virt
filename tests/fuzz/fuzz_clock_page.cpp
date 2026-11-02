#include "core/support/status.hpp"
#include "proto/pages/clock_page_header.hpp"
#include "proto/pages/vblank_record.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::ClockPageHeader header(7u);
    if (header.decode(data, size) == av::Status::ok) {
        if (header.heads() == 0u || header.heads() > av::ClockPageHeader::max_heads) __builtin_trap();
        std::uint8_t again[av::ClockPageHeader::encoded_size] = {};
        if (header.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
        if (std::memcmp(again, data, sizeof again) != 0) __builtin_trap();
    } else if (header.heads() != 7u) {
        __builtin_trap();
    }
    av::VblankRecord record(1u, 2u, 3u, 60000u);
    if (record.decode(data, size) != av::Status::ok) {
        if (record.latched() != 1u || record.sequence() != 2u || record.vblank_ns() != 3u) __builtin_trap();
        return 0;
    }
    if (record.refresh_millihz() < av::VblankRecord::least_millihz ||
        record.refresh_millihz() > av::VblankRecord::most_millihz ||
        record.period_ns() != av::VblankRecord::nanoseconds_times_millihz / record.refresh_millihz())
        __builtin_trap();
    std::uint8_t again[av::VblankRecord::encoded_size] = {};
    if (record.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
    if (std::memcmp(again, data, sizeof again) != 0) __builtin_trap();
    return 0;
}
