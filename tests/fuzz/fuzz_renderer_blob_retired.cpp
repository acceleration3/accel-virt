/* A blob QEMU tells rvk-renderer to drop, fed attacker-chosen bytes: the first
 * byte is how many descriptors arrived beside the message and the rest is its
 * body. What decodes must re-encode to exactly the bytes read. */
#include "core/support/status.hpp"
#include "proto/renderer/blob_retired_message.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    if (size == 0u) return 0;
    av::BlobRetiredMessage message;
    if (message.decode(data + 1u, size - 1u, data[0]) != av::Status::ok) return 0;
    std::uint8_t again[av::BlobRetiredMessage::encoded_size] = {};
    if (message.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
    if (size - 1u != sizeof again || std::memcmp(again, data + 1u, sizeof again) != 0) __builtin_trap();
    if (message.blob() == 0u || data[0] != 0u) __builtin_trap();
    return 0;
}
