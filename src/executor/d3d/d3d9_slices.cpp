#include "executor/d3d/d3d9_slices.hpp"

#include "core/log/driver_log.hpp"
#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/d3d/d3d9_lane.hpp"
#include "executor/d3d/d3d9_lanes.hpp"
#include "executor/d3d/slice_storages.hpp"
#include "executor/import_table.hpp"
#include "executor/shim/d3d9_host_device.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/source_memory.hpp"

namespace av {

D3d9Slices::D3d9Slices(BlockSource &blocks, SliceStorages &storages, D3d9Lanes &lanes, DriverLog &lines)
    : storages_(storages), lanes_(lanes), lines_(lines), host_(nullptr), imports_(nullptr),
      copies_(blocks), reported_(false)
{
    AV_SCOPE("D3d9Slices::D3d9Slices");
}

void D3d9Slices::initialize(D3d9HostDevice *host, ImportTable *imports)
{
    AV_SCOPE("D3d9Slices::initialize");
    host_ = host;
    imports_ = imports;
    reported_ = false;
    lanes_.begin_session();
}

void D3d9Slices::shut_down()
{
    AV_SCOPE("D3d9Slices::shut_down");
    const auto every_device = [this](const D3d9Lane &lane) {
        lane.idle();
        storages_.destroy_for(lane);
    };
    lanes_.for_each(every_device);
    copies_.drop_all([this](uint32_t resource) {
        SliceBindings::Binding was = {};
        static_cast<void>(storages_.leave(resource, was));
    });
    lanes_.forget_all();
    host_ = nullptr;
    imports_ = nullptr;
}

bool D3d9Slices::supported() const
{
    AV_SCOPE("D3d9Slices::supported");
    return host_ != nullptr && host_->opened();
}

bool D3d9Slices::probe(IDirect3DDevice9 *device)
{
    AV_SCOPE("D3d9Slices::probe");
    D3d9Lane *const lane = supported() ? lanes_.lane(device) : nullptr;
    if (lane == nullptr || !lane->connect()) return false;
    IDirect3DVertexBuffer9 *probe_buffer = nullptr;
    constexpr UINT          probe_bytes = 16u;
    if (FAILED(device->CreateVertexBuffer(probe_bytes, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0u, D3DPOOL_DEFAULT,
                                          &probe_buffer, nullptr)))
        return false;
    const bool bindable = lane->bindable(probe_buffer);
    probe_buffer->Release();
    if (bindable) lane->allow();
    return bindable;
}

bool D3d9Slices::bind(IDirect3DDevice9 *device, uint32_t resource, IDirect3DResource9 *buffer, uint64_t memory,
                      uint64_t offset, uint64_t bytes)
{
    AV_SCOPE("D3d9Slices::bind");
    if (memory == 0u) return offset == 0u && bytes == 0u && detach(resource, buffer);
    SourceMemory *const store = imports_ != nullptr ? imports_->named(memory) : nullptr;
    uint8_t *const      slice = store != nullptr ? store->at(offset, bytes) : nullptr;
    if (buffer == nullptr || !supported()) return false;
    if (slice == nullptr || offset % offset_alignment != 0u || !fits(buffer, bytes)) {
        static_cast<void>(detach(resource, buffer));
        return false;
    }
    D3d9Lane *const lane = lanes_.find(device);
    if (lane != nullptr && lane->allowed() && !lanes_.marking_failed() &&
        bound_to_storage(*lane, buffer, resource, *store, slice, offset, bytes))
        return true;
    return copy_at_draw(device, resource, buffer, *store, slice, bytes);
}

bool D3d9Slices::bound_to_storage(D3d9Lane &lane, IDirect3DResource9 *buffer, uint32_t resource, SourceMemory &store,
                                  uint8_t *slice, uint64_t offset, uint64_t bytes)
{
    AV_SCOPE("D3d9Slices::bound_to_storage");
    const VulkanBuffer *const storage = storages_.bind(lane, lane, store);
    if (storage == nullptr) return false;
    SliceBindings::Binding previous = {};
    copies_.drop(resource);
    const bool had_previous = storages_.leave(resource, previous);
    const auto abandon = [this, buffer, had_previous, &previous]() {
        if (had_previous) static_cast<void>(leave(buffer, previous));
    };
    const Status attached = storages_.attach({resource, &store, slice, bytes, &lane});
    if (attached != Status::ok) {
        LogLine(lines_).text("a Direct3D 9 buffer's slice kept nowhere, status ").number(static_cast<uint32_t>(attached)).write();
        abandon();
        return false;
    }
    if (lane.bind_storage(buffer, *storage, offset, bytes, slice)) return true;
    forget(resource);
    abandon();
    return false;
}

bool D3d9Slices::copy_at_draw(IDirect3DDevice9 *device, uint32_t resource, IDirect3DResource9 *buffer,
                              SourceMemory &store, uint8_t *slice, uint64_t bytes)
{
    AV_SCOPE("D3d9Slices::copy_at_draw");
    if (!reported_)
        lines_.write("a Direct3D 9 buffer's slice could not be drawn from in place: the host copies it into the buffer before each draw that uses it");
    reported_ = true;
    SliceBindings::Binding previous = {};
    copies_.drop(resource);
    if (storages_.leave(resource, previous)) static_cast<void>(leave(buffer, previous));
    const Status attached = storages_.attach({resource, &store, slice, bytes, nullptr});
    if (attached != Status::ok) {
        LogLine(lines_).text("a Direct3D 9 buffer's slice kept nowhere, status ").number(static_cast<uint32_t>(attached)).write();
        return false;
    }
    const Status kept = copies_.add(resource, buffer, device);
    if (kept == Status::ok) return true;
    LogLine(lines_).text("a Direct3D 9 buffer's copied slice kept nowhere, status ").number(static_cast<uint32_t>(kept)).write();
    SliceBindings::Binding unused = {};
    static_cast<void>(storages_.leave(resource, unused));
    return false;
}

void D3d9Slices::copy_in_use(IDirect3DDevice9 *device)
{
    AV_SCOPE("D3d9Slices::copy_in_use");
    copies_.due(device, [this](uint32_t resource, IDirect3DResource9 *buffer) { copy_one(resource, buffer); });
}

bool D3d9Slices::stream_bound(IDirect3DDevice9 *device, uint32_t stream, uint32_t resource)
{
    AV_SCOPE("D3d9Slices::stream_bound");
    const Status kept = copies_.stream_bound(device, stream, resource);
    if (kept == Status::ok) return true;
    LogLine(lines_).text("a Direct3D 9 stream's buffer kept nowhere, status ").number(static_cast<uint32_t>(kept)).write();
    return false;
}

bool D3d9Slices::indices_bound(IDirect3DDevice9 *device, uint32_t resource)
{
    AV_SCOPE("D3d9Slices::indices_bound");
    const Status kept = copies_.indices_bound(device, resource);
    if (kept == Status::ok) return true;
    LogLine(lines_).text("a Direct3D 9 index buffer kept nowhere, status ").number(static_cast<uint32_t>(kept)).write();
    return false;
}

void D3d9Slices::copy_one(uint32_t resource, IDirect3DResource9 *buffer)
{
    AV_SCOPE("D3d9Slices::copy_one");
    SliceBindings::Binding was = {};
    if (storages_.bound(resource, was)) static_cast<void>(leave(buffer, was));
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

void D3d9Slices::discard(const SliceBindings::Binding &bound, IDirect3DResource9 *buffer)
{
    AV_SCOPE("D3d9Slices::discard");
    copies_.drop(bound.resource_);
    D3d9Lane *const lane = bound.importer_ != nullptr ? lanes_.find(bound.importer_) : nullptr;
    if (lane != nullptr && buffer != nullptr) static_cast<void>(lane->unbind_storage(buffer));
}

void D3d9Slices::forget(uint32_t resource)
{
    AV_SCOPE("D3d9Slices::forget");
    copies_.drop(resource);
    SliceBindings::Binding was = {};
    static_cast<void>(storages_.leave(resource, was));
}

void D3d9Slices::forget_device(IDirect3DDevice9 *device)
{
    AV_SCOPE("D3d9Slices::forget_device");
    copies_.drop_device(device, [this](uint32_t resource) {
        SliceBindings::Binding was = {};
        static_cast<void>(storages_.leave(resource, was));
    });
    D3d9Lane *const lane = lanes_.find(device);
    if (lane == nullptr) return;
    lane->idle();
    storages_.destroy_for(*lane);
    lanes_.forget(device);
}

void D3d9Slices::reclaim()
{
    AV_SCOPE("D3d9Slices::reclaim");
    storages_.reclaim();
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
    D3d9Lane *const lane = was.importer_ != nullptr ? lanes_.find(was.importer_) : nullptr;
    if (lane != nullptr && !lane->unbind_storage(buffer)) return false;
    void    *mapped = nullptr;
    uint32_t size = 0u;
    if (!lock_for(buffer, lane != nullptr ? D3DLOCK_NOOVERWRITE : D3DLOCK_DISCARD, mapped, size)) return false;
    const uint64_t kept = size < was.bytes_ ? size : was.bytes_;
    __builtin_memcpy(mapped, was.slice_, static_cast<size_t>(kept));
    unlock(buffer);
    return true;
}

bool D3d9Slices::lock_for(IDirect3DResource9 *buffer, uint32_t flags, void *&mapped, uint32_t &size) const
{
    AV_SCOPE("D3d9Slices::lock_for");
    mapped = nullptr;
    if (buffer->GetType() == D3DRTYPE_VERTEXBUFFER) {
        auto                *vertices = static_cast<IDirect3DVertexBuffer9 *>(buffer);
        D3DVERTEXBUFFER_DESC desc = {};
        if (FAILED(vertices->GetDesc(&desc))) return false;
        size = desc.Size;
        if (FAILED(vertices->Lock(0u, 0u, &mapped, flags))) return false;
        if (mapped == nullptr) vertices->Unlock();
        return mapped != nullptr;
    }
    auto               *indices = static_cast<IDirect3DIndexBuffer9 *>(buffer);
    D3DINDEXBUFFER_DESC desc = {};
    if (FAILED(indices->GetDesc(&desc))) return false;
    size = desc.Size;
    if (FAILED(indices->Lock(0u, 0u, &mapped, flags))) return false;
    if (mapped == nullptr) indices->Unlock();
    return mapped != nullptr;
}

void D3d9Slices::unlock(IDirect3DResource9 *buffer) const
{
    AV_SCOPE("D3d9Slices::unlock");
    if (buffer->GetType() == D3DRTYPE_VERTEXBUFFER) static_cast<IDirect3DVertexBuffer9 *>(buffer)->Unlock();
    else static_cast<IDirect3DIndexBuffer9 *>(buffer)->Unlock();
}

bool D3d9Slices::invariants_hold() const
{
    AV_SCOPE("D3d9Slices::invariants_hold");
    return copies_.invariants_hold() && lanes_.invariants_hold();
}

}
