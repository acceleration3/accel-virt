#include "icd/d3d/write_maps.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d {

WriteMaps::WriteMaps() : resources_{}, subresources_{}, flags_{}, count_(0u)
{
    AV_SCOPE("WriteMaps::WriteMaps");
}

uint32_t WriteMaps::row_of(uint32_t resource, uint32_t subresource) const
{
    AV_SCOPE("WriteMaps::row_of");
    for (uint32_t row = 0u; row < count_; row++)
        if (resources_[row] == resource && subresources_[row] == subresource) return row;
    return capacity;
}

bool WriteMaps::open(uint32_t resource, uint32_t subresource, uint32_t flags)
{
    AV_SCOPE("WriteMaps::open");
    if (resource == 0u) return false;
    uint32_t row = row_of(resource, subresource);
    if (row == capacity) {
        if (count_ == capacity) return false;
        row = count_++;
        resources_[row] = resource;
        subresources_[row] = subresource;
    }
    flags_[row] = flags;
    return true;
}

void WriteMaps::drop(uint32_t row)
{
    AV_SCOPE("WriteMaps::drop");
    count_--;
    resources_[row] = resources_[count_];
    subresources_[row] = subresources_[count_];
    flags_[row] = flags_[count_];
    resources_[count_] = 0u;
    subresources_[count_] = 0u;
    flags_[count_] = 0u;
}

bool WriteMaps::close(uint32_t resource, uint32_t subresource, uint32_t &flags)
{
    AV_SCOPE("WriteMaps::close");
    flags = 0u;
    const uint32_t row = row_of(resource, subresource);
    if (row == capacity) return false;
    flags = flags_[row];
    drop(row);
    return true;
}

void WriteMaps::forget(uint32_t resource)
{
    AV_SCOPE("WriteMaps::forget");
    for (uint32_t row = 0u; row < count_;) {
        if (resources_[row] == resource) drop(row);
        else row++;
    }
}

uint32_t WriteMaps::count() const
{
    AV_SCOPE("WriteMaps::count");
    return count_;
}

bool WriteMaps::invariants_hold() const
{
    AV_SCOPE("WriteMaps::invariants_hold");
    if (count_ > capacity) return false;
    for (uint32_t row = 0u; row < capacity; row++) {
        if (row >= count_) {
            if (resources_[row] != 0u || subresources_[row] != 0u || flags_[row] != 0u) return false;
            continue;
        }
        if (resources_[row] == 0u) return false;
        for (uint32_t earlier = 0u; earlier < row; earlier++)
            if (resources_[earlier] == resources_[row] && subresources_[earlier] == subresources_[row]) return false;
    }
    return true;
}

}  // namespace av::d3d
