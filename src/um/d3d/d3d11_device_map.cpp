/* The CPU's way into a resource: a map hands the application the resource's
 * shadow in guest memory, filled from the host first when it is read, and an
 * unmap after a write carries the subresource back to the host. The host's
 * UpdateSubresource runs in command order, so a draw recorded before the
 * unmap still reads the old contents - which is what NO_OVERWRITE promises
 * and DISCARD allows. */
#include "core/trace/function_scope.hpp"
#include "um/d3d/d3d11_device.hpp"

namespace av::d3d11 {

namespace {

/* D3D11_COPY_FLAGS, which the host's UpdateSubresource1 takes as the hint a
 * map gave. */
constexpr uint32_t copy_no_overwrite = 0x1u;
constexpr uint32_t copy_discard = 0x2u;
constexpr uint32_t tracked_subresources = 32u;

}  // namespace

void D3d11Device::map(D3D10DDI_HRESOURCE handle, UINT subresource, D3D10_DDI_MAP type, UINT,
                      D3D10DDI_MAPPED_SUBRESOURCE *mapped)
{
    AV_SCOPE("D3d11Device::map");
    ResourceRecord &resource = record(handle);
    if (resource.shadow == nullptr || subresource >= subresource_count(resource) || mapped == nullptr) {
        set_error(E_INVALIDARG);
        return;
    }
    uint8_t *const at = resource.shadow + subresource_offset(resource, subresource);
    const size_t bytes = subresource_bytes(resource, subresource);
    if ((type == D3D10_DDI_MAP_READ || type == D3D10_DDI_MAP_READWRITE) && !read_back(resource, subresource, at, bytes)) {
        set_error(D3DDDIERR_DEVICEREMOVED);
        return;
    }
    uint32_t width = 0u, height = 0u, depth = 0u;
    extent(resource, subresource, width, height, depth);
    mapped->pData = at;
    mapped->RowPitch = row_bytes(resource, width);
    mapped->DepthPitch = mapped->RowPitch * rows(resource, height);
    if (type != D3D10_DDI_MAP_READ && subresource < tracked_subresources) {
        resource.mapped_write |= 1u << subresource;
        resource.reserved = type == D3D10_DDI_MAP_WRITE_DISCARD      ? copy_discard
                            : type == D3D10_DDI_MAP_WRITE_NOOVERWRITE ? copy_no_overwrite
                                                                      : 0u;
    }
}

void D3d11Device::unmap(D3D10DDI_HRESOURCE handle, UINT subresource)
{
    AV_SCOPE("D3d11Device::unmap");
    ResourceRecord &resource = record(handle);
    if (resource.shadow == nullptr || subresource >= tracked_subresources) return;
    if ((resource.mapped_write & (1u << subresource)) == 0u) return;
    resource.mapped_write &= ~(1u << subresource);
    uint32_t width = 0u, height = 0u, depth = 0u;
    extent(resource, subresource, width, height, depth);
    const uint32_t line = row_bytes(resource, width);
    upload(resource.id, subresource, nullptr, resource.shadow + subresource_offset(resource, subresource),
           subresource_bytes(resource, subresource), line, line * rows(resource, height), resource.reserved);
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
    upload(resource.id, subresource, box, data, upload_bytes(resource, subresource, box, row_pitch, depth_pitch),
           row_pitch, depth_pitch, flags);
}

VOID D3d11Device::default_constant_buffer_update_subresource_up(D3D10DDI_HRESOURCE handle, UINT subresource,
                                                                const D3D10_DDI_BOX *box, const void *data,
                                                                UINT row_pitch, UINT depth_pitch, UINT flags)
{
    AV_SCOPE("D3d11Device::default_constant_buffer_update_subresource_up");
    resource_update_subresource_up(handle, subresource, box, data, row_pitch, depth_pitch, flags);
}

}  // namespace av::d3d11
