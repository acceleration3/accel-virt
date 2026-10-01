#include "device/executor/d3d9_session.hpp"

#include "core/trace/function_scope.hpp"
#include "device/executor/guest_region.hpp"
#include "device/shim/d3d9_host_device.hpp"
#include "device/shim/d3d_headers.hpp"
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
    : region_(region), host_(nullptr), surfaces_(nullptr), objects_(objects), devices_{}, selected_(nullptr), refusal_("d3d9"), arena_(nullptr)
{
    AV_SCOPE("D3d9Session::D3d9Session");
}

D3d9Session::~D3d9Session()
{
    AV_SCOPE("D3d9Session::~D3d9Session");
    shut_down();
}

void D3d9Session::initialize(D3d9HostDevice *host, SharedSurfaces *surfaces)
{
    AV_SCOPE("D3d9Session::initialize");
    host_ = host != nullptr && host->opened() ? host : nullptr;
    surfaces_ = surfaces;
}

/* Objects first: each holds a reference to the device that made it. Only
 * Direct3D 9's: the table is the whole session's. */
void D3d9Session::shut_down()
{
    AV_SCOPE("D3d9Session::shut_down");
    for (uint32_t slot = objects_.next_used(0u); slot < D3dObjectTable::capacity;
         slot = objects_.next_used(slot + 1u)) {
        uint32_t      id = 0u;
        D3dObjectKind kind = D3dObjectKind::none;
        static_cast<void>(objects_.at(slot, id, kind));
        if (kind >= D3dObjectKind::surface9) objects_.remove(id);
    }
    for (Device &device : devices_) {
        if (device.device_ != nullptr) device.device_->Release();
        device = Device{};
    }
    selected_ = nullptr;
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
    if (ran) return true;
    const uint32_t device = selected_ == nullptr ? 0u : selected_->id_;
    if (!refusal_.removes(d3d9proto::command_name(command_id), device,
                          selected_ != nullptr && selected_->removed_ != 0u))
        return false;
    selected_->removed_ = static_cast<uint32_t>(D3DERR_DRIVERINTERNALERROR);
    return true;
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

D3d9Session::Device *D3d9Session::device_named(uint32_t id)
{
    AV_SCOPE("D3d9Session::device_named");
    if (id == 0u) return nullptr;
    for (Device &device : devices_)
        if (device.id_ == id) return &device;
    return nullptr;
}

D3d9Session::Device *D3d9Session::free_slot()
{
    AV_SCOPE("D3d9Session::free_slot");
    for (Device &device : devices_)
        if (device.id_ == 0u) return &device;
    return nullptr;
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
    if (device == 0u || device_named(device) != nullptr || objects_.find_any(device, taken) != nullptr) return true;
    Device *const slot = free_slot();
    hresult = static_cast<uint32_t>(D3DERR_OUTOFVIDEOMEMORY);
    if (slot == nullptr) return true;
    hresult = static_cast<uint32_t>(D3DERR_NOTAVAILABLE);
    if (host_ == nullptr) return true;
    IDirect3DDevice9 *made = nullptr;
    hresult = static_cast<uint32_t>(host_->make_device(made));
    if (made == nullptr) return true;
    made->BeginScene();
    *slot = Device{device, made, 0u};
    selected_ = slot;
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
    if (selected_ == nullptr) return true;
    selected_->device_->EndScene();
    selected_->device_->BeginScene();
    return true;
}

bool D3d9Session::on_SelectDevice9(uint32_t device)
{
    AV_SCOPE("D3d9Session::on_SelectDevice9");
    Device *const named = device_named(device);
    if (named == nullptr) return false;
    selected_ = named;
    return true;
}

/* Every command ran as it arrived; what is left is DXVK's, which a read of
 * the results waits for itself. */
bool D3d9Session::on_Finish9(uint32_t &hresult)
{
    AV_SCOPE("D3d9Session::on_Finish9");
    hresult = selected_ != nullptr ? selected_->removed_ : static_cast<uint32_t>(D3DERR_INVALIDCALL);
    return true;
}

bool D3d9Session::on_Destroy9(uint32_t object)
{
    AV_SCOPE("D3d9Session::on_Destroy9");
    Device *const device = device_named(object);
    if (device != nullptr) {
        if (selected_ == device) selected_ = nullptr;
        device->device_->Release();
        *device = Device{};
        return true;
    }
    D3dObjectKind kind = D3dObjectKind::none;
    if (objects_.find_any(object, kind) == nullptr) return false;
    objects_.remove(object);
    return true;
}

bool D3d9Session::keep(uint32_t id, D3dObjectKind kind, IUnknown *made)
{
    AV_SCOPE("D3d9Session::keep");
    if (made == nullptr) return false;
    if (device_named(id) == nullptr && objects_.insert(id, kind, made)) return true;
    made->Release();
    return false;
}

/* Each kind asks its own interface which device made it: every Direct3D 9
 * object has GetDevice, but not on a common base. */
IUnknown *D3d9Session::owned(uint32_t id, D3dObjectKind kind) const
{
    AV_SCOPE("D3d9Session::owned");
    IUnknown *const found = selected_ != nullptr ? objects_.find(id, kind) : nullptr;
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
    return made_by == selected_->device_ ? found : nullptr;
}

bool D3d9Session::invariants_hold() const
{
    AV_SCOPE("D3d9Session::invariants_hold");
    bool selected_listed = selected_ == nullptr;
    for (uint32_t one = 0u; one < max_devices; one++) {
        const Device &device = devices_[one];
        if (&device == selected_) selected_listed = device.id_ != 0u;
        if ((device.id_ == 0u) != (device.device_ == nullptr)) return false;
        for (uint32_t other = one + 1u; other < max_devices && device.id_ != 0u; other++)
            if (devices_[other].id_ == device.id_) return false;
    }
    return selected_listed && objects_.invariants_hold();
}

}  // namespace av
