/* The bind QEMU sends rvk-renderer for a context, fed attacker-chosen bytes:
 * the first byte is how many descriptors arrived beside it and the rest is its
 * body. What decodes must re-encode to exactly the bytes read, and a doorbell
 * must come with exactly its one eventfd. */
#include "core/support/status.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/renderer/bind_context_message.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    if (size == 0u) return 0;
    av::BindContextMessage bind;
    if (bind.decode(data + 1u, size - 1u, data[0]) != av::Status::ok) return 0;
    std::uint8_t again[av::BindContextMessage::fixed_size + av::ContextSetup::encoded_size] = {};
    if (bind.encoded_length() != size - 1u || bind.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
    if (std::memcmp(again, data + 1u, size - 1u) != 0) __builtin_trap();
    if (bind.context() == 0u || data[0] != (bind.has_doorbell() ? 1u : 0u)) __builtin_trap();
    if (!bind.has_doorbell() && bind.doorbell() != 0u) __builtin_trap();
    return 0;
}
