#include "miniport/display/display_info_response.hpp"
#include "core/support/status.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

std::uint32_t field(const std::uint8_t *data, std::size_t offset)
{
    std::uint32_t value = 0u;
    std::memcpy(&value, data + offset, sizeof value);
    return value;
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::DisplayInfoResponse info;
    if (info.decode(data, size) != av::Status::ok) {
        if (info.enabled_count() != 0u || !info.invariants_hold()) __builtin_trap();
        return 0;
    }
    if (size < av::DisplayInfoResponse::encoded_size) __builtin_trap();
    if (field(data, 0u) != av::DisplayInfoResponse::response_type) __builtin_trap();
    std::uint32_t enabled = 0u;
    for (std::uint32_t scanout = 0u; scanout < av::DisplayInfoResponse::max_scanouts; scanout++) {
        const std::size_t at = av::DisplayInfoResponse::header_size +
                               scanout * av::DisplayInfoResponse::scanout_size;
        const bool on = field(data, at + 16u) != 0u;
        if (info.enabled(scanout) != on) __builtin_trap();
        enabled += on ? 1u : 0u;
        if (!on) {
            if (info.width(scanout) != 0u || info.height(scanout) != 0u) __builtin_trap();
            continue;
        }
        if (info.width(scanout) != field(data, at + 8u)) __builtin_trap();
        if (info.height(scanout) != field(data, at + 12u)) __builtin_trap();
        if (info.width(scanout) == 0u || info.width(scanout) > av::DisplayInfoResponse::max_dimension) __builtin_trap();
        if (info.height(scanout) == 0u || info.height(scanout) > av::DisplayInfoResponse::max_dimension)
            __builtin_trap();
    }
    if (info.enabled_count() != enabled || !info.invariants_hold()) __builtin_trap();
    return 0;
}
