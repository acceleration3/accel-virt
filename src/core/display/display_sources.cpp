#include "core/display/display_sources.hpp"

namespace av {

DisplaySources::DisplaySources(Platform &platform)
    : platform_(platform), count_(1u), private_formats_{}, committed_{}, vsync_enabled_(false),
      scanout_addresses_{}, primaries_{}, visible_{} {}

Status DisplaySources::set_count(uint32_t outputs)
{
    platform_.require_irql_at_most(Irql::passive, "DisplaySources::set_count");
    if (outputs == 0u) return Status::invalid_parameter;
    count_ = outputs < max_sources ? outputs : max_sources;
    return outputs <= max_sources ? Status::ok : Status::too_large;
}

uint32_t DisplaySources::count() const { return count_; }

Status DisplaySources::set_private_format(uint32_t source, uint32_t attribute)
{
    platform_.require_irql_at_most(Irql::passive, "DisplaySources::set_private_format");
    if (source >= count_) return Status::invalid_parameter;
    private_formats_[source] = attribute;
    return Status::ok;
}

uint32_t DisplaySources::private_format(uint32_t source) const
{
    platform_.require_irql_at_most(Irql::apc, "DisplaySources::private_format");
    return source < count_ ? private_formats_[source] : 0u;
}

Status DisplaySources::commit(uint32_t source, const DisplayMode *mode)
{
    platform_.require_irql_at_most(Irql::passive, "DisplaySources::commit");
    if (source >= count_) return Status::invalid_parameter;
    committed_[source] = mode;
    return Status::ok;
}

const DisplayMode *DisplaySources::committed_mode(uint32_t source) const
{
    platform_.require_irql_at_most(Irql::apc, "DisplaySources::committed_mode");
    return source < count_ ? committed_[source] : nullptr;
}

void DisplaySources::set_vsync(bool enabled)
{
    platform_.require_irql_at_most(Irql::passive, "DisplaySources::set_vsync");
    vsync_enabled_ = enabled;
}

/* The vsync the scan-out timer raises reads it at DISPATCH. */
bool DisplaySources::vsync_enabled() const
{
    platform_.require_irql_at_most(Irql::dispatch, "DisplaySources::vsync_enabled");
    return vsync_enabled_;
}

Status DisplaySources::set_scanout(uint32_t source, int64_t address, void *primary)
{
    platform_.require_irql_at_most(Irql::isr, "DisplaySources::set_scanout");
    if (source >= count_) return Status::invalid_parameter;
    platform_.store_atomic(primaries_[source], reinterpret_cast<uintptr_t>(primary));
    platform_.store_atomic(scanout_addresses_[source], static_cast<uint64_t>(address));
    return Status::ok;
}

void *DisplaySources::primary(uint32_t source)
{
    platform_.require_irql_at_most(Irql::dispatch, "DisplaySources::primary");
    if (source >= count_) return nullptr;
    return reinterpret_cast<void *>(static_cast<uintptr_t>(platform_.load_atomic(primaries_[source])));
}

void DisplaySources::forget_primary(void *primary)
{
    platform_.require_irql_at_most(Irql::passive, "DisplaySources::forget_primary");
    for (uint32_t source = 0u; source < count_; source++) {
        if (platform_.load_atomic(primaries_[source]) != reinterpret_cast<uintptr_t>(primary)) continue;
        platform_.store_atomic(scanout_addresses_[source], 0u);
        platform_.store_atomic(primaries_[source], 0u);
    }
}

uint64_t DisplaySources::scanout_address(uint32_t source)
{
    platform_.require_irql_at_most(Irql::isr, "DisplaySources::scanout_address");
    return source < count_ ? platform_.load_atomic(scanout_addresses_[source]) : 0u;
}

Status DisplaySources::set_visible(uint32_t source, bool visible)
{
    platform_.require_irql_at_most(Irql::passive, "DisplaySources::set_visible");
    if (source >= count_) return Status::invalid_parameter;
    platform_.store_atomic(visible_[source], visible ? 1u : 0u);
    return Status::ok;
}

bool DisplaySources::visible(uint32_t source)
{
    platform_.require_irql_at_most(Irql::dispatch, "DisplaySources::visible");
    return source < count_ && platform_.load_atomic(visible_[source]) != 0u;
}

bool DisplaySources::invariants_hold() const { return count_ != 0u && count_ <= max_sources; }

}  // namespace av
