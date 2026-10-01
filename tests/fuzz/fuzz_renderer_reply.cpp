/* A reply between QEMU and av-renderer, fed attacker-chosen bytes: the first
 * byte is how many descriptors arrived beside it and the rest is its body.
 * What decodes must re-encode to exactly the bytes read and carry a status
 * av::Status has. */
#include "core/support/status.hpp"
#include "proto/renderer/reply_message.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    if (size == 0u) return 0;
    av::ReplyMessage reply;
    if (reply.decode(data + 1u, size - 1u, data[0]) != av::Status::ok) return 0;
    std::uint8_t again[av::ReplyMessage::fixed_size + av::ReplyMessage::max_payload_length] = {};
    if (reply.encoded_length() != size - 1u || reply.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
    if (std::memcmp(again, data + 1u, size - 1u) != 0) __builtin_trap();
    if (static_cast<std::uint32_t>(reply.status()) > static_cast<std::uint32_t>(av::Status::malformed)) __builtin_trap();
    return 0;
}
