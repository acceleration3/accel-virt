#include "executor/opengl/gl_memory_objects.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/opengl/gl_memory_group.hpp"
#include "executor/shim/gl_host_import.hpp"
#include "executor/slice_progress.hpp"

namespace av {

GlMemoryObjects::GlMemoryObjects(BlockSource &blocks, const SliceProgress &progress)
    : progress_(progress), memories_(blocks, Pool::paged), placements_(blocks, Pool::paged),
      textures_(blocks, Pool::paged), group_(nullptr), own_(nullptr)
{
    AV_SCOPE("GlMemoryObjects::GlMemoryObjects");
}

GlMemoryObjects::~GlMemoryObjects()
{
    AV_SCOPE("GlMemoryObjects::~GlMemoryObjects");
    part();
}

GlMemoryObjects::Hold::Hold(GlMemoryGroup *group) : group_(group)
{
    AV_SCOPE("GlMemoryObjects::Hold::Hold");
    if (group_ != nullptr) group_->lock();
}

GlMemoryObjects::Hold::~Hold()
{
    AV_SCOPE("GlMemoryObjects::Hold::~Hold");
    if (group_ != nullptr) group_->unlock();
}

bool GlMemoryObjects::join(GlMemoryGroup &group, GlHostImport *own)
{
    AV_SCOPE("GlMemoryObjects::join");
    part();
    own_ = own;
    if (!group.join(*this)) {
        own_ = nullptr;
        return false;
    }
    group_ = &group;
    return true;
}

void GlMemoryObjects::part()
{
    AV_SCOPE("GlMemoryObjects::part");
    if (group_ == nullptr) return;
    group_->part(*this);
    group_ = nullptr;
    own_ = nullptr;
}

GlMemoryObjects *GlMemoryObjects::holder_of(uint32_t name)
{
    AV_SCOPE("GlMemoryObjects::holder_of");
    if (group_ == nullptr) return memories_.find(name) != nullptr ? this : nullptr;
    for (uint32_t index = 0u; index < group_->members(); index++)
        if (group_->at(index)->memories_.find(name) != nullptr) return group_->at(index);
    return nullptr;
}

GlMemoryObjects *GlMemoryObjects::texture_holder_of(uint32_t texture)
{
    AV_SCOPE("GlMemoryObjects::texture_holder_of");
    if (group_ == nullptr) return textures_.find(texture) != nullptr ? this : nullptr;
    for (uint32_t index = 0u; index < group_->members(); index++)
        if (group_->at(index)->textures_.find(texture) != nullptr) return group_->at(index);
    return nullptr;
}

Status GlMemoryObjects::admit(uint32_t buffer, uint32_t name, uint64_t offset, uint64_t bytes)
{
    AV_SCOPE("GlMemoryObjects::admit");
    Hold             hold(group_);
    GlMemoryObjects *const table = holder_of(name);
    return table != nullptr ? table->admit_here(buffer, name, offset, bytes) : Status::invalid_parameter;
}

Status GlMemoryObjects::place(uint32_t buffer, uint32_t name, uint64_t offset, uint64_t bytes)
{
    AV_SCOPE("GlMemoryObjects::place");
    Hold             hold(group_);
    GlMemoryObjects *const table = holder_of(name);
    if (table == nullptr) return Status::invalid_parameter;
    const Status admitted = table->admit_here(buffer, name, offset, bytes);
    if (admitted != Status::ok) return admitted;
    leave_everywhere(buffer);
    return table->place_here(buffer, name, offset, bytes);
}

void GlMemoryObjects::leave(uint32_t buffer)
{
    AV_SCOPE("GlMemoryObjects::leave");
    Hold hold(group_);
    leave_everywhere(buffer);
}

void GlMemoryObjects::leave_everywhere(uint32_t buffer)
{
    AV_SCOPE("GlMemoryObjects::leave_everywhere");
    if (group_ == nullptr) {
        leave_here(buffer);
        return;
    }
    for (uint32_t index = 0u; index < group_->members(); index++) group_->at(index)->leave_here(buffer);
}

void GlMemoryObjects::leave_texture_everywhere(uint32_t texture)
{
    AV_SCOPE("GlMemoryObjects::leave_texture_everywhere");
    if (group_ == nullptr) {
        leave_texture_here(texture);
        return;
    }
    for (uint32_t index = 0u; index < group_->members(); index++) group_->at(index)->leave_texture_here(texture);
}

Status GlMemoryObjects::admit_texture(uint32_t texture, uint32_t name, uint64_t offset, uint64_t bytes)
{
    AV_SCOPE("GlMemoryObjects::admit_texture");
    Hold             hold(group_);
    GlMemoryObjects *const table = holder_of(name);
    return table != nullptr ? table->admit_texture_here(texture, name, offset, bytes) : Status::invalid_parameter;
}

Status GlMemoryObjects::place_texture(uint32_t texture, uint32_t name, uint64_t offset, uint64_t bytes)
{
    AV_SCOPE("GlMemoryObjects::place_texture");
    Hold             hold(group_);
    GlMemoryObjects *const table = holder_of(name);
    if (table == nullptr) return Status::invalid_parameter;
    const Status admitted = table->admit_texture_here(texture, name, offset, bytes);
    if (admitted != Status::ok) return admitted;
    leave_texture_everywhere(texture);
    return table->place_texture_here(texture, name, offset, bytes);
}

Status GlMemoryObjects::inherit_texture(uint32_t view, uint32_t original)
{
    AV_SCOPE("GlMemoryObjects::inherit_texture");
    Hold             hold(group_);
    GlMemoryObjects *const table = texture_holder_of(original);
    if (table == nullptr) return Status::not_found;
    if (view != 0u && view != original) leave_texture_everywhere(view);
    return table->inherit_texture_here(view, original);
}

void GlMemoryObjects::leave_texture(uint32_t texture)
{
    AV_SCOPE("GlMemoryObjects::leave_texture");
    Hold hold(group_);
    leave_texture_everywhere(texture);
}

void GlMemoryObjects::retire(uint32_t name)
{
    AV_SCOPE("GlMemoryObjects::retire");
    Hold             hold(group_);
    GlMemoryObjects *const table = holder_of(name);
    if (table != nullptr) table->retire_here(name);
}

void GlMemoryObjects::reclaim(GlHostImport &import)
{
    AV_SCOPE("GlMemoryObjects::reclaim");
    Hold hold(group_);
    if (group_ == nullptr) {
        reclaim_here(import, import);
        return;
    }
    for (uint32_t index = 0u; index < group_->members(); index++) {
        GlMemoryObjects *const member = group_->at(index);
        member->reclaim_here(import, member->own_ != nullptr ? *member->own_ : import);
    }
}

void GlMemoryObjects::destroy_all(GlHostImport &import)
{
    AV_SCOPE("GlMemoryObjects::destroy_all");
    Hold hold(group_);
    destroy_here(import);
}

bool GlMemoryObjects::holds(uint32_t name)
{
    AV_SCOPE("GlMemoryObjects::holds");
    Hold hold(group_);
    return holder_of(name) != nullptr;
}

Status GlMemoryObjects::add(uint32_t name, uint64_t size)
{
    AV_SCOPE("GlMemoryObjects::add");
    Hold hold(group_);
    if (name == 0u || size == 0u || memories_.find(name) != nullptr) return Status::invalid_parameter;
    uint32_t slot = 0u;
    return memories_.insert(name, Memory{size, 0u, 0u, 0u, false}, slot);
}

Status GlMemoryObjects::admit_here(uint32_t buffer, uint32_t name, uint64_t offset, uint64_t bytes)
{
    AV_SCOPE("GlMemoryObjects::admit_here");
    const Memory *const memory = memories_.find(name);
    if (buffer == 0u || memory == nullptr || memory->retired_ || bytes == 0u) return Status::invalid_parameter;
    if (offset > memory->size_ || bytes > memory->size_ - offset) return Status::invalid_parameter;
    if (placements_.count() + textures_.count() >= most_placements) return Status::no_memory;
    return placements_.reserve(placements_.count() + 1u);
}

Status GlMemoryObjects::place_here(uint32_t buffer, uint32_t name, uint64_t offset, uint64_t bytes)
{
    AV_SCOPE("GlMemoryObjects::place_here");
    const Status admitted = admit_here(buffer, name, offset, bytes);
    if (admitted != Status::ok) return admitted;
    leave_here(buffer);
    uint32_t slot = 0u;
    const Status held = placements_.insert(buffer, name, slot);
    if (held != Status::ok) return held;
    memories_.find(name)->placed_++;
    return Status::ok;
}

void GlMemoryObjects::leave_here(uint32_t buffer)
{
    AV_SCOPE("GlMemoryObjects::leave_here");
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

Status GlMemoryObjects::admit_texture_here(uint32_t texture, uint32_t name, uint64_t offset, uint64_t bytes)
{
    AV_SCOPE("GlMemoryObjects::admit_texture_here");
    const Memory *const memory = memories_.find(name);
    if (texture == 0u || memory == nullptr || memory->retired_ || bytes == 0u) return Status::invalid_parameter;
    if (offset > memory->size_ || bytes > memory->size_ - offset) return Status::invalid_parameter;
    if (placements_.count() + textures_.count() >= most_placements) return Status::no_memory;
    return textures_.reserve(textures_.count() + 1u);
}

Status GlMemoryObjects::place_texture_here(uint32_t texture, uint32_t name, uint64_t offset, uint64_t bytes)
{
    AV_SCOPE("GlMemoryObjects::place_texture_here");
    const Status admitted = admit_texture_here(texture, name, offset, bytes);
    if (admitted != Status::ok) return admitted;
    leave_texture_here(texture);
    uint32_t slot = 0u;
    const Status held = textures_.insert(texture, name, slot);
    if (held != Status::ok) return held;
    memories_.find(name)->textured_++;
    return Status::ok;
}

Status GlMemoryObjects::inherit_texture_here(uint32_t view, uint32_t original)
{
    AV_SCOPE("GlMemoryObjects::inherit_texture_here");
    const uint32_t *const source = textures_.find(original);
    if (source == nullptr) return Status::not_found;
    const uint32_t name = *source;
    if (view == 0u || view == original) return Status::invalid_parameter;
    if (placements_.count() + textures_.count() >= most_placements) return Status::no_memory;
    const Status reserved = textures_.reserve(textures_.count() + 1u);
    if (reserved != Status::ok) return reserved;
    leave_texture_here(view);
    uint32_t slot = 0u;
    const Status held = textures_.insert(view, name, slot);
    if (held != Status::ok) return held;
    memories_.find(name)->textured_++;
    return Status::ok;
}

void GlMemoryObjects::leave_texture_here(uint32_t texture)
{
    AV_SCOPE("GlMemoryObjects::leave_texture_here");
    uint32_t name = 0u;
    if (!textures_.take(texture, name)) return;
    Memory *const memory = memories_.find(name);
    if (memory == nullptr) return;
    memory->textured_--;
    bump(*memory);
}

void GlMemoryObjects::retire_here(uint32_t name)
{
    AV_SCOPE("GlMemoryObjects::retire_here");
    Memory *const memory = memories_.find(name);
    if (memory == nullptr || memory->retired_) return;
    memory->retired_ = true;
    memory->mark_ = progress_.next();
}

void GlMemoryObjects::reclaim_here(GlHostImport &import, GlHostImport &owner)
{
    AV_SCOPE("GlMemoryObjects::reclaim_here");
    const uint64_t reached = progress_.reached();
    memories_.erase_if([&import, &owner, reached](uint32_t name, const Memory &memory) {
        if (!memory.retired_ || memory.placed_ != 0u || memory.textured_ != 0u ||
            memory.mark_ > reached) return false;
        import.delete_memory(name, owner);
        return true;
    });
}

void GlMemoryObjects::destroy_here(GlHostImport &import)
{
    AV_SCOPE("GlMemoryObjects::destroy_here");
    memories_.erase_if([&import](uint32_t name, const Memory &) {
        import.delete_memory(name);
        return true;
    });
    placements_.clear();
    textures_.clear();
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
        placed_in_known = placed_in_known && memories_.find(name) != nullptr;
    });
    bool textured_in_known = true;
    textures_.for_each([this, &textured_in_known](uint32_t, const uint32_t &name) {
        textured_in_known = textured_in_known && memories_.find(name) != nullptr;
    });
    return sound && placed_in_known && textured_in_known && counted == placements_.count() &&
           counted_textures == textures_.count() && memories_.invariants_hold() && placements_.invariants_hold() &&
           textures_.invariants_hold();
}

}
