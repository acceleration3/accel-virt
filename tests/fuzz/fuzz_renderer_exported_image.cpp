#include "core/support/status.hpp"
#include "proto/renderer/exported_image_message.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    if (size == 0u) return 0;
    av::ExportedImageMessage message;
    if (message.decode(data + 1u, size - 1u, data[0]) != av::Status::ok) return 0;
    std::uint8_t again[av::ExportedImageMessage::encoded_size] = {};
    if (message.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
    if (size - 1u != sizeof again || std::memcmp(again, data + 1u, sizeof again) != 0) __builtin_trap();
    if (data[0] != message.descriptors()) __builtin_trap();
    if (message.found() && (message.slot() >= av::ExportedImageMessage::slots || message.width() == 0u)) __builtin_trap();
    if (!message.found() && message.fresh()) __builtin_trap();
    return 0;
}
