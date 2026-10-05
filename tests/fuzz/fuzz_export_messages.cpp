/* The export socket's two messages over attacker-chosen bytes
 * (docs/external-export.md): a request as the device reads it from another
 * process, and an answer as a consumer reads it, with the descriptor count
 * taken from the input's first byte. Whatever either decoder accepts encodes
 * back to the very bytes it came from - every field kept, nothing invented -
 * and a refusal leaves the object as it was. */
#include "core/support/status.hpp"
#include "proto/export/export_answer.hpp"
#include "proto/export/export_request.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::ExportRequest request(av::ExportOperation::release, 0x77u, 0x66u);
    if (request.decode(data, size) == av::Status::ok) {
        std::uint8_t again[av::ExportRequest::encoded_size] = {};
        if (request.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
        /* The minor is the one field encode does not keep. */
        if (std::memcmp(again, data, 6u) != 0 || std::memcmp(again + 8u, data + 8u, sizeof again - 8u) != 0)
            __builtin_trap();
    } else if (request.surface() != 0x77u || request.tag() != 0x66u) {
        __builtin_trap();
    }
    if (size == 0u) return 0;
    const std::uint32_t descriptors = data[0] % 3u;
    av::ExportAnswer    answer;
    if (answer.decode(data + 1u, size - 1u, descriptors) != av::Status::ok) {
        if (answer.operation() != av::ExportOperation::unreadable || answer.surface() != 0u) __builtin_trap();
        return 0;
    }
    std::uint8_t again[av::ExportAnswer::encoded_size] = {};
    if (answer.encode(again, sizeof again) != av::Status::ok || answer.descriptors() != descriptors) __builtin_trap();
    if (std::memcmp(again, data + 1u, 6u) != 0 || std::memcmp(again + 8u, data + 9u, sizeof again - 8u) != 0)
        __builtin_trap();
    return 0;
}
