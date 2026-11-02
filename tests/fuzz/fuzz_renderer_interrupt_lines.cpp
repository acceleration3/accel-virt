#include "core/support/status.hpp"
#include "proto/renderer/interrupt_lines_message.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    if (size == 0u) return 0;
    av::InterruptLinesMessage message;
    if (message.decode(data + 1u, size - 1u, data[0]) != av::Status::ok) return 0;
    std::uint8_t again[av::InterruptLinesMessage::encoded_size] = {};
    if (message.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
    if (size - 1u != sizeof again || std::memcmp(again, data + 1u, sizeof again) != 0) __builtin_trap();
    if (message.lines() == 0u || data[0] != message.descriptors()) __builtin_trap();
    return 0;
}
