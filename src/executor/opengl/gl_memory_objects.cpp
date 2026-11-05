#include "executor/opengl/gl_memory_objects.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/gl_host_import.hpp"
#include "executor/slice_progress.hpp"

namespace av {

GlMemoryObjects::GlMemoryObjects(BlockSource &blocks, const SliceProgress &progress)
    : progress_(progress), memories_(blocks, Pool::paged), placements_(blocks, Pool::paged)
{
    AV_SCOPE("GlMemoryObjects::GlMemoryObjects");
}

Status GlMemoryObjects::add(uint32_t name, uint64_t size)
{
    AV_SCOPE("GlMemoryObjects::add");
    if (name == 0u || size == 0u || memories_.find(name) != nullptr) return Status::invalid_parameter;
    uint32_t slot = 0u;
    return memories_.insert(name, Memory{size, 0u, 0u, false}, slot);
}

Status GlMemoryObjects::place(uint32_t buffer, uint32_t name, uint64_t offset, uint64_t bytes)
{
    AV_SCOPE("GlMemoryObjects::place");
    Memory *const memory = memories_.find(name);
    if (buffer == 0u || memory == nullptr || memory->retired_ || bytes == 0u) return Status::invalid_parameter;
    if (offset > memory->size_ || bytes > memory->size_ - offset) return Status::invalid_parameter;
    leave(buffer);
    if (placements_.count() >= most_placements) return Status::no_memory;
    uint32_t slot = 0u;
    const Status held = placements_.insert(buffer, name, slot);
    if (held != Status::ok) return held;
    memory->placed_++;
    return Status::ok;
}

void GlMemoryObjects::leave(uint32_t buffer)
{
    AV_SCOPE("GlMemoryObjects::leave");
    uint32_t name = 0u;
    if (!placements_.take(buffer, name)) return;
    Memory *const memory = memories_.find(name);
    if (memory != nullptr) memory->placed_--;
}

void GlMemoryObjects::retire(uint32_t name)
{
    AV_SCOPE("GlMemoryObjects::retire");
    Memory *const memory = memories_.find(name);
    if (memory == nullptr || memory->retired_) return;
    memory->retired_ = true;
    memory->mark_ = progress_.next();
}

void GlMemoryObjects::reclaim(GlHostImport &import)
{
    AV_SCOPE("GlMemoryObjects::reclaim");
    const uint64_t reached = progress_.reached();
    memories_.erase_if([&import, reached](uint32_t name, const Memory &memory) {
        if (!memory.retired_ || memory.placed_ != 0u || memory.mark_ > reached) return false;
        import.delete_memory(name);
        return true;
    });
}

void GlMemoryObjects::destroy_all(GlHostImport &import)
{
    AV_SCOPE("GlMemoryObjects::destroy_all");
    memories_.erase_if([&import](uint32_t name, const Memory &) {
        import.delete_memory(name);
        return true;
    });
    placements_.clear();
}

bool GlMemoryObjects::holds(uint32_t name) const
{
    AV_SCOPE("GlMemoryObjects::holds");
    return memories_.find(name) != nullptr;
}

uint32_t GlMemoryObjects::count() const
{
    AV_SCOPE("GlMemoryObjects::count");
    return memories_.count();
}

uint32_t GlMemoryObjects::placed() const
{
    AV_SCOPE("GlMemoryObjects::placed");
    return placements_.count();
}

bool GlMemoryObjects::invariants_hold() const
{
    AV_SCOPE("GlMemoryObjects::invariants_hold");
    uint32_t counted = 0u;
    bool     sound = true;
    memories_.for_each([&counted, &sound](uint32_t, const Memory &memory) {
        counted += memory.placed_;
        sound = sound && memory.size_ != 0u;
    });
    bool placed_in_known = true;
    placements_.for_each([this, &placed_in_known](uint32_t, const uint32_t &name) {
        placed_in_known = placed_in_known && holds(name);
    });
    return sound && placed_in_known && counted == placements_.count() && memories_.invariants_hold() &&
           placements_.invariants_hold();
}

}
