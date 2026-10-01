/* The hello QEMU sends a new av-renderer, fed attacker-chosen bytes: the
 * first byte is how many descriptors arrived beside it and the rest is its
 * body. What decodes must re-encode to exactly the bytes read and name a path
 * dlopen reads to its end. */
#include "core/support/status.hpp"
#include "proto/renderer/hello_message.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    if (size == 0u) return 0;
    av::HelloMessage hello;
    if (hello.decode(data + 1u, size - 1u, data[0]) != av::Status::ok) return 0;
    static std::uint8_t again[av::HelloMessage::fixed_size + av::HelloMessage::max_path_length +
                              av::HelloMessage::max_capset_length];
    if (hello.encoded_length() != size - 1u || hello.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
    if (std::memcmp(again, data + 1u, size - 1u) != 0) __builtin_trap();
    if (hello.executor_path_length() == 0u || hello.capset_length() == 0u) __builtin_trap();
    for (std::uint32_t index = 0u; index < hello.executor_path_length(); index++)
        if (hello.executor_path()[index] == '\0') __builtin_trap();
    return 0;
}
