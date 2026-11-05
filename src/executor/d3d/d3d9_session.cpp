#include "executor/d3d/d3d9_session.hpp"

#include "core/log/driver_log.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/d3d/watched_blocks.hpp"
#include "executor/eventfd_wake.hpp"
#include "executor/shim/d3d9_host_device.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "proto/generated/d3d9/d3d9_command_names.h"
#include "proto/generated/d3d9/d3d9_protocol_ids.h"

#include <string.h>

namespace av {

D3d9Session::D3d9Session(const GuestBytes &guest, D3dObjectTable &objects, WatchedBlocks &blocks,
                         SliceStorages &storages, EventfdWake &wake, DriverLog &lines)
    : d3d9proto::ServerDispatch(guest), host_(nullptr), surfaces_(nullptr), flips_(blocks), objects_(objects), blocks_(blocks),
      devices_(blocks), lines_(lines), refusal_(lines_), arena_(nullptr), lanes_(blocks, wake, lines_), slices_(blocks, storages, lanes_, lines_),
      lost_(false)
{
    AV_SCOPE("D3d9Session::D3d9Session");
}

D3d9Session::~D3d9Session()
{
    AV_SCOPE("D3d9Session::~D3d9Session");
    shut_down();
}

void D3d9Session::initialize(D3d9HostDevice *host, SharedSurfaces *surfaces, D3d11HostDevice *device11,
                             SessionShares *shares, ImportTable *imports)
{
    AV_SCOPE("D3d9Session::initialize");
    host_ = host != nullptr && host->opened() ? host : nullptr;
    surfaces_ = surfaces;
    lost_ = false;
    flips_.bind(device11, shares);
    slices_.initialize(host_, imports);
}

void D3d9Session::shut_down()
{
    AV_SCOPE("D3d9Session::shut_down");
    slices_.shut_down();
    flips_.release();
    objects_.remove_kinds(D3dObjectKind::surface9, D3dObjectKind::query9);
    for (IDirect3DDevice9 *device = devices_.remove_any(); device != nullptr; device = devices_.remove_any())
        device->Release();
    host_ = nullptr;
}

bool D3d9Session::knows(uint32_t command_id) const
{
    AV_SCOPE("D3d9Session::knows");
    return d3d9proto::knows_command(command_id);
}

bool D3d9Session::run(uint32_t command_id, WireReader &payload, WireWriter &reply, WireArena &arena)
{
    AV_SCOPE("D3d9Session::run");
    arena_ = &arena;
    const bool ran = d3d9proto::dispatch_command(*this, command_id, payload, reply, arena);
    arena_ = nullptr;
    if (device() != nullptr && device()->TestCooperativeLevel() == D3DERR_DEVICELOST && !lost_) {
        lines_.write("a device's host Direct3D 9 device was lost; the session is faulted");
        lost_ = true;
    }
    if (ran) return true;
    return devices_.refuse(refusal_, d3d9proto::command_name(command_id),
                           blocks_.refusal(static_cast<uint32_t>(D3DERR_DRIVERINTERNALERROR)));
}

IDirect3DDevice9 *D3d9Session::device() const
{
    AV_SCOPE("D3d9Session::device");
    return devices_.ready() ? devices_.selected() : nullptr;
}

bool D3d9Session::on_CreateDevice9(uint32_t device, uint32_t, uint32_t &hresult)
{
    AV_SCOPE("D3d9Session::on_CreateDevice9");
    D3dObjectKind taken = D3dObjectKind::none;
    hresult = static_cast<uint32_t>(D3DERR_INVALIDCALL);
    if (device == 0u || devices_.find(device) != nullptr || objects_.find_any(device, taken) != nullptr) return true;
    hresult = static_cast<uint32_t>(E_OUTOFMEMORY);
    if (devices_.make_room() != Status::ok) return true;
    hresult = static_cast<uint32_t>(D3DERR_NOTAVAILABLE);
    if (host_ == nullptr) return true;
    IDirect3DDevice9 *made = nullptr;
    hresult = static_cast<uint32_t>(host_->make_device(made));
    if (made == nullptr) return true;
    made->BeginScene();
    static_cast<void>(devices_.add(device, made));
    static_cast<void>(devices_.select(device));
    return true;
}

bool D3d9Session::on_QueryCaps9(uint32_t count, const uint32_t *formats, uint32_t &hresult, uint8_t *caps,
                                uint64_t caps_capacity, uint64_t &caps_size, uint8_t *rows, uint64_t rows_capacity,
                                uint64_t &rows_size)
{
    AV_SCOPE("D3d9Session::on_QueryCaps9");
    caps_size = 0u;
    rows_size = 0u;
    hresult = static_cast<uint32_t>(D3DERR_NOTAVAILABLE);
    D3DCAPS9 answer = {};
    if (host_ == nullptr || caps_capacity < sizeof(answer)) return true;
    if (rows_capacity < uint64_t{count} * sizeof(d3d9proto::FormatCaps9)) return false;
    hresult = static_cast<uint32_t>(host_->caps(answer));
    if (hresult != D3D_OK) return true;
    memcpy(caps, &answer, sizeof(answer));
    caps_size = sizeof(answer);
    for (uint32_t index = 0u; index < count; index++) {
        const d3d9proto::FormatCaps9 row = host_->format_caps(formats[index]);
        memcpy(rows + index * sizeof(row), &row, sizeof(row));
    }
    rows_size = uint64_t{count} * sizeof(d3d9proto::FormatCaps9);
    return true;
}

bool D3d9Session::on_Flush9()
{
    AV_SCOPE("D3d9Session::on_Flush9");
    if (device() == nullptr) return true;
    device()->EndScene();
    device()->BeginScene();
    return true;
}

bool D3d9Session::on_SelectDevice9(uint32_t device)
{
    AV_SCOPE("D3d9Session::on_SelectDevice9");
    if (devices_.find(device) == nullptr) return false;
    static_cast<void>(devices_.select(device));
    return true;
}

bool D3d9Session::on_Finish9(uint32_t &hresult)
{
    AV_SCOPE("D3d9Session::on_Finish9");
    hresult = devices_.selected() != nullptr ? devices_.removed_reason(devices_.selected_id())
                                             : static_cast<uint32_t>(D3DERR_INVALIDCALL);
    return true;
}

bool D3d9Session::on_Destroy9(uint32_t object)
{
    AV_SCOPE("D3d9Session::on_Destroy9");
    IDirect3DDevice9 *const device = devices_.remove(object);
    if (device != nullptr) {
        flips_.forget_device(device);
        slices_.forget_device(device);
        device->Release();
        return true;
    }
    D3dObjectKind kind = D3dObjectKind::none;
    if (objects_.find_any(object, kind) == nullptr || kind < D3dObjectKind::surface9) return false;
    objects_.remove(object);
    slices_.forget(object);
    return true;
}

bool D3d9Session::keep(uint32_t id, D3dObjectKind kind, IUnknown *made)
{
    AV_SCOPE("D3d9Session::keep");
    if (made != nullptr && devices_.find(id) != nullptr) {
        made->Release();
        return false;
    }
    return objects_.keep(id, kind, made) == Status::ok;
}

IUnknown *D3d9Session::owned(uint32_t id, D3dObjectKind kind) const
{
    AV_SCOPE("D3d9Session::owned");
    IUnknown *const found = device() != nullptr ? objects_.find(id, kind) : nullptr;
    if (found == nullptr) return nullptr;
    IDirect3DDevice9 *made_by = nullptr;
    HRESULT           asked = D3DERR_INVALIDCALL;
    switch (kind) {
    case D3dObjectKind::vertex_shader9: asked = static_cast<IDirect3DVertexShader9 *>(found)->GetDevice(&made_by); break;
    case D3dObjectKind::pixel_shader9: asked = static_cast<IDirect3DPixelShader9 *>(found)->GetDevice(&made_by); break;
    case D3dObjectKind::vertex_declaration9:
        asked = static_cast<IDirect3DVertexDeclaration9 *>(found)->GetDevice(&made_by);
        break;
    case D3dObjectKind::query9: asked = static_cast<IDirect3DQuery9 *>(found)->GetDevice(&made_by); break;
    default: asked = static_cast<IDirect3DResource9 *>(found)->GetDevice(&made_by); break;
    }
    if (FAILED(asked)) return nullptr;
    made_by->Release();
    return made_by == device() ? found : nullptr;
}

bool D3d9Session::lost() const
{
    AV_SCOPE("D3d9Session::lost");
    return lost_;
}

void D3d9Session::end_batch()
{
    AV_SCOPE("D3d9Session::end_batch");
    if (!slices_.supported()) return;
    lanes_.mark_touched();
    slices_.reclaim();
}

void D3d9Session::discard_slice(const SliceBindings::Binding &bound)
{
    AV_SCOPE("D3d9Session::discard_slice");
    slices_.discard(bound, buffer_object(bound.resource_));
}

uint64_t D3d9Session::marked() const
{
    AV_SCOPE("D3d9Session::marked");
    return lanes_.marked();
}

uint64_t D3d9Session::reached() const
{
    AV_SCOPE("D3d9Session::reached");
    return lanes_.reached();
}

bool D3d9Session::polled() const
{
    AV_SCOPE("D3d9Session::polled");
    return lanes_.polled();
}

IDirect3DResource9 *D3d9Session::buffer_object(uint32_t id) const
{
    AV_SCOPE("D3d9Session::buffer_object");
    IUnknown *found = objects_.find(id, D3dObjectKind::vertex_buffer9);
    if (found == nullptr) found = objects_.find(id, D3dObjectKind::index_buffer9);
    return static_cast<IDirect3DResource9 *>(static_cast<void *>(found));
}

bool D3d9Session::invariants_hold() const
{
    AV_SCOPE("D3d9Session::invariants_hold");
    return devices_.invariants_hold() && objects_.invariants_hold() && lanes_.invariants_hold() &&
           slices_.invariants_hold();
}

}
