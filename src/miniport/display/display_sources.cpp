#include "miniport/display/display_sources.hpp"

#include "miniport/ddi/dxgk_multiplane_overlay_plane_record.hpp"
#include "miniport/ddi/dxgk_multiplane_overlay_vsync_info_record.hpp"
#include "miniport/display/display_mode.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av {

DisplaySources::DisplaySources(Platform &platform)
    : platform_(platform), count_(1u), private_formats_{}, committed_{}, vsync_enabled_(false),
      scanout_addresses_{}, primaries_{}, visible_{}, flips_{}, frames_{}, numbered_frames_{}, numbered_flips_{}, base_attributes_{},
      planes_(), last_reported_(0u), programmed_(platform), reported_(platform)
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

Status DisplaySources::present_planes(uint32_t source, const OverlayPlane *planes, uint32_t count)
{
    AV_SCOPE("DisplaySources::present_planes");
    platform_.require_irql_at_most(Irql::isr, "DisplaySources::present_planes");
    if (source >= count_ || (count != 0u && planes == nullptr)) return Status::invalid_parameter;
    for (uint32_t index = 0u; index < count; index++)
        if (planes[index].layer() >= OverlayPlane::max_planes) return Status::invalid_parameter;
    for (uint32_t index = 0u; index < count; index++) planes_[source][planes[index].layer()] = planes[index];
    const OverlayPlane &base = planes_[source][0];
    platform_.store_atomic(primaries_[source], reinterpret_cast<uintptr_t>(base.allocation()));
    platform_.store_atomic(scanout_addresses_[source], static_cast<uint64_t>(base.address()));
    platform_.store_atomic(flips_[source], static_cast<uint64_t>(Flip::presented_plane));
    number(source, FlipView(planes_[source]));
    return Status::ok;
}

void DisplaySources::planes(uint32_t source, OverlayPlane *planes)
{
    AV_SCOPE("DisplaySources::planes");
    platform_.require_irql_at_most(Irql::dispatch, "DisplaySources::planes");
    for (uint32_t layer = 0u; layer < OverlayPlane::max_planes; layer++)
        planes[layer] = source < count_ ? planes_[source][layer] : OverlayPlane();
}

DisplaySources::Flip DisplaySources::flip(uint32_t source)
{
    AV_SCOPE("DisplaySources::flip");
    platform_.require_irql_at_most(Irql::isr, "DisplaySources::flip");
    if (source >= count_) return Flip::address;
    return static_cast<Flip>(platform_.load_atomic(flips_[source]));
}

void DisplaySources::number(uint32_t source, const FlipView &flip)
{
    AV_SCOPE("DisplaySources::number");
    const uint64_t frame = platform_.load_atomic(frames_[source]) % 0xFFFFFFFFu + 1u;
    const uint32_t slot = static_cast<uint32_t>(frame % remembered_flips);
    platform_.store_atomic(numbered_frames_[source][slot], 0u);
    numbered_flips_[source][slot] = flip;
    platform_.store_atomic(numbered_frames_[source][slot], frame);
    platform_.store_atomic(frames_[source], frame);
    programmed_.note(flip.plane(0u).present_id(), frame, static_cast<uint64_t>(flip.kind()));
}

void DisplaySources::note_reported(const FlipView &shown, uint64_t sequence, uint64_t blank_ns)
{
    AV_SCOPE("DisplaySources::note_reported");
    platform_.require_irql_at_most(Irql::dispatch, "DisplaySources::note_reported");
    const uint64_t id = shown.plane(0u).present_id();
    if (id == platform_.load_atomic(last_reported_)) return;
    platform_.store_atomic(last_reported_, id);
    reported_.note(id, sequence, blank_ns);
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
    if (kind == Flip::presented_plane) return FlipView(planes_[source]);
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
        for (OverlayPlane &plane : planes_[source])
            if (plane.allocation() == primary) plane = OverlayPlane(plane.layer(), plane.present_id());
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
    programmed_.report(writer, "programmed");
    reported_.report(writer, "reported");
    for (uint32_t source = 0u; source < count_; source++) {
        writer.open("source", source);
        writer.number("format", private_formats_[source]);
        writer.number("width", committed_[source] != nullptr ? committed_[source]->width() : 0u);
        writer.number("height", committed_[source] != nullptr ? committed_[source]->height() : 0u);
        writer.hex("scanout", platform_.load_atomic(scanout_addresses_[source]));
        writer.hex("primary", platform_.load_atomic(primaries_[source]));
        writer.number("flip", platform_.load_atomic(flips_[source]));
        writer.flag("visible", platform_.load_atomic(visible_[source]) != 0u);
        for (const OverlayPlane &plane : planes_[source]) {
            if (!plane.flipped()) continue;
            writer.open("plane", plane.layer());
            writer.number("present_id", plane.present_id());
            writer.flag("shown", plane.shown());
            writer.address("allocation", plane.allocation());
            const RectRecord &into = plane.destination();
            writer.number("left", static_cast<uint32_t>(into.left));
            writer.number("top", static_cast<uint32_t>(into.top));
            writer.number("right", static_cast<uint32_t>(into.right));
            writer.number("bottom", static_cast<uint32_t>(into.bottom));
            writer.flag("blended", plane.blended());
            writer.close();
        }
        writer.close();
    }
    writer.close();
}

bool DisplaySources::invariants_hold() const
{
    AV_SCOPE("DisplaySources::invariants_hold");
    return count_ != 0u && count_ <= max_sources;
}

}
