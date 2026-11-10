#include "icd/opencl/svm_allocation.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

SvmAllocation::SvmAllocation() : host_base_(0u), mirror_(nullptr), raw_(nullptr), bytes_(0u), map_depth_(0u), device_wrote_(false), regions_{}
{
    AV_SCOPE("SvmAllocation::SvmAllocation");
}

void SvmAllocation::take(uint64_t host_base, uint8_t *mirror, size_t bytes, uint8_t *raw)
{
    AV_SCOPE("SvmAllocation::take");
    host_base_ = host_base;
    mirror_ = mirror;
    raw_ = raw != nullptr ? raw : mirror;
    bytes_ = bytes;
    map_depth_ = 0u;
}

void SvmAllocation::give_up()
{
    AV_SCOPE("SvmAllocation::give_up");
    host_base_ = 0u;
    mirror_ = nullptr;
    raw_ = nullptr;
    bytes_ = 0u;
    map_depth_ = 0u;
    for (Region &region : regions_) region = Region{};
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

uint8_t *SvmAllocation::raw() const
{
    AV_SCOPE("SvmAllocation::raw");
    return raw_;
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

void SvmAllocation::note_region(uint64_t offset, uint64_t bytes, bool writes)
{
    AV_SCOPE("SvmAllocation::note_region");
    for (Region &region : regions_) {
        if (region.used_) continue;
        region = Region{offset, bytes, writes, true};
        return;
    }
    regions_[0] = Region{0u, bytes_, writes || regions_[0].writes_, true};
}

bool SvmAllocation::take_region(uint64_t offset, uint64_t &bytes, bool &writes)
{
    AV_SCOPE("SvmAllocation::take_region");
    for (Region &region : regions_) {
        if (!region.used_ || region.offset_ != offset) continue;
        bytes = region.bytes_;
        writes = region.writes_;
        region = Region{};
        return true;
    }
    return false;
}

void SvmAllocation::note_device_write()
{
    AV_SCOPE("SvmAllocation::note_device_write");
    device_wrote_ = true;
}

bool SvmAllocation::take_device_write()
{
    AV_SCOPE("SvmAllocation::take_device_write");
    const bool wrote = device_wrote_;
    device_wrote_ = false;
    return wrote;
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
    return bytes_ <= ~uint64_t{0} - host_base_;
}

}
