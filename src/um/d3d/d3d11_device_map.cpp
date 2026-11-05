#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "core/log/keyed_log.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"
#include "um/d3d/d3d11_device.hpp"
#include "um/d3d/d3d_session.hpp"

namespace av::d3d11 {

namespace {
constexpr uint32_t copy_no_overwrite = 0x1u;
constexpr uint32_t copy_discard = 0x2u;

}

void D3d11Device::map(D3D10DDI_HRESOURCE handle, UINT subresource, D3D10_DDI_MAP type, UINT,
                      D3D10DDI_MAPPED_SUBRESOURCE *mapped)
{
    AV_SCOPE("D3d11Device::map");
    ResourceRecord &resource = record(handle);
    if (resource.shadow == nullptr || subresource >= subresource_count(resource) || mapped == nullptr) {
        set_error(E_INVALIDARG);
        return;
    }
    if (parent_ != nullptr && resource.shadow_local == 0u && !own_shadow(resource)) {
        set_error(E_OUTOFMEMORY);
        return;
    }
    if (resource.on_slice != 0u && type == D3D10_DDI_MAP_WRITE_DISCARD && !rename_slice(resource)) {
        lines_.once("no slice free for a discarding map: the buffer is copied from then on");
        if (!leave_slice_for_copies(resource)) {
            set_error(E_OUTOFMEMORY);
            return;
        }
    }
    if (resource.on_slice != 0u) {
        mapped->pData = resource.shadow;
        mapped->RowPitch = static_cast<UINT>(resource.shadow_bytes);
        mapped->DepthPitch = static_cast<UINT>(resource.shadow_bytes);
        return;
    }
    if (resource.dimension == D3D10DDIRESOURCE_BUFFER && (resource.cpu_access & D3D10_DDI_CPU_ACCESS_WRITE) != 0u &&
        type != D3D10_DDI_MAP_READ && session_.slices() == nullptr)
        wire().note(ChannelCounter::copied_mappings, 1u);
    const uint32_t flags = type == D3D10_DDI_MAP_WRITE_DISCARD      ? copy_discard
                           : type == D3D10_DDI_MAP_WRITE_NOOVERWRITE ? copy_no_overwrite
                                                                     : 0u;
    const size_t   offset = subresource_offset(resource, subresource);
    uint8_t *const at = resource.shadow + offset;
    const size_t   bytes = subresource_bytes(resource, subresource);
    if (type != D3D10_DDI_MAP_READ && !open_map(resource, subresource, offset, bytes, flags)) {
        lines_.once("a map the device had no memory to hold open for writing");
        set_error(E_OUTOFMEMORY);
        return;
    }
    if ((type == D3D10_DDI_MAP_READ || type == D3D10_DDI_MAP_READWRITE) && !read_back(resource, subresource, at, bytes)) {
        LogLine(lines_)
            .text("a map could not read back ")
            .number(bytes)
            .text(" bytes of format ")
            .number(resource.format)
            .write();
        lost(__func__);
        return;
    }
    if ((type == D3D10_DDI_MAP_READ || type == D3D10_DDI_MAP_READWRITE) && resource.shadow_watched != 0u)
        shadows_.forget_writes(at, bytes);
    uint32_t width = 0u, height = 0u, depth = 0u;
    extent(resource, subresource, width, height, depth);
    mapped->pData = at;
    mapped->RowPitch = row_bytes(resource, width);
    mapped->DepthPitch = mapped->RowPitch * rows(resource, height);
}

void D3d11Device::unmap(D3D10DDI_HRESOURCE handle, UINT subresource)
{
    AV_SCOPE("D3d11Device::unmap");
    ResourceRecord &resource = record(handle);
    if (resource.on_slice != 0u || resource.shadow == nullptr) return;
    icd::StagedMapping *const open = maps_.find(resource.stamp.id(), subresource);
    if (open == nullptr) return;
    const uint32_t flags = open->flags();
    maps_.close(*open);
    if (resource.shadow_watched != 0u && upload_written(resource, flags)) return;
    uint32_t width = 0u, height = 0u, depth = 0u;
    extent(resource, subresource, width, height, depth);
    const uint32_t line = row_bytes(resource, width);
    upload(resource, subresource, nullptr, resource.shadow + subresource_offset(resource, subresource), line,
           line * rows(resource, height), flags);
}

bool D3d11Device::open_map(const ResourceRecord &resource, uint32_t subresource, size_t offset, size_t bytes,
                           uint32_t flags)
{
    AV_SCOPE("D3d11Device::open_map");
    icd::StagedMapping *const open = maps_.find(resource.stamp.id(), subresource);
    if (open != nullptr) {
        open->reflag(flags);
        return true;
    }
    icd::StagedMapping *opened = nullptr;
    return maps_.lend(icd::StagedMapping(resource.stamp.id(), subresource, offset, bytes, flags),
                      resource.shadow + offset, opened) == Status::ok;
}

void D3d11Device::forget_maps(uint32_t id)
{
    AV_SCOPE("D3d11Device::forget_maps");
    static_cast<void>(maps_.close_if([id](const icd::StagedMapping &open) { return open.object() == id; }));
}

bool D3d11Device::upload_written(const ResourceRecord &resource, uint32_t flags)
{
    AV_SCOPE("D3d11Device::upload_written");
    if (!shadows_.take_written(resource.shadow, resource.shadow_bytes, written_)) return false;
    for (uint32_t index = 0u; index < written_.count(); index++) {
        const icd::WrittenRuns::Run run = written_.run(index);
        const D3D10_DDI_BOX box = {static_cast<LONG>(run.offset_), 0, 0, static_cast<LONG>(run.offset_ + run.bytes_), 1, 1};
        upload(resource, 0u, &box, resource.shadow + run.offset_, 0u, 0u, index == 0u ? flags : flags & ~copy_discard);
    }
    return true;
}

VOID D3d11Device::resource_map(D3D10DDI_HRESOURCE resource, UINT sub, D3D10_DDI_MAP type, UINT flags,
                               D3D10DDI_MAPPED_SUBRESOURCE *mapped)
{
    AV_SCOPE("D3d11Device::resource_map");
    map(resource, sub, type, flags, mapped);
}

VOID D3d11Device::resource_unmap(D3D10DDI_HRESOURCE resource, UINT sub)
{
    AV_SCOPE("D3d11Device::resource_unmap");
    unmap(resource, sub);
}

VOID D3d11Device::staging_resource_map(D3D10DDI_HRESOURCE resource, UINT sub, D3D10_DDI_MAP type, UINT flags,
                                       D3D10DDI_MAPPED_SUBRESOURCE *mapped)
{
    AV_SCOPE("D3d11Device::staging_resource_map");
    map(resource, sub, type, flags, mapped);
}

VOID D3d11Device::staging_resource_unmap(D3D10DDI_HRESOURCE resource, UINT sub)
{
    AV_SCOPE("D3d11Device::staging_resource_unmap");
    unmap(resource, sub);
}

VOID D3d11Device::dynamic_ia_buffer_map_no_overwrite(D3D10DDI_HRESOURCE resource, UINT sub, D3D10_DDI_MAP type,
                                                     UINT flags, D3D10DDI_MAPPED_SUBRESOURCE *mapped)
{
    AV_SCOPE("D3d11Device::dynamic_ia_buffer_map_no_overwrite");
    map(resource, sub, type, flags, mapped);
}

VOID D3d11Device::dynamic_ia_buffer_map_discard(D3D10DDI_HRESOURCE resource, UINT sub, D3D10_DDI_MAP type, UINT flags,
                                                D3D10DDI_MAPPED_SUBRESOURCE *mapped)
{
    AV_SCOPE("D3d11Device::dynamic_ia_buffer_map_discard");
    map(resource, sub, type, flags, mapped);
}

VOID D3d11Device::dynamic_ia_buffer_unmap(D3D10DDI_HRESOURCE resource, UINT sub)
{
    AV_SCOPE("D3d11Device::dynamic_ia_buffer_unmap");
    unmap(resource, sub);
}

VOID D3d11Device::dynamic_constant_buffer_map_discard(D3D10DDI_HRESOURCE resource, UINT sub, D3D10_DDI_MAP type,
                                                      UINT flags, D3D10DDI_MAPPED_SUBRESOURCE *mapped)
{
    AV_SCOPE("D3d11Device::dynamic_constant_buffer_map_discard");
    map(resource, sub, type, flags, mapped);
}

VOID D3d11Device::dynamic_constant_buffer_map_no_overwrite(D3D10DDI_HRESOURCE resource, UINT sub, D3D10_DDI_MAP type,
                                                           UINT flags, D3D10DDI_MAPPED_SUBRESOURCE *mapped)
{
    AV_SCOPE("D3d11Device::dynamic_constant_buffer_map_no_overwrite");
    map(resource, sub, type, flags, mapped);
}

VOID D3d11Device::dynamic_constant_buffer_unmap(D3D10DDI_HRESOURCE resource, UINT sub)
{
    AV_SCOPE("D3d11Device::dynamic_constant_buffer_unmap");
    unmap(resource, sub);
}

VOID D3d11Device::dynamic_resource_map_discard(D3D10DDI_HRESOURCE resource, UINT sub, D3D10_DDI_MAP type, UINT flags,
                                               D3D10DDI_MAPPED_SUBRESOURCE *mapped)
{
    AV_SCOPE("D3d11Device::dynamic_resource_map_discard");
    map(resource, sub, type, flags, mapped);
}

VOID D3d11Device::dynamic_resource_unmap(D3D10DDI_HRESOURCE resource, UINT sub)
{
    AV_SCOPE("D3d11Device::dynamic_resource_unmap");
    unmap(resource, sub);
}

BOOL D3d11Device::resource_is_staging_busy(D3D10DDI_HRESOURCE)
{
    AV_SCOPE("D3d11Device::resource_is_staging_busy");
    return FALSE;
}

VOID D3d11Device::resource_update_subresource_up(D3D10DDI_HRESOURCE handle, UINT subresource, const D3D10_DDI_BOX *box,
                                                 const void *data, UINT row_pitch, UINT depth_pitch, UINT flags)
{
    AV_SCOPE("D3d11Device::resource_update_subresource_up");
    const ResourceRecord &resource = record(handle);
    upload(resource, subresource, box, data, row_pitch, depth_pitch, flags);
}

VOID D3d11Device::default_constant_buffer_update_subresource_up(D3D10DDI_HRESOURCE handle, UINT subresource,
                                                                const D3D10_DDI_BOX *box, const void *data,
                                                                UINT row_pitch, UINT depth_pitch, UINT flags)
{
    AV_SCOPE("D3d11Device::default_constant_buffer_update_subresource_up");
    resource_update_subresource_up(handle, subresource, box, data, row_pitch, depth_pitch, flags);
}

bool D3d11Device::place_on_slice(ResourceRecord &resource, size_t bytes)
{
    AV_SCOPE("D3d11Device::place_on_slice");
    if (resource.dimension != D3D10DDIRESOURCE_BUFFER || (resource.cpu_access & D3D10_DDI_CPU_ACCESS_WRITE) == 0u ||
        !slices_.place(bytes, resource.slice))
        return false;
    resource.on_slice = 1u;
    resource.shadow = session_.slice_view() + resource.slice.offset_;
    return true;
}

bool D3d11Device::rename_slice(ResourceRecord &resource)
{
    AV_SCOPE("D3d11Device::rename_slice");
    if (!slices_.rename(resource.shadow_bytes, resource.slice)) return false;
    resource.shadow = session_.slice_view() + resource.slice.offset_;
    return bind_slice(resource);
}

bool D3d11Device::own_shadow(ResourceRecord &resource)
{
    AV_SCOPE("D3d11Device::own_shadow");
    bool     watched = false;
    uint8_t *shadow = resource.dimension == D3D10DDIRESOURCE_BUFFER
                          ? shadows_.allocate(resource.shadow_bytes, watched)
                          : static_cast<uint8_t *>(memory_.allocate(resource.shadow_bytes, Pool::paged));
    if (shadow == nullptr) return false;
    if (resource.on_slice != 0u) leave_slice(resource);
    resource.shadow = shadow;
    resource.shadow_watched = watched ? 1u : 0u;
    resource.shadow_local = 1u;
    return true;
}

void D3d11Device::forget_copy(ResourceRecord &resource)
{
    AV_SCOPE("D3d11Device::forget_copy");
    forget_maps(resource.stamp.id());
    if (resource.shadow_local != 0u) shadows_.release(resource.shadow, resource.shadow_watched != 0u);
    resource = ResourceRecord{};
}

bool D3d11Device::leave_slice_for_copies(ResourceRecord &resource)
{
    AV_SCOPE("D3d11Device::leave_slice_for_copies");
    bool     watched = false;
    uint8_t *shadow = shadows_.allocate(resource.shadow_bytes, watched);
    if (shadow == nullptr) return false;
    __builtin_memcpy(shadow, resource.shadow, resource.shadow_bytes);
    if (watched) shadows_.forget_writes(shadow, resource.shadow_bytes);
    leave_slice(resource);
    resource.shadow = shadow;
    resource.shadow_watched = watched ? 1u : 0u;
    return true;
}

bool D3d11Device::bind_slice(const ResourceRecord &resource)
{
    AV_SCOPE("D3d11Device::bind_slice");
    return d3dproto::av_d3d_BindBufferSlice(wire(), resource.stamp.id(), session_.slice_memory(), resource.slice.offset_,
                                            resource.slice.bytes_);
}

void D3d11Device::leave_slice(ResourceRecord &resource)
{
    AV_SCOPE("D3d11Device::leave_slice");
    slices_.leave(resource.slice);
    resource.on_slice = 0u;
    resource.shadow = nullptr;
}

}
