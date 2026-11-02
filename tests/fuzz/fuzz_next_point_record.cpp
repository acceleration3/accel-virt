#include "core/support/status.hpp"
#include "proto/escape/next_point_record.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::NextPointRecord record;
    if (record.decode(data, size) != av::Status::ok) return 0;
    std::uint8_t again[av::NextPointRecord::encoded_size] = {};
    if (record.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
    if (std::memcmp(again, data, sizeof again) != 0) __builtin_trap();
    if (record.surface() == 0u || record.point() == 0u) __builtin_trap();
    return 0;
}
