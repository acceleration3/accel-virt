#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "core/log/keyed_log.hpp"
#include "icd/d3d/d3d_device_channel.hpp"
#include "icd/d3d/kernel_surface.hpp"
#include "proto/d3d/d3d9_present_format.hpp"
#include "proto/resources/pitched_rows.hpp"
#include "proto/generated/d3d9/d3d9_dispatch_client.h"
#include "proto/escape/next_point_record.hpp"
#include "um/d3d/d3d9_device.hpp"
#include "um/d3d/d3d_session.hpp"
#include "um/d3d/d3d9_host_caps.hpp"

#include <string.h>

namespace av::d3d9 {

HRESULT D3d9Device::follow_host()
{
    AV_SCOPE("D3d9Device::follow_host");
    return present_order_.constructed() ? present_order_.object().follow() : E_FAIL;
}

HRESULT D3d9Device::flush()
{
    AV_SCOPE("D3d9Device::flush");
    if (!channel_.constructed()) return S_OK;
    if (!send_states() || wire().application_flush() != Status::ok) return lost(__func__);
    const HRESULT followed = follow_host();
    trim_memory();
    return followed;
}

HRESULT D3d9Device::allocate(D3d9Resource &resource)
{
    AV_SCOPE("D3d9Device::allocate");
    const D3d9Subresource &top = resource.subresources[0];
    D3d9PresentFormat presented(0u);
    if (!presented.take_ddi(static_cast<uint32_t>(resource.format))) return E_INVALIDARG;
    d3d::KernelSurface surface(top.width, top.height, presented.dxgi());
    if (surface.make() != Status::ok) return E_INVALIDARG;
    D3DKMT_HANDLE allocation = 0u;
    const HRESULT made = allocations_.make(surface, nullptr, allocation);
    if (FAILED(made)) return made;
    resource.allocation = allocation;
    resource.allocation_pitch = surface.pitch();
    resource.shared_key = surface.key();
    return S_OK;
}

HRESULT D3d9Device::allocate_primary(D3d9Resource &resource, const D3DDDIARG_CREATERESOURCE &create)
{
    AV_SCOPE("D3d9Device::allocate_primary");
    const D3d9Subresource &top = resource.subresources[0];
    D3d9PresentFormat presented(0u);
    if (!presented.take_ddi(static_cast<uint32_t>(resource.format))) return E_INVALIDARG;
    d3d::KernelSurface surface(top.width, top.height, presented.dxgi(),
                               ScanoutTarget(create.VidPnSourceId, create.RefreshRate.Numerator,
                                             create.RefreshRate.Denominator));
    if (surface.make() != Status::ok) return E_INVALIDARG;
    D3DKMT_HANDLE allocation = 0u;
    const HRESULT made = allocations_.make(surface, create.hResource, allocation);
    if (FAILED(made)) return made;
    resource.allocation = allocation;
    resource.allocation_resource = create.hResource;
    resource.allocation_pitch = surface.pitch();
    resource.shared_key = surface.key();
    return resource.shared_key != 0u ? show_primary(resource, 0u) : S_OK;
}

HRESULT D3d9Device::show_primary(D3d9Resource &primary, uint32_t index)
{
    AV_SCOPE("D3d9Device::show_primary");
    uint32_t id = 0u, subresource = 0u;
    if (!surface_of(primary, index, id, subresource)) return E_INVALIDARG;
    if (!d3d9proto::av_d3d9_PresentPoint9(wire(), id, subresource, primary.shared_key, 0u) ||
        wire().flush() != Status::ok)
        return lost(__func__);
    return follow_host();
}

HRESULT D3d9Device::publish_presented(D3d9Resource &resource, uint32_t index)
{
    AV_SCOPE("D3d9Device::publish_presented");
    if (resource.allocation_resource != nullptr && resource.shared_key != 0u) return show_primary(resource, index);
    return publish_point(resource, index);
}

HRESULT D3d9Device::publish_point(D3d9Resource &resource, uint32_t index)
{
    AV_SCOPE("D3d9Device::publish_point");
    const bool staged_format = resource.format == D3DDDIFMT_X8R8G8B8 || resource.format == D3DDDIFMT_A8R8G8B8;
    uint32_t   id = 0u, subresource = 0u;
    if (!caps_.present_snapshots() || resource.shared_key == 0u || !staged_format ||
        !surface_of(resource, index, id, subresource))
        return publish(resource, index);
    const uint32_t point = resource.presents.next();
    if (!d3d9proto::av_d3d9_PresentPoint9(wire(), id, subresource, resource.shared_key, point) ||
        wire().flush() != Status::ok)
        return lost(__func__);
    const Status named = send_point(resource, point, NextPointRecord::Action::name);
    if (named == Status::ok) {
        resource.named_point = point;
        return S_OK;
    }
    if (lines_.first("a Direct3D 9 point the miniport refused", static_cast<int64_t>(named)))
        LogLine(lines_)
            .text("the miniport took no point for a Direct3D 9 present, status ")
            .number(static_cast<uint32_t>(named))
            .text("; its frames are read back")
            .write();
    static_cast<void>(send_point(resource, 0u, NextPointRecord::Action::pages));
    return publish(resource, index);
}

Status D3d9Device::send_point(const D3d9Resource &resource, uint32_t point, NextPointRecord::Action action)
{
    AV_SCOPE("D3d9Device::send_point");
    uint8_t record[NextPointRecord::encoded_size] = {};
    if (NextPointRecord(resource.shared_key, point, action).encode(record, sizeof record) != Status::ok)
        return Status::invalid_parameter;
    return session_.escape(record, sizeof record);
}

void D3d9Device::withdraw_point(D3d9Resource &resource)
{
    AV_SCOPE("D3d9Device::withdraw_point");
    if (resource.named_point == 0u) return;
    static_cast<void>(send_point(resource, resource.named_point, NextPointRecord::Action::withdraw));
    resource.named_point = 0u;
}

HRESULT D3d9Device::set_display_mode(const D3DDDIARG_SETDISPLAYMODE *args)
{
    AV_SCOPE("D3d9Device::set_display_mode");
    D3d9Resource *const primary = resource(args->hResource);
    if (primary == nullptr || args->SubResourceIndex >= primary->count || primary->allocation == 0u)
        return E_INVALIDARG;
    const HRESULT published = publish_presented(*primary, args->SubResourceIndex);
    if (FAILED(published)) return published;
    D3DDDICB_SETDISPLAYMODE mode = {};
    mode.hPrimaryAllocation = primary->allocation;
    return kernel_.set_display_mode(mode);
}

HRESULT D3d9Device::publish(D3d9Resource &resource, uint32_t index)
{
    AV_SCOPE("D3d9Device::publish");
    uint32_t      pitch = 0u, rows = 0u;
    const HRESULT read = read_surface(resource, index, pitch, rows);
    if (FAILED(read)) return read;
    auto *const pixels = static_cast<uint8_t *>(kernel_.lock_to_write(resource.allocation));
    if (pixels == nullptr) return E_FAIL;
    const uint32_t height = resource.subresources[index].height;
    PitchedRows(pixels, resource.allocation_pitch, size_t{height} * resource.allocation_pitch)
        .copy_from(PitchedRows(readback_, pitch, size_t{rows} * pitch), rows);
    kernel_.unlock(resource.allocation);
    return S_OK;
}

HRESULT D3d9Device::present(const D3DDDIARG_PRESENT *args)
{
    AV_SCOPE("D3d9Device::present");
    D3d9Resource *const source = resource(args->hSrcResource);
    if (source == nullptr || args->SrcSubResourceIndex >= source->count) return E_INVALIDARG;
    const D3d9Resource *const destination = resource(args->hDstResource);
    const uint64_t began = clock_.microseconds();
    trace("present", static_cast<uint32_t>(busy_), calls_, 0u,
          static_cast<uint32_t>(began / 1000u));
    trace("frame", draws_[0] | (draws_[1] << 16u), draws_[2] | (draws_[3] << 16u), rebased_ | (null_shaders_ << 16u),
          static_cast<uint32_t>(written_ / 1024u));
    null_shaders_ = 0u;
    busy_ = 0u;
    calls_ = 0u;
    draws_[0] = draws_[1] = draws_[2] = draws_[3] = 0u;
    rebased_ = 0u;
    written_ = 0u;
    if (source->allocation == 0u) {
        const HRESULT made = allocate(*source);
        if (FAILED(made)) {
            LogLine(lines_)
                .text("a Direct3D 9 buffer of format ")
                .number(static_cast<uint32_t>(source->format))
                .text(" has no allocation to present ")
                .hex(static_cast<uint32_t>(made), 8u)
                .write();
            return made;
        }
    }
    const HRESULT published = publish_presented(*source, args->SrcSubResourceIndex);
    if (FAILED(published)) {
        lines_.once("a Direct3D 9 present whose buffer could not be read back");
        return published;
    }
    trim_memory();
    const HRESULT followed = follow_host();
    if (FAILED(followed)) {
        withdraw_point(*source);
        return followed;
    }
    D3DDDICB_PRESENT present = {};
    present.hSrcAllocation = source->allocation;
    present.hDstAllocation = destination == nullptr ? 0u : destination->allocation;
    present.hContext = context_;
    const HRESULT presented = present_order_.object().submitted(kernel_.present(present),
        source->allocation_resource != nullptr ? d3d::PresentOrder::Reuse::after_context
                                               : d3d::PresentOrder::Reuse::immediate);
    if (FAILED(presented)) withdraw_point(*source);
    source->named_point = 0u;
    trace("presented", static_cast<uint32_t>(presented), static_cast<uint32_t>(clock_.microseconds() - began), 0u, 0u);
    return presented;
}

}
