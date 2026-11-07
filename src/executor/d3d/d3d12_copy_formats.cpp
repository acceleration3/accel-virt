#include "executor/d3d/d3d12_copy_formats.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

D3d12CopyFormats::D3d12CopyFormats(uint32_t destination, uint32_t source) : destination_(destination), source_(source)
{
    AV_SCOPE("D3d12CopyFormats::D3d12CopyFormats");
}

bool D3d12CopyFormats::same_texels() const
{
    AV_SCOPE("D3d12CopyFormats::same_texels");
    return destination_.known() && source_.known() && destination_.element_bytes() != 0u &&
           destination_.element_bytes() == source_.element_bytes() &&
           destination_.element_width() == source_.element_width() &&
           destination_.element_height() == source_.element_height();
}

bool D3d12CopyFormats::plane_of_destination() const
{
    AV_SCOPE("D3d12CopyFormats::plane_of_destination");
    return destination_.known() && source_.known() && destination_.depth() && source_.element_bytes() != 0u &&
           source_.element_bytes() <= destination_.element_bytes() && source_.element_width() == 1u &&
           source_.element_height() == 1u;
}

}
