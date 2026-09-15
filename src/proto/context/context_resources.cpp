#include "proto/context/context_resources.hpp"

namespace av {

ContextResources::ContextResources(uint32_t first) : first_(first) {}

uint32_t ContextResources::ring() const  { return first_; }
uint32_t ContextResources::fence() const { return first_ + 1u; }
uint32_t ContextResources::reply() const { return first_ + 2u; }

uint32_t ContextResources::chunk(uint32_t slot) const
{
    return first_ + transport_resources + slot;
}

uint32_t ContextResources::chunk_slot(uint32_t id) const
{
    if (id < first_) return chunk_slots;
    const uint32_t index = id - first_;
    if (index < transport_resources || index >= per_context) return chunk_slots;
    return index - transport_resources;
}

}  // namespace av
