/* The header of every datagram QEMU and rvk-renderer send each other, fed
 * attacker-chosen bytes: a renderer running guest shaders may be compromised,
 * so QEMU reads what it sends as hostile. What decodes must re-encode to the
 * bytes read and describe exactly the body that arrived. */
#include "core/support/status.hpp"
#include "proto/renderer/renderer_message_header.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::RendererMessageHeader header;
    if (header.decode(data, size) != av::Status::ok) return 0;
    std::uint8_t again[av::RendererMessageHeader::encoded_size] = {};
    if (header.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
    if (std::memcmp(again, data, sizeof again) != 0) __builtin_trap();
    if (size - av::RendererMessageHeader::encoded_size != header.body_length()) __builtin_trap();
    if (header.body_length() > av::RendererMessageHeader::max_body_length) __builtin_trap();
    return 0;
}
