#include "executor/d3d/d3d9_copy_bindings.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

D3d9CopyBindings::D3d9CopyBindings(BlockSource &blocks)
    : copies_(blocks, Pool::paged), sources_(blocks, Pool::paged), draw_(0u)
{
    AV_SCOPE("D3d9CopyBindings::D3d9CopyBindings");
}

Status D3d9CopyBindings::add(uint32_t resource, IDirect3DResource9 *buffer, IDirect3DDevice9 *device)
{
    AV_SCOPE("D3d9CopyBindings::add");
    copies_.erase(resource);
    uint32_t at = KeyedSlots<uint32_t, Copy>::no_slot;
    return copies_.insert(resource, Copy{buffer, device, 0u}, at);
}

void D3d9CopyBindings::drop(uint32_t resource)
{
    AV_SCOPE("D3d9CopyBindings::drop");
    copies_.erase(resource);
}

D3d9CopyBindings::Sources *D3d9CopyBindings::sources_of(IDirect3DDevice9 *device, Status &status)
{
    AV_SCOPE("D3d9CopyBindings::sources_of");
    const uintptr_t key = reinterpret_cast<uintptr_t>(device);
    status = Status::ok;
    Sources *found = sources_.find(key);
    if (found != nullptr) return found;
    uint32_t at = KeyedSlots<uintptr_t, Sources>::no_slot;
    status = sources_.insert(key, Sources{}, at);
    return status == Status::ok ? sources_.find(key) : nullptr;
}

Status D3d9CopyBindings::stream_bound(IDirect3DDevice9 *device, uint32_t stream, uint32_t resource)
{
    AV_SCOPE("D3d9CopyBindings::stream_bound");
    if (stream >= most_streams) return Status::ok;
    Status         status = Status::ok;
    Sources *const sources = sources_of(device, status);
    if (sources != nullptr) sources->streams_[stream] = resource;
    return status;
}

Status D3d9CopyBindings::indices_bound(IDirect3DDevice9 *device, uint32_t resource)
{
    AV_SCOPE("D3d9CopyBindings::indices_bound");
    Status         status = Status::ok;
    Sources *const sources = sources_of(device, status);
    if (sources != nullptr) sources->indices_ = resource;
    return status;
}

void D3d9CopyBindings::forget_sources(IDirect3DDevice9 *device)
{
    AV_SCOPE("D3d9CopyBindings::forget_sources");
    sources_.erase(reinterpret_cast<uintptr_t>(device));
}

uint32_t D3d9CopyBindings::count() const
{
    AV_SCOPE("D3d9CopyBindings::count");
    return copies_.count();
}

bool D3d9CopyBindings::invariants_hold() const
{
    AV_SCOPE("D3d9CopyBindings::invariants_hold");
    bool sound = copies_.invariants_hold() && sources_.invariants_hold();
    copies_.for_each([&sound, this](uint32_t, const Copy &held) {
        sound = sound && held.buffer_ != nullptr && held.device_ != nullptr && held.drawn_ <= draw_;
    });
    return sound;
}

}
