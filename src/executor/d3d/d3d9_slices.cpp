#include "executor/d3d/d3d9_slices.hpp"

#include "core/log/driver_log.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/d3d/d3d9_batch_progress.hpp"
#include "executor/d3d/slice_storages.hpp"
#include "executor/import_table.hpp"
#include "executor/shim/d3d9_host_device.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/source_memory.hpp"

namespace av {

D3d9Slices::D3d9Slices(SliceStorages &storages, D3d9BatchProgress &progress, DriverLog &lines)
    : storages_(storages), progress_(progress), lines_(lines), host_(nullptr), imports_(nullptr), lane_(),
      reported_(false)
{
    AV_SCOPE("D3d9Slices::D3d9Slices");
}

void D3d9Slices::initialize(D3d9HostDevice *host, ImportTable *imports)
{
    AV_SCOPE("D3d9Slices::initialize");
    host_ = host;
    imports_ = imports;
    reported_ = false;
}

void D3d9Slices::shut_down()
{
    AV_SCOPE("D3d9Slices::shut_down");
    settle();
    storages_.destroy_all();
    lane_.release();
    host_ = nullptr;
    imports_ = nullptr;
}

bool D3d9Slices::supported() const
{
    AV_SCOPE("D3d9Slices::supported");
    return host_ != nullptr && host_->slices();
}

bool D3d9Slices::active() const
{
    AV_SCOPE("D3d9Slices::active");
    return lane_.connected();
}

bool D3d9Slices::bind(IDirect3DDevice9 *device, uint32_t resource, IDirect3DResource9 *buffer, uint64_t memory,
                      uint64_t offset, uint64_t bytes)
{
    AV_SCOPE("D3d9Slices::bind");
    if (memory == 0u) return offset == 0u && bytes == 0u && detach(resource, buffer);
    SourceMemory *const store = imports_ != nullptr ? imports_->named(memory) : nullptr;
    uint8_t *const      slice = store != nullptr ? store->at(offset, bytes) : nullptr;
    if (buffer == nullptr || !supported() || !fits(buffer, bytes) || !lane_.connect(device)) return false;
    const VulkanBuffer *const storage =
        slice != nullptr ? storages_.bind(lane_, *store, progress_.next(), progress_.reached()) : nullptr;
    if (storage == nullptr) {
        if (slice != nullptr && !reported_) lines_.write("a block of slice memory could not be imported to draw Direct3D 9 buffers from: they are copied");
        reported_ = reported_ || slice != nullptr;
        static_cast<void>(detach(resource, buffer));
        return false;
    }
    SliceBindings::Binding previous = {};
    const bool             had_previous = storages_.leave(resource, previous);
    const auto             abandon = [this, buffer, had_previous, &previous]() {
        if (had_previous) static_cast<void>(leave(buffer, previous));
    };
    if (!storages_.attach({resource, store, slice, bytes})) {
        abandon();
        return false;
    }
    if (lane_.bind_storage(buffer, *storage, offset, bytes, slice)) return true;
    forget(resource);
    abandon();
    return false;
}

bool D3d9Slices::detach(uint32_t resource, IDirect3DResource9 *buffer)
{
    AV_SCOPE("D3d9Slices::detach");
    if (buffer == nullptr) return false;
    SliceBindings::Binding was = {};
    if (!storages_.bound(resource, was)) return true;
    if (!leave(buffer, was)) return false;
    forget(resource);
    return true;
}

void D3d9Slices::discard(IDirect3DResource9 *buffer)
{
    AV_SCOPE("D3d9Slices::discard");
    void    *mapped = nullptr;
    uint32_t size = 0u;
    if (buffer == nullptr || !lock_fresh(buffer, mapped, size)) return;
    unlock(buffer);
}

void D3d9Slices::forget(uint32_t resource)
{
    AV_SCOPE("D3d9Slices::forget");
    SliceBindings::Binding was = {};
    static_cast<void>(storages_.leave(resource, was));
}

void D3d9Slices::forget_device(IDirect3DDevice9 *device)
{
    AV_SCOPE("D3d9Slices::forget_device");
    progress_.forget(device);
    if (!lane_.is(device)) return;
    settle();
    storages_.destroy_all();
    lane_.release();
}

void D3d9Slices::reclaim()
{
    AV_SCOPE("D3d9Slices::reclaim");
    storages_.reclaim(progress_.reached());
}

bool D3d9Slices::fits(IDirect3DResource9 *buffer, uint64_t bytes) const
{
    AV_SCOPE("D3d9Slices::fits");
    D3DVERTEXBUFFER_DESC vertices = {};
    D3DINDEXBUFFER_DESC  indices = {};
    const bool           vertex = buffer->GetType() == D3DRTYPE_VERTEXBUFFER;
    const bool           described = vertex ? SUCCEEDED(static_cast<IDirect3DVertexBuffer9 *>(buffer)->GetDesc(&vertices))
                                            : SUCCEEDED(static_cast<IDirect3DIndexBuffer9 *>(buffer)->GetDesc(&indices));
    const uint32_t       size = vertex ? vertices.Size : indices.Size;
    const DWORD          usage = vertex ? vertices.Usage : indices.Usage;
    const D3DPOOL        pool = vertex ? vertices.Pool : indices.Pool;
    return described && pool == D3DPOOL_DEFAULT && (usage & D3DUSAGE_DYNAMIC) != 0u && bytes >= size;
}

bool D3d9Slices::leave(IDirect3DResource9 *buffer, const SliceBindings::Binding &was)
{
    AV_SCOPE("D3d9Slices::leave");
    void    *mapped = nullptr;
    uint32_t size = 0u;
    if (!lock_fresh(buffer, mapped, size)) return false;
    const uint64_t kept = size < was.bytes_ ? size : was.bytes_;
    __builtin_memcpy(mapped, was.slice_, static_cast<size_t>(kept));
    unlock(buffer);
    return true;
}

bool D3d9Slices::lock_fresh(IDirect3DResource9 *buffer, void *&mapped, uint32_t &size) const
{
    AV_SCOPE("D3d9Slices::lock_fresh");
    mapped = nullptr;
    if (buffer->GetType() == D3DRTYPE_VERTEXBUFFER) {
        auto                *vertices = static_cast<IDirect3DVertexBuffer9 *>(buffer);
        D3DVERTEXBUFFER_DESC desc = {};
        if (FAILED(vertices->GetDesc(&desc))) return false;
        size = desc.Size;
        if (FAILED(vertices->Lock(0u, 0u, &mapped, D3DLOCK_DISCARD))) return false;
        if (mapped == nullptr) vertices->Unlock();
        return mapped != nullptr;
    }
    auto               *indices = static_cast<IDirect3DIndexBuffer9 *>(buffer);
    D3DINDEXBUFFER_DESC desc = {};
    if (FAILED(indices->GetDesc(&desc))) return false;
    size = desc.Size;
    if (FAILED(indices->Lock(0u, 0u, &mapped, D3DLOCK_DISCARD))) return false;
    if (mapped == nullptr) indices->Unlock();
    return mapped != nullptr;
}

void D3d9Slices::unlock(IDirect3DResource9 *buffer) const
{
    AV_SCOPE("D3d9Slices::unlock");
    if (buffer->GetType() == D3DRTYPE_VERTEXBUFFER) static_cast<IDirect3DVertexBuffer9 *>(buffer)->Unlock();
    else static_cast<IDirect3DIndexBuffer9 *>(buffer)->Unlock();
}

void D3d9Slices::settle()
{
    AV_SCOPE("D3d9Slices::settle");
    if (storages_.count() != 0u) lane_.idle();
}

bool D3d9Slices::invariants_hold() const
{
    AV_SCOPE("D3d9Slices::invariants_hold");
    return lane_.invariants_hold() && (!lane_.connected() || host_ != nullptr);
}

}
