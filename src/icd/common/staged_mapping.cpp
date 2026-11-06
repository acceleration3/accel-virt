#include "icd/common/staged_mapping.hpp"

#include "core/trace/function_scope.hpp"

namespace av::icd {

StagedMapping::StagedMapping()
    : object_(0u), offset_(0u), bytes_(0u), at_(nullptr), subresource_(0u), flags_(0u), memory_(Memory::named)
{
    AV_SCOPE("StagedMapping::StagedMapping");
}

StagedMapping::StagedMapping(uint64_t object, uint32_t subresource, uint64_t offset, uint64_t bytes, uint32_t flags)
    : object_(object), offset_(offset), bytes_(bytes), at_(nullptr), subresource_(subresource), flags_(flags),
      memory_(Memory::named)
{
    AV_SCOPE("StagedMapping::StagedMapping");
}

uint64_t StagedMapping::object() const
{
    AV_SCOPE("StagedMapping::object");
    return object_;
}

uint32_t StagedMapping::subresource() const
{
    AV_SCOPE("StagedMapping::subresource");
    return subresource_;
}

uint64_t StagedMapping::offset() const
{
    AV_SCOPE("StagedMapping::offset");
    return offset_;
}

uint64_t StagedMapping::bytes() const
{
    AV_SCOPE("StagedMapping::bytes");
    return bytes_;
}

uint32_t StagedMapping::flags() const
{
    AV_SCOPE("StagedMapping::flags");
    return flags_;
}

void StagedMapping::reflag(uint32_t flags)
{
    AV_SCOPE("StagedMapping::reflag");
    flags_ = flags;
}

uint8_t *StagedMapping::at() const
{
    AV_SCOPE("StagedMapping::at");
    return at_;
}

bool StagedMapping::copied() const
{
    AV_SCOPE("StagedMapping::copied");
    return at_ != nullptr && memory_ == Memory::copy;
}

void StagedMapping::hand(uint8_t *at, Memory memory)
{
    AV_SCOPE("StagedMapping::hand");
    at_ = at;
    memory_ = memory;
}

bool StagedMapping::invariants_hold() const
{
    AV_SCOPE("StagedMapping::invariants_hold");
    if (at_ == nullptr) return memory_ == Memory::named;
    return object_ != 0u && (memory_ == Memory::named || bytes_ != 0u);
}

}
