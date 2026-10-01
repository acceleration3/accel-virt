/* A shared object QEMU tells av-renderer the guest let go of, fed
 * attacker-chosen bytes: the first byte is how many descriptors arrived beside
 * the message and the rest is its body. What decodes must re-encode to exactly
 * the bytes read, with no descriptor beside it. */
#include "core/support/status.hpp"
#include "proto/renderer/shared_released_message.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    if (size == 0u) return 0;
    av::SharedReleasedMessage message;
    if (message.decode(data + 1u, size - 1u, data[0]) != av::Status::ok) return 0;
    std::uint8_t again[av::SharedReleasedMessage::encoded_size] = {};
    if (message.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
    if (size - 1u != sizeof again || std::memcmp(again, data + 1u, sizeof again) != 0) __builtin_trap();
    if (data[0] != 0u) __builtin_trap();
    return 0;
}
