#include "executor/opengl/gl_memory_objects.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/gl_host_import.hpp"
#include "executor/slice_progress.hpp"

namespace av {

GlMemoryObjects::GlMemoryObjects(BlockSource &blocks, const SliceProgress &progress)
    : progress_(progress), memories_(blocks, Pool::paged), placements_(blocks, Pool::paged),
      textures_(blocks, Pool::paged)
{
    AV_SCOPE("GlMemoryObjects::GlMemoryObjects");
}

Status GlMemoryObjects::add(uint32_t name, uint64_t size)
{
    AV_SCOPE("GlMemoryObjects::add");
    if (name == 0u || size == 0u || memories_.find(name) != nullptr) return Status::invalid_parameter;
    uint32_t slot = 0u;
    return memories_.insert(name, Memory{size, 0u, 0u, 0u, false}, slot);
}

Status GlMemoryObjects::admit(uint32_t buffer, uint32_t name, uint64_t offset, uint64_t bytes)
{
    AV_SCOPE("GlMemoryObjects::admit");
    const Memory *const memory = memories_.find(name);
    if (buffer == 0u || memory == nullptr || memory->retired_ || bytes == 0u) return Status::invalid_parameter;
    if (offset > memory->size_ || bytes > memory->size_ - offset) return Status::invalid_parameter;
    if (placements_.count() + textures_.count() >= most_placements) return Status::no_memory;
    return placements_.reserve(placements_.count() + 1u);
}

Status GlMemoryObjects::place(uint32_t buffer, uint32_t name, uint64_t offset, uint64_t bytes)
{
    AV_SCOPE("GlMemoryObjects::place");
    const Status admitted = admit(buffer, name, offset, bytes);
    if (admitted != Status::ok) return admitted;
    leave(buffer);
    uint32_t slot = 0u;
    const Status held = placements_.insert(buffer, name, slot);
    if (held != Status::ok) return held;
    memories_.find(name)->placed_++;
    return Status::ok;
}

void GlMemoryObjects::leave(uint32_t buffer)
{
    AV_SCOPE("GlMemoryObjects::leave");
    uint32_t name = 0u;
    if (!placements_.take(buffer, name)) return;
    Memory *const memory = memories_.find(name);
    if (memory == nullptr) return;
    memory->placed_--;
    bump(*memory);
}

void GlMemoryObjects::bump(Memory &memory)
{
    AV_SCOPE("GlMemoryObjects::bump");
    if (memory.retired_) memory.mark_ = progress_.next();
}

Status GlMemoryObjects::admit_texture(uint32_t texture, uint32_t name, uint64_t offset, uint64_t bytes)
{
    AV_SCOPE("GlMemoryObjects::admit_texture");
    const Memory *const memory = memories_.find(name);
    if (texture == 0u || memory == nullptr || memory->retired_ || bytes == 0u) return Status::invalid_parameter;
    if (offset > memory->size_ || bytes > memory->size_ - offset) return Status::invalid_parameter;
    if (placements_.count() + textures_.count() >= most_placements) return Status::no_memory;
    return textures_.reserve(textures_.count() + 1u);
}

Status GlMemoryObjects::place_texture(uint32_t texture, uint32_t name, uint64_t offset, uint64_t bytes)
{
    AV_SCOPE("GlMemoryObjects::place_texture");
    const Status admitted = admit_texture(texture, name, offset, bytes);
    if (admitted != Status::ok) return admitted;
    leave_texture(texture);
    uint32_t slot = 0u;
    const Status held = textures_.insert(texture, name, slot);
    if (held != Status::ok) return held;
    memories_.find(name)->textured_++;
    return Status::ok;
}

Status GlMemoryObjects::inherit_texture(uint32_t view, uint32_t original)
{
    AV_SCOPE("GlMemoryObjects::inherit_texture");
    const uint32_t *const source = textures_.find(original);
    if (source == nullptr) return Status::not_found;
    const uint32_t name = *source;
    if (view == 0u || view == original) return Status::invalid_parameter;
    if (placements_.count() + textures_.count() >= most_placements) return Status::no_memory;
    const Status reserved = textures_.reserve(textures_.count() + 1u);
    if (reserved != Status::ok) return reserved;
    leave_texture(view);
    uint32_t slot = 0u;
    const Status held = textures_.insert(view, name, slot);
    if (held != Status::ok) return held;
    memories_.find(name)->textured_++;
    return Status::ok;
}

void GlMemoryObjects::leave_texture(uint32_t texture)
{
    AV_SCOPE("GlMemoryObjects::leave_texture");
    uint32_t name = 0u;
    if (!textures_.take(texture, name)) return;
    Memory *const memory = memories_.find(name);
    if (memory == nullptr) return;
    memory->textured_--;
    bump(*memory);
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
        if (!memory.retired_ || memory.placed_ != 0u || memory.textured_ != 0u ||
            memory.mark_ > reached) return false;
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
    textures_.clear();
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

uint32_t GlMemoryObjects::textured() const
{
    AV_SCOPE("GlMemoryObjects::textured");
    return textures_.count();
}

bool GlMemoryObjects::invariants_hold() const
{
    AV_SCOPE("GlMemoryObjects::invariants_hold");
    uint32_t counted = 0u;
    uint32_t counted_textures = 0u;
    bool     sound = true;
    memories_.for_each([&counted, &counted_textures, &sound](uint32_t, const Memory &memory) {
        counted += memory.placed_;
        counted_textures += memory.textured_;
        sound = sound && memory.size_ != 0u;
    });
    bool placed_in_known = true;
    placements_.for_each([this, &placed_in_known](uint32_t, const uint32_t &name) {
        placed_in_known = placed_in_known && holds(name);
    });
    bool textured_in_known = true;
    textures_.for_each([this, &textured_in_known](uint32_t, const uint32_t &name) {
        textured_in_known = textured_in_known && holds(name);
    });
    return sound && placed_in_known && textured_in_known && counted == placements_.count() &&
           counted_textures == textures_.count() && memories_.invariants_hold() && placements_.invariants_hold() &&
           textures_.invariants_hold();
}

}
