#include "core/support/bgra_rows.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

BgraRows::BgraRows(uint8_t *base, uint64_t bytes, uint64_t pitch)
    : base_(base), source_(base), bytes_(base == nullptr ? 0u : bytes), pitch_(pitch)
{
    AV_SCOPE("BgraRows::BgraRows");
}

BgraRows::BgraRows(const uint8_t *base, uint64_t bytes, uint64_t pitch)
    : base_(nullptr), source_(base), bytes_(base == nullptr ? 0u : bytes), pitch_(pitch)
{
    AV_SCOPE("BgraRows::BgraRows");
}

bool BgraRows::holds(uint32_t width, uint32_t height) const
{
    AV_SCOPE("BgraRows::holds");
    const uint64_t line = uint64_t{width} * sizeof(uint32_t);
    if (width == 0u || height == 0u || pitch_ < line || bytes_ < line) return false;
    return uint64_t{height - 1u} <= (bytes_ - line) / pitch_;
}

Status BgraRows::copy_from(const BgraRows &from, uint32_t width, uint32_t height, Channels channels)
{
    AV_SCOPE("BgraRows::copy_from");
    if (base_ == nullptr || !holds(width, height) || !from.holds(width, height)) return Status::invalid_parameter;
    const size_t line = static_cast<size_t>(width) * sizeof(uint32_t);
    for (uint32_t row = 0u; row < height; row++) {
        const uint8_t *const source = from.source_ + static_cast<size_t>(uint64_t{row} * from.pitch_);
        uint8_t *const destination = base_ + static_cast<size_t>(uint64_t{row} * pitch_);
        if (channels == Channels::unchanged) {
            __builtin_memmove(destination, source, line);
            continue;
        }
        for (size_t at = 0u; at < line; at += sizeof(uint32_t)) {
            uint32_t pixel = 0u;
            __builtin_memcpy(&pixel, source + at, sizeof pixel);
            pixel = (pixel & 0xFF00FF00u) | ((pixel >> 16u) & 0xFFu) | ((pixel & 0xFFu) << 16u);
            __builtin_memcpy(destination + at, &pixel, sizeof pixel);
        }
    }
    return Status::ok;
}

bool BgraRows::invariants_hold() const
{
    AV_SCOPE("BgraRows::invariants_hold");
    return (source_ != nullptr || bytes_ == 0u) && (base_ == nullptr || base_ == source_);
}

}
