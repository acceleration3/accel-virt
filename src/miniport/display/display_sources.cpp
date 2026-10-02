#include "miniport/display/display_sources.hpp"

#include "miniport/ddi/dxgk_multiplane_overlay_plane_record.hpp"
#include "miniport/ddi/dxgk_multiplane_overlay_vsync_info_record.hpp"
#include "miniport/display/display_mode.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av {

DisplaySources::DisplaySources(Platform &platform)
    : platform_(platform), count_(1u), private_formats_{}, committed_{}, vsync_enabled_(false),
      scanout_addresses_{}, primaries_{}, visible_{}, flips_{}, frames_{}, numbered_frames_{}, numbered_flips_{}, base_attributes_{}
{
    AV_SCOPE("DisplaySources::DisplaySources");
}

Status DisplaySources::set_count(uint32_t outputs)
{
    AV_SCOPE("DisplaySources::set_count");
    platform_.require_irql_at_most(Irql::passive, "DisplaySources::set_count");
    if (outputs == 0u) return Status::invalid_parameter;
    count_ = outputs < max_sources ? outputs : max_sources;
    return outputs <= max_sources ? Status::ok : Status::too_large;
}

uint32_t DisplaySources::count() const
{
    AV_SCOPE("DisplaySources::count");
    return count_;
}

Status DisplaySources::set_private_format(uint32_t source, uint32_t attribute)
{
    AV_SCOPE("DisplaySources::set_private_format");
    platform_.require_irql_at_most(Irql::passive, "DisplaySources::set_private_format");
    if (source >= count_) return Status::invalid_parameter;
    private_formats_[source] = attribute;
    return Status::ok;
}

uint32_t DisplaySources::private_format(uint32_t source) const
{
    AV_SCOPE("DisplaySources::private_format");
    platform_.require_irql_at_most(Irql::apc, "DisplaySources::private_format");
    return source < count_ ? private_formats_[source] : 0u;
}

Status DisplaySources::commit(uint32_t source, const DisplayMode *mode)
{
    AV_SCOPE("DisplaySources::commit");
    platform_.require_irql_at_most(Irql::passive, "DisplaySources::commit");
    if (source >= count_) return Status::invalid_parameter;
    committed_[source] = mode;
    return Status::ok;
}

const DisplayMode *DisplaySources::committed_mode(uint32_t source) const
{
    AV_SCOPE("DisplaySources::committed_mode");
    platform_.require_irql_at_most(Irql::apc, "DisplaySources::committed_mode");
    return source < count_ ? committed_[source] : nullptr;
}

void DisplaySources::set_vsync(bool enabled)
{
    AV_SCOPE("DisplaySources::set_vsync");
    platform_.require_irql_at_most(Irql::passive, "DisplaySources::set_vsync");
    vsync_enabled_ = enabled;
}

/* The vsync the scan-out timer raises reads it at DISPATCH. */
bool DisplaySources::vsync_enabled() const
{
    AV_SCOPE("DisplaySources::vsync_enabled");
    platform_.require_irql_at_most(Irql::dispatch, "DisplaySources::vsync_enabled");
    return vsync_enabled_;
}

Status DisplaySources::set_scanout(uint32_t source, int64_t address, void *primary)
{
    AV_SCOPE("DisplaySources::set_scanout");
    platform_.require_irql_at_most(Irql::isr, "DisplaySources::set_scanout");
    if (source >= count_) return Status::invalid_parameter;
    platform_.store_atomic(primaries_[source], reinterpret_cast<uintptr_t>(primary));
    platform_.store_atomic(scanout_addresses_[source], static_cast<uint64_t>(address));
    platform_.store_atomic(flips_[source], static_cast<uint64_t>(Flip::address));
    number(source, FlipView(Flip::address, static_cast<uint64_t>(address), nullptr));
    return Status::ok;
}

Status DisplaySources::set_multiplane_scanout(uint32_t source, const DxgkMultiplaneOverlayPlaneRecord *base)
{
    AV_SCOPE("DisplaySources::set_multiplane_scanout");
    platform_.require_irql_at_most(Irql::isr, "DisplaySources::set_multiplane_scanout");
    if (source >= count_) return Status::invalid_parameter;
    if (base == nullptr) {
        platform_.store_atomic(primaries_[source], 0u);
        platform_.store_atomic(scanout_addresses_[source], 0u);
        platform_.store_atomic(flips_[source], static_cast<uint64_t>(Flip::planes_disabled));
        number(source, FlipView(Flip::planes_disabled, 0u, nullptr));
        return Status::ok;
    }
    for (uint32_t word = 0u; word < multiplane_overlay_attribute_words; word++) base_attributes_[source][word] = base->PlaneAttributes[word];
    platform_.store_atomic(primaries_[source], reinterpret_cast<uintptr_t>(base->hAllocation));
    platform_.store_atomic(scanout_addresses_[source], static_cast<uint64_t>(base->AllocationAddress));
    platform_.store_atomic(flips_[source], static_cast<uint64_t>(Flip::base_plane));
    number(source, FlipView(Flip::base_plane, static_cast<uint64_t>(base->AllocationAddress), base->PlaneAttributes));
    return Status::ok;
}

DisplaySources::Flip DisplaySources::flip(uint32_t source)
{
    AV_SCOPE("DisplaySources::flip");
    platform_.require_irql_at_most(Irql::isr, "DisplaySources::flip");
    if (source >= count_) return Flip::address;
    return static_cast<Flip>(platform_.load_atomic(flips_[source]));
}

/* Frames are u32 on the wire and 0 is none, so the count wraps past it. */
void DisplaySources::number(uint32_t source, const FlipView &flip)
{
    AV_SCOPE("DisplaySources::number");
    const uint64_t frame = platform_.load_atomic(frames_[source]) % 0xFFFFFFFFu + 1u;
    const uint32_t slot = static_cast<uint32_t>(frame % remembered_flips);
    platform_.store_atomic(numbered_frames_[source][slot], 0u);
    numbered_flips_[source][slot] = flip;
    platform_.store_atomic(numbered_frames_[source][slot], frame);
    platform_.store_atomic(frames_[source], frame);
}

uint32_t DisplaySources::frame(uint32_t source)
{
    AV_SCOPE("DisplaySources::frame");
    platform_.require_irql_at_most(Irql::isr, "DisplaySources::frame");
    return source < count_ ? static_cast<uint32_t>(platform_.load_atomic(frames_[source])) : 0u;
}

FlipView DisplaySources::shown(uint32_t source, uint32_t frame)
{
    AV_SCOPE("DisplaySources::shown");
    platform_.require_irql_at_most(Irql::dispatch, "DisplaySources::shown");
    if (source >= count_) return FlipView();
    const uint32_t slot = frame % remembered_flips;
    if (frame != 0u && platform_.load_atomic(numbered_frames_[source][slot]) == frame) {
        const FlipView copy = numbered_flips_[source][slot];
        if (platform_.load_atomic(numbered_frames_[source][slot]) == frame) return copy;
    }
    const Flip kind = static_cast<Flip>(platform_.load_atomic(flips_[source]));
    return FlipView(kind, platform_.load_atomic(scanout_addresses_[source]),
                    kind == Flip::base_plane ? base_attributes_[source] : nullptr);
}

void *DisplaySources::primary(uint32_t source)
{
    AV_SCOPE("DisplaySources::primary");
    platform_.require_irql_at_most(Irql::dispatch, "DisplaySources::primary");
    if (source >= count_) return nullptr;
    return reinterpret_cast<void *>(static_cast<uintptr_t>(platform_.load_atomic(primaries_[source])));
}

void DisplaySources::forget_primary(void *primary)
{
    AV_SCOPE("DisplaySources::forget_primary");
    platform_.require_irql_at_most(Irql::passive, "DisplaySources::forget_primary");
    for (uint32_t source = 0u; source < count_; source++) {
        if (platform_.load_atomic(primaries_[source]) != reinterpret_cast<uintptr_t>(primary)) continue;
        platform_.store_atomic(scanout_addresses_[source], 0u);
        platform_.store_atomic(primaries_[source], 0u);
    }
}

uint64_t DisplaySources::scanout_address(uint32_t source)
{
    AV_SCOPE("DisplaySources::scanout_address");
    platform_.require_irql_at_most(Irql::isr, "DisplaySources::scanout_address");
    return source < count_ ? platform_.load_atomic(scanout_addresses_[source]) : 0u;
}

Status DisplaySources::set_visible(uint32_t source, bool visible)
{
    AV_SCOPE("DisplaySources::set_visible");
    platform_.require_irql_at_most(Irql::passive, "DisplaySources::set_visible");
    if (source >= count_) return Status::invalid_parameter;
    platform_.store_atomic(visible_[source], visible ? 1u : 0u);
    return Status::ok;
}

bool DisplaySources::visible(uint32_t source)
{
    AV_SCOPE("DisplaySources::visible");
    platform_.require_irql_at_most(Irql::dispatch, "DisplaySources::visible");
    return source < count_ && platform_.load_atomic(visible_[source]) != 0u;
}

void DisplaySources::report(StateWriter &writer)
{
    AV_SCOPE("DisplaySources::report");
    writer.open("display_sources", 0u);
    writer.number("count", count_);
    writer.flag("vsync", vsync_enabled_);
    for (uint32_t source = 0u; source < count_; source++) {
        writer.open("source", source);
        writer.number("format", private_formats_[source]);
        writer.number("width", committed_[source] != nullptr ? committed_[source]->width() : 0u);
        writer.number("height", committed_[source] != nullptr ? committed_[source]->height() : 0u);
        writer.hex("scanout", platform_.load_atomic(scanout_addresses_[source]));
        writer.hex("primary", platform_.load_atomic(primaries_[source]));
        writer.number("flip", platform_.load_atomic(flips_[source]));
        writer.flag("visible", platform_.load_atomic(visible_[source]) != 0u);
        writer.close();
    }
    writer.close();
}

bool DisplaySources::invariants_hold() const
{
    AV_SCOPE("DisplaySources::invariants_hold");
    return count_ != 0u && count_ <= max_sources;
}

}  // namespace av
