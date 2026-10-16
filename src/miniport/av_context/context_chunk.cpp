#include "miniport/av_context/context_chunk.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ContextChunk::ContextChunk(uint32_t slot, uint32_t resource_id, uint32_t first_page, uint32_t pages, void *mapping)
    : slot_(slot), resource_id_(resource_id), first_page_(first_page), pages_(pages), mapping_(mapping),
      locked_(nullptr)
{
    AV_SCOPE("ContextChunk::ContextChunk");
}

ContextChunk::ContextChunk(uint32_t slot, uint32_t resource_id, void *locked)
    : slot_(slot), resource_id_(resource_id), first_page_(0u), pages_(0u), mapping_(nullptr), locked_(locked)
{
    AV_SCOPE("ContextChunk::ContextChunk");
}

uint32_t ContextChunk::slot() const
{
    AV_SCOPE("ContextChunk::slot");
    return slot_;
}

uint32_t ContextChunk::resource_id() const
{
    AV_SCOPE("ContextChunk::resource_id");
    return resource_id_;
}

uint32_t ContextChunk::first_page() const
{
    AV_SCOPE("ContextChunk::first_page");
    return first_page_;
}

uint32_t ContextChunk::pages() const
{
    AV_SCOPE("ContextChunk::pages");
    return pages_;
}

void *ContextChunk::mapping() const
{
    AV_SCOPE("ContextChunk::mapping");
    return mapping_;
}

void *ContextChunk::locked() const
{
    AV_SCOPE("ContextChunk::locked");
    return locked_;
}

bool ContextChunk::invariants_hold() const
{
    AV_SCOPE("ContextChunk::invariants_hold");
    if (resource_id_ == 0u) return false;
    return locked_ != nullptr || (pages_ != 0u && mapping_ != nullptr);
}

}  // namespace av
