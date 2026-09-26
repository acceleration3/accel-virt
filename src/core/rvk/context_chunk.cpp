#include "core/rvk/context_chunk.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ContextChunk::ContextChunk() : allocation_(nullptr), mapping_(nullptr), resource_id_(0u), first_page_(0u), pages_(0u)
{
    AV_SCOPE("ContextChunk::ContextChunk");
}

void ContextChunk::take_guest(const Allocation &allocation, uint32_t resource_id)
{
    AV_SCOPE("ContextChunk::take_guest");
    allocation_ = &allocation;
    mapping_ = nullptr;
    resource_id_ = resource_id;
    first_page_ = 0u;
    pages_ = 0u;
}

void ContextChunk::take_window(uint32_t resource_id, uint32_t first, uint32_t pages, void *mapping)
{
    AV_SCOPE("ContextChunk::take_window");
    allocation_ = nullptr;
    mapping_ = mapping;
    resource_id_ = resource_id;
    first_page_ = first;
    pages_ = pages;
}

void ContextChunk::clear()
{
    AV_SCOPE("ContextChunk::clear");
    allocation_ = nullptr;
    mapping_ = nullptr;
    resource_id_ = 0u;
    first_page_ = 0u;
    pages_ = 0u;
}

bool ContextChunk::bound() const
{
    AV_SCOPE("ContextChunk::bound");
    return resource_id_ != 0u;
}

bool ContextChunk::in_window() const
{
    AV_SCOPE("ContextChunk::in_window");
    return mapping_ != nullptr;
}

const Allocation *ContextChunk::allocation() const
{
    AV_SCOPE("ContextChunk::allocation");
    return allocation_;
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

/* A slot holds exactly one kind of chunk when it names a resource, and nothing
 * when it does not; a window chunk is at least a page. */
bool ContextChunk::invariants_hold() const
{
    AV_SCOPE("ContextChunk::invariants_hold");
    const bool guest = allocation_ != nullptr;
    const bool window = mapping_ != nullptr;
    if (resource_id_ == 0u) return !guest && !window && pages_ == 0u && first_page_ == 0u;
    if (guest) return !window && pages_ == 0u && first_page_ == 0u;
    return window && pages_ != 0u;
}

}  // namespace av
