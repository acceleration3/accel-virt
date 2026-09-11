#include "core/display/display_sources.hpp"

namespace av {

DisplaySources::DisplaySources(Platform &platform)
    : platform_(platform), private_formats_{}, committed_{}, vsync_enabled_(false),
      scanout_addresses_{}, visible_{} {}

Status DisplaySources::set_private_format(uint32_t source, uint32_t attribute)
{
    platform_.require_irql_at_most(Irql::passive, "DisplaySources::set_private_format");
    if (source >= source_count) return Status::invalid_parameter;
    private_formats_[source] = attribute;
    return Status::ok;
}

uint32_t DisplaySources::private_format(uint32_t source) const
{
    platform_.require_irql_at_most(Irql::apc, "DisplaySources::private_format");
    return source < source_count ? private_formats_[source] : 0u;
}

Status DisplaySources::commit(uint32_t source, const DisplayMode *mode)
{
    platform_.require_irql_at_most(Irql::passive, "DisplaySources::commit");
    if (source >= source_count) return Status::invalid_parameter;
    committed_[source] = mode;
    return Status::ok;
}

const DisplayMode *DisplaySources::committed_mode(uint32_t source) const
{
    platform_.require_irql_at_most(Irql::apc, "DisplaySources::committed_mode");
    return source < source_count ? committed_[source] : nullptr;
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

Status DisplaySources::set_scanout(uint32_t source, int64_t address)
{
    platform_.require_irql_at_most(Irql::isr, "DisplaySources::set_scanout");
    if (source >= source_count) return Status::invalid_parameter;
    platform_.store_atomic(scanout_addresses_[source], static_cast<uint64_t>(address));
    return Status::ok;
}

uint64_t DisplaySources::scanout_address(uint32_t source)
{
    platform_.require_irql_at_most(Irql::isr, "DisplaySources::scanout_address");
    return source < source_count ? platform_.load_atomic(scanout_addresses_[source]) : 0u;
}

Status DisplaySources::set_visible(uint32_t source, bool visible)
{
    platform_.require_irql_at_most(Irql::passive, "DisplaySources::set_visible");
    if (source >= source_count) return Status::invalid_parameter;
    visible_[source] = visible;
    return Status::ok;
}

bool DisplaySources::visible(uint32_t source) const
{
    platform_.require_irql_at_most(Irql::apc, "DisplaySources::visible");
    return source < source_count && visible_[source];
}

}  // namespace av
