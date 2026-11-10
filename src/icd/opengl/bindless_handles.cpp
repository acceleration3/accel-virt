#include "icd/opengl/bindless_handles.hpp"

#include "core/trace/function_scope.hpp"

namespace av::glicd {

namespace {

constexpr uint64_t group_mix = 0x9E3779B97F4A7C15ull;
constexpr uint32_t most_forgotten = 4096u;

}

BindlessHandles::BindlessHandles(BlockSource &memory) : entries_(memory, Pool::paged)
{
    AV_SCOPE("BindlessHandles::BindlessHandles");
}

uint64_t BindlessHandles::key_of(uint32_t group, uint64_t handle)
{
    AV_SCOPE("BindlessHandles::key_of");
    uint64_t mixed = handle ^ ((uint64_t{group} + 1u) * group_mix);
    mixed ^= mixed >> 29u;
    return mixed;
}

Status BindlessHandles::add(uint32_t group, uint64_t handle, Kind kind, uint32_t texture)
{
    AV_SCOPE("BindlessHandles::add");
    if (handle == 0u) return Status::invalid_parameter;
    const uint64_t key = key_of(group, handle);
    Entry *const   found = entries_.find(key);
    if (found != nullptr) {
        if (found->handle_ != handle || found->group_ != group) return Status::conflict;
        found->kind_ = kind;
        found->texture_ = texture;
        return Status::ok;
    }
    uint32_t slot = 0u;
    return entries_.insert(key, Entry{handle, group, texture, kind}, slot);
}

bool BindlessHandles::valid(uint32_t group, uint64_t handle, Kind kind) const
{
    AV_SCOPE("BindlessHandles::valid");
    const Entry *const found = entries_.find(key_of(group, handle));
    return found != nullptr && found->handle_ == handle && found->group_ == group && found->kind_ == kind;
}

void BindlessHandles::forget_textures(uint32_t group, int32_t count, const uint32_t *names)
{
    AV_SCOPE("BindlessHandles::forget_textures");
    if (names == nullptr || count <= 0 || entries_.count() == 0u) return;
    uint64_t doomed[most_forgotten];
    uint32_t found = 0u;
    entries_.for_each([&](uint64_t key, const Entry &entry) {
        if (entry.group_ != group || found >= most_forgotten) return;
        for (int32_t at = 0; at < count; at++)
            if (names[at] == entry.texture_) {
                doomed[found++] = key;
                return;
            }
    });
    for (uint32_t at = 0u; at < found; at++) {
        Entry gone{};
        static_cast<void>(entries_.take(doomed[at], gone));
    }
}

uint32_t BindlessHandles::count() const
{
    AV_SCOPE("BindlessHandles::count");
    return entries_.count();
}

bool BindlessHandles::invariants_hold() const
{
    AV_SCOPE("BindlessHandles::invariants_hold");
    return entries_.invariants_hold();
}

}
