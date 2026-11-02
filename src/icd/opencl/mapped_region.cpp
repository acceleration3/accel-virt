#include "icd/opencl/mapped_region.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

MappedRegion::MappedRegion() : host_(0u), origin_{}, region_{}, element_bytes_(0u), memory_(Memory::shared)
{
    AV_SCOPE("MappedRegion::MappedRegion");
}

MappedRegion::MappedRegion(uint64_t buffer, uint64_t host, size_t offset, size_t bytes, cl_map_flags flags,
                           Memory memory)
    : StagedMapping(buffer, 0u, offset, bytes, static_cast<uint32_t>(flags)), host_(host), origin_{}, region_{},
      element_bytes_(0u), memory_(memory)
{
    AV_SCOPE("MappedRegion::MappedRegion");
}

MappedRegion::MappedRegion(uint64_t image, const size_t *origin, const size_t *region, size_t element_bytes,
                           size_t bytes, cl_map_flags flags)
    : StagedMapping(image, 0u, 0u, bytes, static_cast<uint32_t>(flags)), host_(0u), origin_{}, region_{},
      element_bytes_(element_bytes), memory_(Memory::staged)
{
    AV_SCOPE("MappedRegion::MappedRegion");
    for (size_t axis = 0u; axis < 3u; ++axis) {
        origin_[axis] = origin[axis];
        region_[axis] = region[axis];
    }
}

uint64_t MappedRegion::host() const
{
    AV_SCOPE("MappedRegion::host");
    return host_;
}

bool MappedRegion::image() const
{
    AV_SCOPE("MappedRegion::image");
    return element_bytes_ != 0u;
}

ClImageRegion MappedRegion::image_layout() const
{
    AV_SCOPE("MappedRegion::image_layout");
    return ClImageRegion(origin_, region_, 0u, 0u, element_bytes_);
}

bool MappedRegion::writes_back() const
{
    AV_SCOPE("MappedRegion::writes_back");
    return memory_ != Memory::shared && (flags() & (CL_MAP_WRITE | CL_MAP_WRITE_INVALIDATE_REGION)) != 0u;
}

bool MappedRegion::invariants_hold() const
{
    AV_SCOPE("MappedRegion::invariants_hold");
    if (!StagedMapping::invariants_hold()) return false;
    if (at() == nullptr) return true;
    if (bytes() == 0u || object() == 0u || copied() != (memory_ == Memory::staged)) return false;
    if (element_bytes_ != 0u) return host_ == 0u && memory_ == Memory::staged;
    return host_ != 0u;
}

}
