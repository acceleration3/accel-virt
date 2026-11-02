#include "core/support/status.hpp"
#include "proto/escape/tail_publish_record.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::TailPublishRecord record;
    if (record.decode(data, size) != av::Status::ok) return 0;
    std::uint8_t again[av::TailPublishRecord::encoded_size] = {};
    if (record.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
    if (size != sizeof again || std::memcmp(again, data, sizeof again) != 0) __builtin_trap();
    return 0;
}
