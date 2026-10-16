#include "icd/d3d/write_maps.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d {

WriteMaps::WriteMaps(BlockSource &memory) : flags_(memory, Pool::paged)
{
    AV_SCOPE("WriteMaps::WriteMaps");
}

uint64_t WriteMaps::key_of(uint32_t resource, uint32_t subresource) const
{
    AV_SCOPE("WriteMaps::key_of");
    return uint64_t{resource} << 32u | subresource;
}

Status WriteMaps::open(uint32_t resource, uint32_t subresource, uint32_t flags)
{
    AV_SCOPE("WriteMaps::open");
    if (resource == 0u) return Status::invalid_parameter;
    uint32_t *const open = flags_.find(key_of(resource, subresource));
    uint32_t        slot = 0u;
    if (open == nullptr) return flags_.insert(key_of(resource, subresource), flags, slot);
    *open = flags;
    return Status::ok;
}

bool WriteMaps::close(uint32_t resource, uint32_t subresource, uint32_t &flags)
{
    AV_SCOPE("WriteMaps::close");
    flags = 0u;
    return flags_.take(key_of(resource, subresource), flags);
}

void WriteMaps::forget(uint32_t resource)
{
    AV_SCOPE("WriteMaps::forget");
    flags_.erase_if([resource](uint64_t key, uint32_t &) { return key >> 32u == resource; });
}

uint32_t WriteMaps::count() const
{
    AV_SCOPE("WriteMaps::count");
    return flags_.count();
}

bool WriteMaps::invariants_hold() const
{
    AV_SCOPE("WriteMaps::invariants_hold");
    bool named = true;
    flags_.for_each([&named](uint64_t key, const uint32_t &) { named = named && key >> 32u != 0u; });
    return named && flags_.invariants_hold();
}

}  // namespace av::d3d
