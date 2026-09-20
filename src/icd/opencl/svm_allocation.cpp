#include "icd/opencl/svm_allocation.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

SvmAllocation::SvmAllocation() : host_base_(0u), mirror_(nullptr), bytes_(0u), map_depth_(0u)
{
    AV_SCOPE("SvmAllocation::SvmAllocation");
}

void SvmAllocation::take(uint64_t host_base, uint8_t *mirror, size_t bytes)
{
    AV_SCOPE("SvmAllocation::take");
    host_base_ = host_base;
    mirror_ = mirror;
    bytes_ = bytes;
    map_depth_ = 0u;
}

void SvmAllocation::give_up()
{
    AV_SCOPE("SvmAllocation::give_up");
    host_base_ = 0u;
    mirror_ = nullptr;
    bytes_ = 0u;
    map_depth_ = 0u;
}

bool SvmAllocation::held() const
{
    AV_SCOPE("SvmAllocation::held");
    return mirror_ != nullptr;
}

uint64_t SvmAllocation::host_base() const
{
    AV_SCOPE("SvmAllocation::host_base");
    return host_base_;
}

uint8_t *SvmAllocation::mirror() const
{
    AV_SCOPE("SvmAllocation::mirror");
    return mirror_;
}

size_t SvmAllocation::bytes() const
{
    AV_SCOPE("SvmAllocation::bytes");
    return bytes_;
}

bool SvmAllocation::covers(const void *address) const
{
    AV_SCOPE("SvmAllocation::covers");
    if (mirror_ == nullptr || address == nullptr) return false;
    const uint8_t *const at = static_cast<const uint8_t *>(address);
    /* The last byte is inside; one past the end is not, and is also the first
     * byte of whatever the allocator put next. */
    return at >= mirror_ && at < mirror_ + bytes_;
}

uint64_t SvmAllocation::host_address_of(const void *address) const
{
    AV_SCOPE("SvmAllocation::host_address_of");
    if (!covers(address)) return 0u;
    const uint8_t *const at = static_cast<const uint8_t *>(address);
    return host_base_ + static_cast<uint64_t>(at - mirror_);
}

void SvmAllocation::mapped()
{
    AV_SCOPE("SvmAllocation::mapped");
    if (map_depth_ != ~uint32_t{0}) ++map_depth_;
}

bool SvmAllocation::unmapped()
{
    AV_SCOPE("SvmAllocation::unmapped");
    if (map_depth_ == 0u) return false;
    --map_depth_;
    return true;
}

uint32_t SvmAllocation::map_depth() const
{
    AV_SCOPE("SvmAllocation::map_depth");
    return map_depth_;
}

bool SvmAllocation::invariants_hold() const
{
    AV_SCOPE("SvmAllocation::invariants_hold");
    if (mirror_ == nullptr) return host_base_ == 0u && bytes_ == 0u && map_depth_ == 0u;
    if (bytes_ == 0u || host_base_ == 0u) return false;
    /* The pair is the same size, so a host address inside it is representable
     * from a mirror address inside it. */
    return bytes_ <= ~uint64_t{0} - host_base_;
}

}  // namespace av::clicd
