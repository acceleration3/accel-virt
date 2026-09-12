#include "core/display/display_info_response.hpp"

#include "proto/wire/wire_reader.hpp"

namespace av {

DisplayInfoResponse::DisplayInfoResponse() : widths_{}, heights_{}, enabled_(0u) {}

Status DisplayInfoResponse::read_scanout(WireReader &reader, uint32_t index, uint32_t *widths, uint32_t *heights,
                                        uint64_t &enabled) const
{
    const uint32_t x = reader.read_u32();
    const uint32_t y = reader.read_u32();
    const uint32_t width = reader.read_u32();
    const uint32_t height = reader.read_u32();
    const uint32_t on = reader.read_u32();
    static_cast<void>(reader.read_u32());   /* flags: the device's own */
    if (reader.status() != Status::ok) return Status::malformed;
    if (on > 1u) return Status::malformed;
    if (on == 0u) return Status::ok;
    if (width == 0u || height == 0u || width > max_dimension || height > max_dimension) return Status::malformed;
    if (x > max_dimension || y > max_dimension) return Status::malformed;
    widths[index] = width;
    heights[index] = height;
    enabled |= uint64_t{1} << index;
    return Status::ok;
}

Status DisplayInfoResponse::decode(const uint8_t *bytes, size_t length)
{
    WireReader reader(bytes, length);
    if (reader.read_u32() != response_type) return Status::malformed;
    for (size_t skipped = 4u; skipped < header_size; skipped += 4u) static_cast<void>(reader.read_u32());
    uint32_t widths[max_scanouts] = {};
    uint32_t heights[max_scanouts] = {};
    uint64_t enabled = 0u;
    for (uint32_t index = 0u; index < max_scanouts; index++) {
        const Status status = read_scanout(reader, index, widths, heights, enabled);
        if (status != Status::ok) return status;
    }
    for (uint32_t index = 0u; index < max_scanouts; index++) {
        widths_[index] = widths[index];
        heights_[index] = heights[index];
    }
    enabled_ = enabled;
    return Status::ok;
}

uint32_t DisplayInfoResponse::enabled_count() const
{
    return static_cast<uint32_t>(__builtin_popcountll(enabled_));
}

bool DisplayInfoResponse::enabled(uint32_t scanout) const
{
    return scanout < max_scanouts && (enabled_ & uint64_t{1} << scanout) != 0u;
}

uint32_t DisplayInfoResponse::width(uint32_t scanout) const
{
    return scanout < max_scanouts ? widths_[scanout] : 0u;
}

uint32_t DisplayInfoResponse::height(uint32_t scanout) const
{
    return scanout < max_scanouts ? heights_[scanout] : 0u;
}

bool DisplayInfoResponse::invariants_hold() const
{
    for (uint32_t index = 0u; index < max_scanouts; index++)
        if (enabled(index) != (widths_[index] != 0u && heights_[index] != 0u)) return false;
    return enabled_ >> max_scanouts == 0u;
}

}  // namespace av
