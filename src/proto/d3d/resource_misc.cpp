#include "proto/d3d/resource_misc.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ResourceMisc::ResourceMisc(uint32_t flags) : flags_(flags)
{
    AV_SCOPE("ResourceMisc::ResourceMisc");
}

bool ResourceMisc::shared() const
{
    AV_SCOPE("ResourceMisc::shared");
    return (flags_ & shared_bit) != 0u;
}

bool ResourceMisc::rows() const
{
    AV_SCOPE("ResourceMisc::rows");
    return shared() && (flags_ & rows_bit) != 0u;
}

}  // namespace av
