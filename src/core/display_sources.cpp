#include "display_sources.hpp"

namespace av {

DisplaySources::DisplaySources(Platform &platform) : platform_(platform), private_formats_{} {}

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

}  // namespace av
