#include "executor/d3d/d3d9_session.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/guest_region.hpp"
#include "executor/shim/d3d9_host_device.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "proto/generated/d3d9/d3d9_command_names.h"
#include "proto/generated/d3d9/d3d9_protocol_ids.h"

#include <stdio.h>
#include <string.h>

namespace av {

namespace {

/* QueryFormats9 answers two words a format. */
constexpr uint64_t format_answer_bytes = 2u * sizeof(uint32_t);

}  // namespace

D3d9Session::D3d9Session(GuestRegion &region, D3dObjectTable &objects)
    : region_(region), host_(nullptr), surfaces_(nullptr), objects_(objects), devices_(), sink_(stderr), lines_(sink_, "virtio-gpu-av: d3d9: "), refusal_(lines_), arena_(nullptr), lost_(false)
{
    AV_SCOPE("D3d9Session::D3d9Session");
}

D3d9Session::~D3d9Session()
{
    AV_SCOPE("D3d9Session::~D3d9Session");
    shut_down();
}

void D3d9Session::initialize(D3d9HostDevice *host, SharedSurfaces *surfaces, D3d11HostDevice *device11,
                             SessionShares *shares)
{
    AV_SCOPE("D3d9Session::initialize");
    host_ = host != nullptr && host->opened() ? host : nullptr;
    surfaces_ = surfaces;
    lost_ = false;
    flips_.bind(device11, shares);
}

/* Objects first: each holds a reference to the device that made it. Only
 * Direct3D 9's: the table is the whole session's. */
void D3d9Session::shut_down()
{
    AV_SCOPE("D3d9Session::shut_down");
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
                           static_cast<uint32_t>(D3DERR_DRIVERINTERNALERROR));
}

const uint8_t *D3d9Session::guest_bytes(uint64_t offset, uint64_t size)
{
    AV_SCOPE("D3d9Session::guest_bytes");
    return region_.at(offset, size);
}

uint8_t *D3d9Session::guest_bytes_out(uint64_t offset, uint64_t size)
{
    AV_SCOPE("D3d9Session::guest_bytes_out");
    return region_.at(offset, size);
}

IDirect3DDevice9 *D3d9Session::device() const
{
    AV_SCOPE("D3d9Session::device");
    return devices_.ready() ? devices_.selected() : nullptr;
}

/* A device id is in the session's id space too, so no object may already
 * have it: the guest names both from one counter. A device is always inside a
 * scene, so a draw never needs the guest to say BeginScene, and Flush9 ends
 * one and begins the next. */
bool D3d9Session::on_CreateDevice9(uint32_t device, uint32_t, uint32_t &hresult)
{
    AV_SCOPE("D3d9Session::on_CreateDevice9");
    D3dObjectKind taken = D3dObjectKind::none;
    hresult = static_cast<uint32_t>(D3DERR_INVALIDCALL);
    if (device == 0u || devices_.find(device) != nullptr || objects_.find_any(device, taken) != nullptr) return true;
    hresult = static_cast<uint32_t>(D3DERR_OUTOFVIDEOMEMORY);
    if (devices_.count() == max_devices) return true;
    hresult = static_cast<uint32_t>(D3DERR_NOTAVAILABLE);
    if (host_ == nullptr) return true;
    IDirect3DDevice9 *made = nullptr;
    hresult = static_cast<uint32_t>(host_->make_device(made));
    if (made == nullptr) return true;
    made->BeginScene();
    /* The id is free and there is room: both were asked above. */
    static_cast<void>(devices_.add(device, made));
    static_cast<void>(devices_.select(device));
    return true;
}

bool D3d9Session::on_QueryCaps9(uint32_t &hresult, uint32_t &vendor_id, uint32_t &device_id, uint32_t &subsystem_id,
                                uint32_t &revision, uint32_t &video_memory_mb, uint8_t *caps, uint64_t caps_capacity,
                                uint64_t &caps_size)
{
    AV_SCOPE("D3d9Session::on_QueryCaps9");
    caps_size = 0u;
    hresult = static_cast<uint32_t>(D3DERR_NOTAVAILABLE);
    D3DCAPS9               answer = {};
    D3DADAPTER_IDENTIFIER9 identity = {};
    if (host_ == nullptr || caps_capacity < sizeof(answer)) return true;
    hresult = static_cast<uint32_t>(host_->caps(answer));
    if (hresult == D3D_OK) hresult = static_cast<uint32_t>(host_->identity(identity));
    if (hresult != D3D_OK) return true;
    vendor_id = identity.VendorId;
    device_id = identity.DeviceId;
    subsystem_id = identity.SubSysId;
    revision = identity.Revision;
    video_memory_mb = host_->video_memory_mb();
    memcpy(caps, &answer, sizeof(answer));
    caps_size = sizeof(answer);
    return true;
}

bool D3d9Session::on_QueryFormats9(uint32_t count, const uint32_t *formats, uint8_t *support,
                                   uint64_t support_capacity, uint64_t &support_size)
{
    AV_SCOPE("D3d9Session::on_QueryFormats9");
    support_size = 0u;
    if (host_ == nullptr || support_capacity < count * format_answer_bytes) return false;
    for (uint32_t index = 0u; index < count; index++) {
        uint32_t words[2] = {};
        host_->format_support(formats[index], words[0], words[1]);
        memcpy(support + index * format_answer_bytes, words, sizeof(words));
    }
    support_size = count * format_answer_bytes;
    return true;
}

bool D3d9Session::on_QueryMultisample9(uint32_t format, uint32_t multisample, uint32_t &hresult,
                                       uint32_t &quality_levels)
{
    AV_SCOPE("D3d9Session::on_QueryMultisample9");
    quality_levels = 0u;
    hresult = static_cast<uint32_t>(D3DERR_NOTAVAILABLE);
    if (host_ != nullptr) hresult = static_cast<uint32_t>(host_->multisample(format, multisample, quality_levels));
    return true;
}

/* With no device selected - the one that was is destroyed - there is nothing
 * to flush, which is no reason to fault the session (the guest flushes as
 * its devices go). */
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

/* Every command ran as it arrived; what is left is DXVK's, which a read of
 * the results waits for itself. */
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
        device->Release();
        return true;
    }
    /* The session's table holds every runtime's objects: only Direct3D 9's
     * are Destroy9's to remove. */
    D3dObjectKind kind = D3dObjectKind::none;
    if (objects_.find_any(object, kind) == nullptr || kind < D3dObjectKind::surface9) return false;
    objects_.remove(object);
    return true;
}

bool D3d9Session::keep(uint32_t id, D3dObjectKind kind, IUnknown *made)
{
    AV_SCOPE("D3d9Session::keep");
    /* A device's id is in the objects' space too. */
    if (made != nullptr && devices_.find(id) != nullptr) {
        made->Release();
        return false;
    }
    return objects_.keep(id, kind, made);
}

/* Each kind asks its own interface which device made it: every Direct3D 9
 * object has GetDevice, but not on a common base. */
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

bool D3d9Session::invariants_hold() const
{
    AV_SCOPE("D3d9Session::invariants_hold");
    return devices_.invariants_hold() && objects_.invariants_hold();
}

}  // namespace av
