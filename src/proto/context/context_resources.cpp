#include "proto/context/context_resources.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ContextResources::ContextResources(uint32_t first) : first_(first)
{
    AV_SCOPE("ContextResources::ContextResources");
}

uint32_t ContextResources::chunk(uint32_t slot) const
{
    AV_SCOPE("ContextResources::chunk");
    return first_ + transport_resources + slot;
}

uint32_t ContextResources::chunk_slot(uint32_t id) const
{
    AV_SCOPE("ContextResources::chunk_slot");
    if (id < first_) return chunk_slots;
    const uint32_t index = id - first_;
    if (index < transport_resources || index >= per_context) return chunk_slots;
    return index - transport_resources;
}

}  // namespace av
