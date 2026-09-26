/* The escape a process sends to publish its own ring's tail, fed
 * attacker-chosen bytes: every escape's private data is tried as one. What
 * decodes must be exactly the record the encoder writes. */
#include "core/support/status.hpp"
#include "proto/context/tail_publish_record.hpp"

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
