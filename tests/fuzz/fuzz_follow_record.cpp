/* The private data of a packet on an ordinary context, fed attacker-chosen
 * bytes: Adapter::submit_command_virtual decodes it for every such packet, and
 * a process writes it. What decodes must re-encode to exactly the bytes read
 * and name a ring and a fence. */
#include "core/support/status.hpp"
#include "proto/escape/follow_record.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::FollowRecord record;
    if (record.decode(data, size) != av::Status::ok) return 0;
    std::uint8_t again[av::FollowRecord::encoded_size] = {};
    if (record.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
    if (std::memcmp(again, data, sizeof again) != 0) __builtin_trap();
    if (record.ring() == 0u || record.fence() == 0u) __builtin_trap();
    return 0;
}
