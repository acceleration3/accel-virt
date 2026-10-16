/* The creation slots: every object the runtime makes is made on the host under
 * an id this device hands out, and its record kept in the runtime's block. */
#include "core/platform/block_source.hpp"
#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/transport/shared_object_call.hpp"
#include "core/log/log_sink.hpp"
#include "icd/d3d/d3d_command_stream.hpp"
#include "icd/d3d/kernel_surface.hpp"
#include "icd/d3d/passthrough_geometry_shader.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"
#include "proto/d3d/resource_misc.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "icd/d3d/shared_rows_request.hpp"
#include "um/d3d/d3d11_device.hpp"
#include "um/d3d/d3d_session.hpp"

namespace av::d3d11 {

namespace {

/* D3D11_RESOURCE_MISC_* the host keeps, by the DDI's bit. */
constexpr uint32_t api_generate_mips = 0x1u;
constexpr uint32_t api_texture_cube = 0x4u;
constexpr uint32_t misc_same_bits = 0xF0u; /* draw-indirect, raw views, structured, clamp */
constexpr uint32_t api_bind_uav = 0x80u;
constexpr uint32_t api_bind_pipeline = 0x7Fu;

/* D3D11_{SRV,RTV,DSV,UAV}_DIMENSION values. */
constexpr uint32_t view_buffer = 1u;
constexpr uint32_t view_1d = 2u;
constexpr uint32_t view_1d_array = 3u;
constexpr uint32_t view_2d = 4u;
constexpr uint32_t view_2d_array = 5u;
constexpr uint32_t view_2d_ms = 6u;
constexpr uint32_t view_2d_ms_array = 7u;
constexpr uint32_t view_3d = 8u;
constexpr uint32_t srv_cube = 9u;
constexpr uint32_t srv_cube_array = 10u;
constexpr uint32_t srv_buffer_ex = 11u;
constexpr uint32_t dsv_1d = 1u;
constexpr uint32_t dsv_1d_array = 2u;
constexpr uint32_t dsv_2d = 3u;
constexpr uint32_t dsv_2d_array = 4u;
constexpr uint32_t dsv_2d_ms = 5u;
constexpr uint32_t dsv_2d_ms_array = 6u;

constexpr uint32_t stage_vertex = 0u;
constexpr uint32_t stage_hull = 1u;
constexpr uint32_t stage_domain = 2u;
constexpr uint32_t stage_geometry = 3u;
constexpr uint32_t stage_pixel = 4u;
constexpr uint32_t stage_compute = 5u;
constexpr uint32_t no_stream = 0xffffffffu;

}  // namespace

/* ---------------------------------------------------------------- resources */

SIZE_T D3d11Device::calc_private_resource_size(const D3D11DDIARG_CREATERESOURCE *)
{
    AV_SCOPE("D3d11Device::calc_private_resource_size");
    return sizeof(ResourceRecord);
}

SIZE_T D3d11Device::calc_private_opened_resource_size(const D3D10DDIARG_OPENRESOURCE *)
{
    AV_SCOPE("D3d11Device::calc_private_opened_resource_size");
    return sizeof(ResourceRecord);
}

VOID D3d11Device::create_resource(const D3D11DDIARG_CREATERESOURCE *args, D3D10DDI_HRESOURCE handle,
                                  D3D10DDI_HRTRESOURCE runtime)
{
    AV_SCOPE("D3d11Device::create_resource");
    if (context_local(handle.pDrvPrivate, runtime.handle, sizeof(ResourceRecord))) return;
    ResourceRecord &resource = record(handle);
    resource = ResourceRecord{};
    resource.stamp = d3d::RecordStamp(ResourceRecord::tag, next_id());
    resource.dimension = static_cast<uint32_t>(args->ResourceDimension);
    resource.usage = args->Usage;
    resource.bind = args->BindFlags;
    resource.cpu_access = args->MapFlags;
    resource.misc = args->MiscFlags;
    resource.format = static_cast<uint32_t>(args->Format);
    resource.width = args->pMipInfoList[0].TexelWidth;
    resource.height = args->pMipInfoList[0].TexelHeight;
    resource.depth = args->pMipInfoList[0].TexelDepth;
    resource.mips = args->MipLevels;
    const bool cube = args->ResourceDimension == D3D10DDIRESOURCE_TEXTURECUBE;
    /* A cube's ArraySize already counts its faces, six a cube, as the
     * application's TEXTURE2D_DESC did: counted again, a cube's initial data
     * was read for 36 faces from the runtime's six, and Dark Souls
     * Remastered's first environment map faulted in the copy. */
    resource.layers = args->ArraySize;
    resource.samples = args->SampleDesc.Count;
    resource.runtime_resource = runtime.handle;

    d3dproto::ResourceDesc desc = {};
    desc.dimension = cube ? static_cast<uint32_t>(D3D10DDIRESOURCE_TEXTURE2D) : resource.dimension;
    desc.width = resource.width;
    desc.height = resource.height;
    desc.depth_or_array = resource.dimension == D3D10DDIRESOURCE_TEXTURE3D ? resource.depth : resource.layers;
    desc.mip_levels = resource.mips;
    desc.format = resource.format;
    desc.sample_count = args->SampleDesc.Count;
    desc.sample_quality = args->SampleDesc.Quality;
    desc.usage = args->Usage;
    desc.bind_flags = (args->BindFlags & api_bind_pipeline) |
                      ((args->BindFlags & D3D11_DDI_BIND_UNORDERED_ACCESS) != 0u ? api_bind_uav : 0u);
    desc.misc_flags = (args->MiscFlags & misc_same_bits) |
                      ((args->MiscFlags & D3D10_DDI_RESOURCE_AUTO_GEN_MIP_MAP) != 0u ? api_generate_mips : 0u) |
                      (cube ? api_texture_cube : 0u);
    desc.structure_stride = args->ByteStride;
    /* Every resource the kernel will know - scanned out, presented or shared -
     * is made under a host key, so any session on the GPU can name it: DWM
     * opening an application's buffer, and the scanout showing a primary
     * (docs/d3d-desktop.md §3.1). */
    const bool kernel_visible = args->pPrimaryDesc != nullptr || (args->BindFlags & D3D10_DDI_BIND_PRESENT) != 0u ||
                                (args->MiscFlags & D3D10_DDI_RESOURCE_MISC_SHARED) != 0u;
    /* A shared texture with no present or scanout in it is one another API
     * may open (feature 053): the host is told it is shared, and makes it over
     * memory they can import - as rows when the thread making it asked
     * (ResourceMisc), in the GPU's own tiling otherwise. */
    const bool interop = (args->MiscFlags & D3D10_DDI_RESOURCE_MISC_SHARED) != 0u && args->pPrimaryDesc == nullptr &&
                         (args->BindFlags & D3D10_DDI_BIND_PRESENT) == 0u &&
                         resource.dimension == D3D10DDIRESOURCE_TEXTURE2D && resource.mips == 1u &&
                         resource.layers == 1u && args->SampleDesc.Count <= 1u;
    const bool as_rows = interop && rows_.asked_by(GetCurrentThreadId());
    if (interop) desc.misc_flags |= ResourceMisc::shared_bit | (as_rows ? ResourceMisc::rows_bit : 0u);
    /* Allocated first: the key is the identity the miniport mints over the
     * allocation, and the host resource is made under it. */
    if (kernel_visible) {
        const HRESULT allocated = allocate(resource, *args, runtime);
        if (FAILED(allocated)) set_error(allocated);
    }
    if (!d3dproto::av_d3d_CreateResource(wire(), resource.stamp.id(), desc, resource.shared_key)) {
        set_error(E_OUTOFMEMORY);
        return;
    }
    /* Only rows are memory another API can lay an image over. */
    if (as_rows && resource.shared_key != 0u) publish_shared(resource);

    if (args->Usage == D3D10_DDI_USAGE_DYNAMIC || args->Usage == D3D10_DDI_USAGE_STAGING || args->MapFlags != 0u) {
        size_t bytes = 0u;
        for (uint32_t sub = 0u; sub < subresource_count(resource); sub++) bytes += subresource_bytes(resource, sub);
        /* A dynamic buffer is a slice the host draws in place, where the
         * session has them; any other buffer's writes are watched, so an
         * unmap sends only what changed; one subresource, so no page is
         * shared with another's. */
        bool watched = false;
        if (!(args->Usage == D3D10_DDI_USAGE_DYNAMIC && !kernel_visible && place_on_slice(resource, bytes)))
            resource.shadow = resource.dimension == D3D10DDIRESOURCE_BUFFER
                                  ? shadows_.allocate(bytes, watched)
                                  : static_cast<uint8_t *>(memory_.allocate(bytes, Pool::paged));
        resource.shadow_watched = watched ? 1u : 0u;
        resource.shadow_bytes = resource.shadow == nullptr ? 0u : bytes;
        if (resource.shadow == nullptr) set_error(E_OUTOFMEMORY);
    }
    if (args->pInitialDataUP != nullptr) {
        for (uint32_t sub = 0u; sub < subresource_count(resource); sub++) {
            const D3D10_DDIARG_SUBRESOURCE_UP &initial = args->pInitialDataUP[sub];
            /* A slice is read in place: the copy into it below is the upload. */
            if (resource.on_slice == 0u)
                upload(resource, sub, nullptr, initial.pSysMem, initial.SysMemPitch, initial.SysMemSlicePitch, 0u);
            if (resource.shadow == nullptr) continue;
            /* The shadow is packed; the application's rows are at its pitch. */
            uint32_t width = 0u, height = 0u, depth = 0u;
            extent(resource, sub, width, height, depth);
            const uint32_t line = row_bytes(resource, width), lines = rows(resource, height);
            uint8_t *to = resource.shadow + subresource_offset(resource, sub);
            for (uint32_t slice = 0u; slice < depth; slice++)
                for (uint32_t row = 0u; row < lines; row++)
                    __builtin_memcpy(to + (static_cast<size_t>(slice) * lines + row) * line,
                                     static_cast<const uint8_t *>(initial.pSysMem) +
                                         static_cast<size_t>(slice) * initial.SysMemSlicePitch +
                                         static_cast<size_t>(row) * initial.SysMemPitch,
                                     line);
        }
        /* The host was made with this data: it is not a write to send. */
        if (resource.shadow_watched != 0u) shadows_.forget_writes(resource.shadow, resource.shadow_bytes);
    }
    /* Bound once its first contents are in the slice. */
    if (resource.on_slice != 0u && !bind_slice(resource)) set_error(D3DDDIERR_DEVICEREMOVED);

    /* Another process - DWM - may open the key as soon as the kernel hands
     * it the handle, on its own stream: the host must have made the resource
     * by then, contents included. */
    if (kernel_visible && wire().run_held() != Status::ok) set_error(D3DDDIERR_DEVICEREMOVED);
}

/* Published under the key the miniport minted, as the size its allocation
 * descriptor carries - which is what an opener reads back and names. A texture
 * the host could not make shareable is refused, and stays a Direct3D one. */
void D3d11Device::publish_shared(const ResourceRecord &resource)
{
    AV_SCOPE("D3d11Device::publish_shared");
    const uint64_t   bytes = d3d::KernelSurface(resource.width, resource.height, resource.format).bytes();
    SharedObjectCall call(wire());
    if (call.share(resource.shared_key, SharedObjectKind::memory, bytes, resource.stamp.id()) == Status::ok) return;
    LogLine(lines_)
        .text("a shared texture of format ")
        .number(resource.format)
        .text(" cannot be opened by OpenGL or Vulkan, status ")
        .number(call.device_status())
        .write();
}

VOID D3d11Device::open_resource(const D3D10DDIARG_OPENRESOURCE *args, D3D10DDI_HRESOURCE handle,
                                D3D10DDI_HRTRESOURCE runtime)
{
    AV_SCOPE("D3d11Device::open_resource");
    if (context_local(handle.pDrvPrivate, runtime.handle, sizeof(ResourceRecord))) return;
    ResourceRecord &resource = record(handle);
    resource = ResourceRecord{};
    resource.stamp = d3d::RecordStamp(ResourceRecord::tag, next_id());
    resource.runtime_resource = runtime.handle;
    if (args->NumAllocations != 0u) resource.allocation = args->pOpenAllocationInfo2[0].hAllocation;
    /* The key is the identity the miniport minted over the creator's
     * allocation, which dxgkrnl hands every process that opens it - whichever
     * of this driver's devices made it, Direct3D 11's or 12's. */
    d3d::KernelSurface opened;
    const bool         surface =
        args->NumAllocations != 0u &&
        opened.open(args->pPrivateDriverData, args->PrivateDriverDataSize, args->pOpenAllocationInfo2[0].pPrivateDriverData,
                    args->pOpenAllocationInfo2[0].PrivateDriverDataSize) == Status::ok;
    const uint64_t key = surface ? opened.key() : 0u;
    uint32_t found = 0u;
    d3dproto::ResourceDesc desc = {};
    if (key == 0u && open_guest_surface(resource, *args)) return;
    /* The creator's stream is another queue on the host: a key it has just
     * shared can still be on its way there, so a miss is asked again. */
    constexpr uint32_t open_attempts = 64u;
    bool sent = key != 0u;
    for (uint32_t attempt = 0u; sent && found == 0u && attempt < open_attempts; attempt++)
        sent = d3dproto::av_d3d_OpenResource(wire(), resource.stamp.id(), key, &found, &desc);
    if (!sent || found == 0u) {
        LogLine line(lines_);
        line.text("a shared resource no Direct3D session on the host made: key ")
            .hex(key, 16u)
            .text(" allocations ")
            .hex(args->NumAllocations, 2u)
            .text(" resource data ")
            .hex(args->PrivateDriverDataSize, 4u)
            .text(" allocation data ")
            .hex(args->NumAllocations != 0u ? args->pOpenAllocationInfo2[0].PrivateDriverDataSize : 0u, 4u);
        if (args->NumAllocations != 0u && args->pOpenAllocationInfo2[0].PrivateDriverDataSize >= 8u) {
            uint64_t head = 0u;
            __builtin_memcpy(&head, args->pOpenAllocationInfo2[0].pPrivateDriverData, sizeof(head));
            line.text(" head ").hex(head, 16u);
        }
        line.write();
        set_error(E_INVALIDARG);
        return;
    }
    resource.shared_key = key;
    resource.dimension = desc.dimension;
    resource.width = desc.width;
    resource.height = desc.height == 0u ? 1u : desc.height;
    resource.depth = 1u;
    resource.layers = desc.depth_or_array == 0u ? 1u : desc.depth_or_array;
    resource.mips = desc.mip_levels == 0u ? 1u : desc.mip_levels;
    resource.format = desc.format;
    resource.samples = desc.sample_count == 0u ? 1u : desc.sample_count;
    resource.bind = desc.bind_flags;
}

VOID D3d11Device::destroy_resource(D3D10DDI_HRESOURCE handle)
{
    AV_SCOPE("D3d11Device::destroy_resource");
    ResourceRecord &resource = record(handle);
    if (parent_ != nullptr) {
        forget_copy(resource);
        return;
    }
    forget_guest_surface(resource);
    maps_.forget(resource.stamp.id());
    destroy_object(resource.stamp.id());
    if (resource.slice.bytes_ != 0u) leave_slice(resource);
    else if (resource.shadow != nullptr) shadows_.release(resource.shadow, resource.shadow_watched != 0u);
    /* Keyed or not: a presented, scanned-out or shared resource's allocation
     * was this device's to make and is its to give back. Kept, every swap
     * chain DWM remade leaked its primaries. An opened one is its maker's. */
    if (resource.allocation_owned != 0u)
        kernel_.deallocate(resource.allocation, static_cast<HANDLE>(resource.runtime_resource));
    resource = ResourceRecord{};
}

/* ---------------------------------------------------------------- views */

SIZE_T D3d11Device::calc_private_shader_resource_view_size(const D3DWDDM2_0DDIARG_CREATESHADERRESOURCEVIEW *)
{
    AV_SCOPE("D3d11Device::calc_private_shader_resource_view_size");
    return sizeof(ObjectRecord);
}

VOID D3d11Device::create_shader_resource_view(const D3DWDDM2_0DDIARG_CREATESHADERRESOURCEVIEW *args,
                                              D3D10DDI_HSHADERRESOURCEVIEW handle, D3D10DDI_HRTSHADERRESOURCEVIEW runtime)
{
    AV_SCOPE("D3d11Device::create_shader_resource_view");
    if (context_local(handle.pDrvPrivate, runtime.handle, sizeof(ObjectRecord))) return;
    const ResourceRecord &resource = record(args->hDrvResource);
    ObjectRecord &view = *static_cast<ObjectRecord *>(handle.pDrvPrivate);
    view = ObjectRecord{d3d::RecordStamp(ObjectRecord::tag, next_id()), 0u, resource.stamp.id()};
    d3dproto::ViewDesc desc = {};
    desc.resource = resource.stamp.id();
    desc.format = static_cast<uint32_t>(args->Format);
    switch (args->ResourceDimension) {
    case D3D10DDIRESOURCE_BUFFER:
        desc.dimension = view_buffer;
        desc.args[0] = args->Buffer.FirstElement;
        desc.args[1] = args->Buffer.NumElements;
        break;
    case D3D11DDIRESOURCE_BUFFEREX:
        desc.dimension = srv_buffer_ex;
        desc.args[0] = args->BufferEx.FirstElement;
        desc.args[1] = args->BufferEx.NumElements;
        desc.flags = args->BufferEx.Flags;
        break;
    case D3D10DDIRESOURCE_TEXTURE1D: {
        const bool array = resource.layers > 1u;
        desc.dimension = array ? view_1d_array : view_1d;
        desc.args[0] = args->Tex1D.MostDetailedMip;
        desc.args[1] = args->Tex1D.MipLevels;
        desc.args[2] = args->Tex1D.FirstArraySlice;
        desc.args[3] = args->Tex1D.ArraySize;
        break;
    }
    case D3D10DDIRESOURCE_TEXTURE2D: {
        const bool array = resource.layers > 1u;
        if (resource.samples > 1u) {
            desc.dimension = array ? view_2d_ms_array : view_2d_ms;
            desc.args[0] = args->Tex2D.FirstArraySlice;
            desc.args[1] = args->Tex2D.ArraySize;
        } else {
            desc.dimension = array ? view_2d_array : view_2d;
            desc.args[0] = args->Tex2D.MostDetailedMip;
            desc.args[1] = args->Tex2D.MipLevels;
            desc.args[2] = args->Tex2D.FirstArraySlice;
            desc.args[3] = args->Tex2D.ArraySize;
        }
        break;
    }
    case D3D10DDIRESOURCE_TEXTURE3D:
        desc.dimension = view_3d;
        desc.args[0] = args->Tex3D.MostDetailedMip;
        desc.args[1] = args->Tex3D.MipLevels;
        break;
    default:
        desc.dimension = resource.layers > 6u ? srv_cube_array : srv_cube;
        desc.args[0] = args->TexCube.MostDetailedMip;
        desc.args[1] = args->TexCube.MipLevels;
        desc.args[2] = args->TexCube.First2DArrayFace;
        desc.args[3] = args->TexCube.NumCubes;
        break;
    }
    if (!d3dproto::av_d3d_CreateShaderResourceView(wire(), view.stamp.id(), desc)) set_error(E_OUTOFMEMORY);
}

VOID D3d11Device::destroy_shader_resource_view(D3D10DDI_HSHADERRESOURCEVIEW handle)
{
    AV_SCOPE("D3d11Device::destroy_shader_resource_view");
    /* A deferred context's handle is a copy: the object is the immediate one's. */
    if (parent_ != nullptr) return;
    destroy_object(id_of(handle.pDrvPrivate));
}

SIZE_T D3d11Device::calc_private_render_target_view_size(const D3DWDDM2_0DDIARG_CREATERENDERTARGETVIEW *)
{
    AV_SCOPE("D3d11Device::calc_private_render_target_view_size");
    return sizeof(ObjectRecord);
}

VOID D3d11Device::create_render_target_view(const D3DWDDM2_0DDIARG_CREATERENDERTARGETVIEW *args,
                                            D3D10DDI_HRENDERTARGETVIEW handle, D3D10DDI_HRTRENDERTARGETVIEW runtime)
{
    AV_SCOPE("D3d11Device::create_render_target_view");
    if (context_local(handle.pDrvPrivate, runtime.handle, sizeof(ObjectRecord))) return;
    const ResourceRecord &resource = record(args->hDrvResource);
    ObjectRecord &view = *static_cast<ObjectRecord *>(handle.pDrvPrivate);
    view = ObjectRecord{d3d::RecordStamp(ObjectRecord::tag, next_id()), 0u, resource.stamp.id()};
    d3dproto::ViewDesc desc = {};
    desc.resource = resource.stamp.id();
    desc.format = static_cast<uint32_t>(args->Format);
    const bool array = resource.layers > 1u;
    switch (args->ResourceDimension) {
    case D3D10DDIRESOURCE_BUFFER:
        desc.dimension = view_buffer;
        desc.args[0] = args->Buffer.FirstElement;
        desc.args[1] = args->Buffer.NumElements;
        break;
    case D3D10DDIRESOURCE_TEXTURE1D:
        desc.dimension = array ? view_1d_array : view_1d;
        desc.args[0] = args->Tex1D.MipSlice;
        desc.args[1] = args->Tex1D.FirstArraySlice;
        desc.args[2] = args->Tex1D.ArraySize;
        break;
    case D3D10DDIRESOURCE_TEXTURE3D:
        desc.dimension = view_3d;
        desc.args[0] = args->Tex3D.MipSlice;
        desc.args[1] = args->Tex3D.FirstW;
        desc.args[2] = args->Tex3D.WSize;
        break;
    default: {
        /* Texture2D and TextureCube: a cube's faces are array slices of a 2D
         * texture on the host. */
        const bool faces = args->ResourceDimension == D3D10DDIRESOURCE_TEXTURECUBE;
        const UINT mip = faces ? args->TexCube.MipSlice : args->Tex2D.MipSlice;
        const UINT first = faces ? args->TexCube.FirstArraySlice : args->Tex2D.FirstArraySlice;
        const UINT size = faces ? args->TexCube.ArraySize : args->Tex2D.ArraySize;
        if (resource.samples > 1u) {
            desc.dimension = array ? view_2d_ms_array : view_2d_ms;
            desc.args[0] = first;
            desc.args[1] = size;
        } else {
            desc.dimension = array ? view_2d_array : view_2d;
            desc.args[0] = mip;
            desc.args[1] = first;
            desc.args[2] = size;
        }
        break;
    }
    }
    if (!d3dproto::av_d3d_CreateRenderTargetView(wire(), view.stamp.id(), desc)) set_error(E_OUTOFMEMORY);
}

VOID D3d11Device::destroy_render_target_view(D3D10DDI_HRENDERTARGETVIEW handle)
{
    AV_SCOPE("D3d11Device::destroy_render_target_view");
    /* A deferred context's handle is a copy: the object is the immediate one's. */
    if (parent_ != nullptr) return;
    destroy_object(id_of(handle.pDrvPrivate));
}

SIZE_T D3d11Device::calc_private_depth_stencil_view_size(const D3D11DDIARG_CREATEDEPTHSTENCILVIEW *)
{
    AV_SCOPE("D3d11Device::calc_private_depth_stencil_view_size");
    return sizeof(ObjectRecord);
}

VOID D3d11Device::create_depth_stencil_view(const D3D11DDIARG_CREATEDEPTHSTENCILVIEW *args,
                                            D3D10DDI_HDEPTHSTENCILVIEW handle, D3D10DDI_HRTDEPTHSTENCILVIEW runtime)
{
    AV_SCOPE("D3d11Device::create_depth_stencil_view");
    if (context_local(handle.pDrvPrivate, runtime.handle, sizeof(ObjectRecord))) return;
    const ResourceRecord &resource = record(args->hDrvResource);
    ObjectRecord &view = *static_cast<ObjectRecord *>(handle.pDrvPrivate);
    view = ObjectRecord{d3d::RecordStamp(ObjectRecord::tag, next_id()), 0u, resource.stamp.id()};
    d3dproto::ViewDesc desc = {};
    desc.resource = resource.stamp.id();
    desc.format = static_cast<uint32_t>(args->Format);
    desc.flags = args->Flags;
    const bool array = resource.layers > 1u;
    if (args->ResourceDimension == D3D10DDIRESOURCE_TEXTURE1D) {
        desc.dimension = array ? dsv_1d_array : dsv_1d;
        desc.args[0] = args->Tex1D.MipSlice;
        desc.args[1] = args->Tex1D.FirstArraySlice;
        desc.args[2] = args->Tex1D.ArraySize;
    } else if (resource.samples > 1u) {
        desc.dimension = array ? dsv_2d_ms_array : dsv_2d_ms;
        desc.args[0] = args->Tex2D.FirstArraySlice;
        desc.args[1] = args->Tex2D.ArraySize;
    } else {
        desc.dimension = array ? dsv_2d_array : dsv_2d;
        desc.args[0] = args->Tex2D.MipSlice;
        desc.args[1] = args->Tex2D.FirstArraySlice;
        desc.args[2] = args->Tex2D.ArraySize;
    }
    if (!d3dproto::av_d3d_CreateDepthStencilView(wire(), view.stamp.id(), desc)) set_error(E_OUTOFMEMORY);
}

VOID D3d11Device::destroy_depth_stencil_view(D3D10DDI_HDEPTHSTENCILVIEW handle)
{
    AV_SCOPE("D3d11Device::destroy_depth_stencil_view");
    /* A deferred context's handle is a copy: the object is the immediate one's. */
    if (parent_ != nullptr) return;
    destroy_object(id_of(handle.pDrvPrivate));
}

SIZE_T D3d11Device::calc_private_unordered_access_view_size(const D3DWDDM2_0DDIARG_CREATEUNORDEREDACCESSVIEW *)
{
    AV_SCOPE("D3d11Device::calc_private_unordered_access_view_size");
    return sizeof(ObjectRecord);
}

VOID D3d11Device::create_unordered_access_view(const D3DWDDM2_0DDIARG_CREATEUNORDEREDACCESSVIEW *args,
                                               D3D11DDI_HUNORDEREDACCESSVIEW handle, D3D11DDI_HRTUNORDEREDACCESSVIEW runtime)
{
    AV_SCOPE("D3d11Device::create_unordered_access_view");
    if (context_local(handle.pDrvPrivate, runtime.handle, sizeof(ObjectRecord))) return;
    const ResourceRecord &resource = record(args->hDrvResource);
    ObjectRecord &view = *static_cast<ObjectRecord *>(handle.pDrvPrivate);
    view = ObjectRecord{d3d::RecordStamp(ObjectRecord::tag, next_id()), 0u, resource.stamp.id()};
    d3dproto::ViewDesc desc = {};
    desc.resource = resource.stamp.id();
    desc.format = static_cast<uint32_t>(args->Format);
    const bool array = resource.layers > 1u;
    switch (args->ResourceDimension) {
    case D3D10DDIRESOURCE_BUFFER:
        desc.dimension = view_buffer;
        desc.args[0] = args->Buffer.FirstElement;
        desc.args[1] = args->Buffer.NumElements;
        desc.flags = args->Buffer.Flags;
        break;
    case D3D10DDIRESOURCE_TEXTURE1D:
        desc.dimension = array ? view_1d_array : view_1d;
        desc.args[0] = args->Tex1D.MipSlice;
        desc.args[1] = args->Tex1D.FirstArraySlice;
        desc.args[2] = args->Tex1D.ArraySize;
        break;
    case D3D10DDIRESOURCE_TEXTURE3D:
        desc.dimension = view_3d;
        desc.args[0] = args->Tex3D.MipSlice;
        desc.args[1] = args->Tex3D.FirstW;
        desc.args[2] = args->Tex3D.WSize;
        break;
    default:
        desc.dimension = array ? view_2d_array : view_2d;
        desc.args[0] = args->Tex2D.MipSlice;
        desc.args[1] = args->Tex2D.FirstArraySlice;
        desc.args[2] = args->Tex2D.ArraySize;
        break;
    }
    if (!d3dproto::av_d3d_CreateUnorderedAccessView(wire(), view.stamp.id(), desc)) set_error(E_OUTOFMEMORY);
}

VOID D3d11Device::destroy_unordered_access_view(D3D11DDI_HUNORDEREDACCESSVIEW handle)
{
    AV_SCOPE("D3d11Device::destroy_unordered_access_view");
    /* A deferred context's handle is a copy: the object is the immediate one's. */
    if (parent_ != nullptr) return;
    destroy_object(id_of(handle.pDrvPrivate));
}

/* ---------------------------------------------------------------- shaders */

void D3d11Device::create_shader(uint32_t stage, const UINT *code, D3D10DDI_HSHADER shader,
                                const D3D11_1DDIARG_SIGNATURE_ENTRY2 *inputs, UINT input_count,
                                const D3D11_1DDIARG_SIGNATURE_ENTRY2 *outputs, UINT output_count,
                                const D3D11_1DDIARG_SIGNATURE_ENTRY2 *patch, UINT patch_count,
                                const D3D11DDIARG_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT *stream_out)
{
    AV_SCOPE("D3d11Device::create_shader");
    ObjectRecord &record_of = *static_cast<ObjectRecord *>(shader.pDrvPrivate);
    record_of = ObjectRecord{d3d::RecordStamp(ObjectRecord::tag, next_id()), stage, 0u};
    constexpr uint32_t max_entries = 128u;
    d3dproto::SignatureEntry signatures[3][max_entries] = {};
    const D3D11_1DDIARG_SIGNATURE_ENTRY2 *const sources[3] = {inputs, outputs, patch};
    const UINT counts[3] = {input_count > max_entries ? max_entries : input_count,
                            output_count > max_entries ? max_entries : output_count,
                            patch_count > max_entries ? max_entries : patch_count};
    for (uint32_t which = 0u; which < 3u; which++)
        for (UINT index = 0u; index < counts[which]; index++) {
            const D3D11_1DDIARG_SIGNATURE_ENTRY2 &entry = sources[which][index];
            signatures[which][index] = d3dproto::SignatureEntry{static_cast<uint32_t>(entry.SystemValue), entry.Register,
                                                                entry.Mask, static_cast<uint32_t>(entry.RegisterComponentType),
                                                                static_cast<uint32_t>(entry.MinPrecision), entry.Stream};
        }
    d3dproto::StreamOutEntry declaration[max_entries] = {};
    uint32_t declared = 0u, strides_count = 0u, rasterized = no_stream;
    const uint32_t *strides = nullptr;
    if (stream_out != nullptr) {
        declared = stream_out->NumEntries > max_entries ? max_entries : stream_out->NumEntries;
        for (uint32_t index = 0u; index < declared; index++) {
            const D3D11DDIARG_STREAM_OUTPUT_DECLARATION_ENTRY &entry = stream_out->pOutputStreamDecl[index];
            declaration[index] = d3dproto::StreamOutEntry{entry.Stream, entry.OutputSlot, entry.RegisterIndex,
                                                          entry.RegisterMask};
        }
        strides_count = stream_out->NumStrides;
        strides = stream_out->BufferStridesInBytes;
        rasterized = stream_out->RasterizedStream;
    }
    /* The second token is the stream's length in tokens. */
    const size_t bytes = static_cast<size_t>(code[1]) * sizeof(UINT);
    if (!wire().make_room_for(bytes))
        log_.write("avumd: a shader larger than the arena");
    if (!d3dproto::av_d3d_CreateShader(wire(), record_of.stamp.id(), stage, counts[0], signatures[0], counts[1],
                                        signatures[1], counts[2], signatures[2], declared, declaration, strides_count,
                                        strides, rasterized, code, bytes))
        set_error(E_OUTOFMEMORY);
}

SIZE_T D3d11Device::calc_private_shader_size(const UINT *, const D3D11_1DDIARG_STAGE_IO_SIGNATURES *)
{
    AV_SCOPE("D3d11Device::calc_private_shader_size");
    return sizeof(ObjectRecord);
}

SIZE_T D3d11Device::calc_private_tessellation_shader_size(const UINT *, const D3D11_1DDIARG_TESSELLATION_IO_SIGNATURES *)
{
    AV_SCOPE("D3d11Device::calc_private_tessellation_shader_size");
    return sizeof(ObjectRecord);
}

SIZE_T D3d11Device::calc_private_geometry_shader_with_stream_output(
    const D3D11DDIARG_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT *, const D3D11_1DDIARG_STAGE_IO_SIGNATURES *)
{
    AV_SCOPE("D3d11Device::calc_private_geometry_shader_with_stream_output");
    return sizeof(ObjectRecord);
}

VOID D3d11Device::create_vertex_shader(const UINT *code, D3D10DDI_HSHADER shader, D3D10DDI_HRTSHADER runtime,
                                       const D3D11_1DDIARG_STAGE_IO_SIGNATURES *signatures)
{
    AV_SCOPE("D3d11Device::create_vertex_shader");
    if (context_local(shader.pDrvPrivate, runtime.handle, sizeof(ObjectRecord))) return;
    create_shader(stage_vertex, code, shader, signatures->pInputSignature, signatures->NumInputSignatureEntries,
                  signatures->pOutputSignature, signatures->NumOutputSignatureEntries, nullptr, 0u, nullptr);
}

VOID D3d11Device::create_geometry_shader(const UINT *code, D3D10DDI_HSHADER shader, D3D10DDI_HRTSHADER runtime,
                                         const D3D11_1DDIARG_STAGE_IO_SIGNATURES *signatures)
{
    AV_SCOPE("D3d11Device::create_geometry_shader");
    if (context_local(shader.pDrvPrivate, runtime.handle, sizeof(ObjectRecord))) return;
    create_shader(stage_geometry, code, shader, signatures->pInputSignature, signatures->NumInputSignatureEntries,
                  signatures->pOutputSignature, signatures->NumOutputSignatureEntries, nullptr, 0u, nullptr);
}

VOID D3d11Device::create_pixel_shader(const UINT *code, D3D10DDI_HSHADER shader, D3D10DDI_HRTSHADER runtime,
                                      const D3D11_1DDIARG_STAGE_IO_SIGNATURES *signatures)
{
    AV_SCOPE("D3d11Device::create_pixel_shader");
    if (context_local(shader.pDrvPrivate, runtime.handle, sizeof(ObjectRecord))) return;
    create_shader(stage_pixel, code, shader, signatures->pInputSignature, signatures->NumInputSignatureEntries,
                  signatures->pOutputSignature, signatures->NumOutputSignatureEntries, nullptr, 0u, nullptr);
}

VOID D3d11Device::create_geometry_shader_with_stream_output(const D3D11DDIARG_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT *args,
                                                            D3D10DDI_HSHADER shader, D3D10DDI_HRTSHADER runtime,
                                                            const D3D11_1DDIARG_STAGE_IO_SIGNATURES *signatures)
{
    AV_SCOPE("D3d11Device::create_geometry_shader_with_stream_output");
    if (context_local(shader.pDrvPrivate, runtime.handle, sizeof(ObjectRecord))) return;
    if (args->pShaderCode == nullptr) {
        stream_vertex_output(*args, shader, *signatures);
        return;
    }
    create_shader(stage_geometry, args->pShaderCode, shader, signatures->pInputSignature,
                  signatures->NumInputSignatureEntries, signatures->pOutputSignature,
                  signatures->NumOutputSignatureEntries, nullptr, 0u, args);
}

/* A declaration with no shader streams the vertex shader's output: the
 * runtime hands no code then, only what the stage reads, so the shader it
 * stands for is written here (PassthroughGeometryShader) and writes out what
 * it read. */
void D3d11Device::stream_vertex_output(const D3D11DDIARG_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT &args,
                                       D3D10DDI_HSHADER shader, const D3D11_1DDIARG_STAGE_IO_SIGNATURES &signatures)
{
    AV_SCOPE("D3d11Device::stream_vertex_output");
    const bool reads = signatures.NumInputSignatureEntries != 0u;
    const D3D11_1DDIARG_SIGNATURE_ENTRY2 *const passed = reads ? signatures.pInputSignature : signatures.pOutputSignature;
    const UINT count = reads ? signatures.NumInputSignatureEntries : signatures.NumOutputSignatureEntries;
    d3d::PassthroughGeometryShader written;
    bool                           taken = passed != nullptr && count != 0u;
    for (UINT index = 0u; taken && index < count; index++)
        taken = written.add(passed[index].Register, passed[index].Mask, static_cast<uint32_t>(passed[index].SystemValue));
    const uint32_t *const tokens = taken ? written.tokens() : nullptr;
    if (tokens == nullptr) {
        log_.write("avumd: a stream output with no shader names no register it can pass through");
        set_error(E_INVALIDARG);
        return;
    }
    create_shader(stage_geometry, tokens, shader, passed, count, passed, count, nullptr, 0u, &args);
}

VOID D3d11Device::create_hull_shader(const UINT *code, D3D10DDI_HSHADER shader, D3D10DDI_HRTSHADER runtime,
                                     const D3D11_1DDIARG_TESSELLATION_IO_SIGNATURES *signatures)
{
    AV_SCOPE("D3d11Device::create_hull_shader");
    if (context_local(shader.pDrvPrivate, runtime.handle, sizeof(ObjectRecord))) return;
    create_shader(stage_hull, code, shader, signatures->pInputSignature, signatures->NumInputSignatureEntries,
                  signatures->pOutputSignature, signatures->NumOutputSignatureEntries,
                  signatures->pPatchConstantSignature, signatures->NumPatchConstantSignatureEntries, nullptr);
}

VOID D3d11Device::create_domain_shader(const UINT *code, D3D10DDI_HSHADER shader, D3D10DDI_HRTSHADER runtime,
                                       const D3D11_1DDIARG_TESSELLATION_IO_SIGNATURES *signatures)
{
    AV_SCOPE("D3d11Device::create_domain_shader");
    if (context_local(shader.pDrvPrivate, runtime.handle, sizeof(ObjectRecord))) return;
    create_shader(stage_domain, code, shader, signatures->pInputSignature, signatures->NumInputSignatureEntries,
                  signatures->pOutputSignature, signatures->NumOutputSignatureEntries,
                  signatures->pPatchConstantSignature, signatures->NumPatchConstantSignatureEntries, nullptr);
}

VOID D3d11Device::create_compute_shader(const UINT *code, D3D10DDI_HSHADER shader, D3D10DDI_HRTSHADER runtime)
{
    AV_SCOPE("D3d11Device::create_compute_shader");
    if (context_local(shader.pDrvPrivate, runtime.handle, sizeof(ObjectRecord))) return;
    create_shader(stage_compute, code, shader, nullptr, 0u, nullptr, 0u, nullptr, 0u, nullptr);
}

VOID D3d11Device::destroy_shader(D3D10DDI_HSHADER handle)
{
    AV_SCOPE("D3d11Device::destroy_shader");
    /* A deferred context's handle is a copy: the object is the immediate one's. */
    if (parent_ != nullptr) return;
    destroy_object(id_of(handle.pDrvPrivate));
}

/* ---------------------------------------------------------------- layouts and states */

SIZE_T D3d11Device::calc_private_element_layout_size(const D3D10DDIARG_CREATEELEMENTLAYOUT *)
{
    AV_SCOPE("D3d11Device::calc_private_element_layout_size");
    return sizeof(ObjectRecord);
}

VOID D3d11Device::create_element_layout(const D3D10DDIARG_CREATEELEMENTLAYOUT *args, D3D10DDI_HELEMENTLAYOUT handle,
                                        D3D10DDI_HRTELEMENTLAYOUT runtime)
{
    AV_SCOPE("D3d11Device::create_element_layout");
    if (context_local(handle.pDrvPrivate, runtime.handle, sizeof(ObjectRecord))) return;
    ObjectRecord &layout = *static_cast<ObjectRecord *>(handle.pDrvPrivate);
    layout = ObjectRecord{d3d::RecordStamp(ObjectRecord::tag, next_id()), 0u, 0u};
    constexpr uint32_t max_elements = 32u;
    d3dproto::InputElement elements[max_elements] = {};
    const uint32_t count = args->NumElements > max_elements ? max_elements : args->NumElements;
    for (uint32_t index = 0u; index < count; index++) {
        const D3D10DDIARG_INPUT_ELEMENT_DESC &element = args->pVertexElements[index];
        elements[index] = d3dproto::InputElement{element.InputSlot, element.AlignedByteOffset,
                                                 static_cast<uint32_t>(element.Format),
                                                 static_cast<uint32_t>(element.InputSlotClass),
                                                 element.InstanceDataStepRate, element.InputRegister};
    }
    if (!d3dproto::av_d3d_CreateInputLayout(wire(), layout.stamp.id(), count, elements)) set_error(E_OUTOFMEMORY);
}

VOID D3d11Device::destroy_element_layout(D3D10DDI_HELEMENTLAYOUT handle)
{
    AV_SCOPE("D3d11Device::destroy_element_layout");
    /* A deferred context's handle is a copy: the object is the immediate one's. */
    if (parent_ != nullptr) return;
    destroy_object(id_of(handle.pDrvPrivate));
}

SIZE_T D3d11Device::calc_private_blend_state_size(const D3D11_1_DDI_BLEND_DESC *)
{
    AV_SCOPE("D3d11Device::calc_private_blend_state_size");
    return sizeof(ObjectRecord);
}

VOID D3d11Device::create_blend_state(const D3D11_1_DDI_BLEND_DESC *args, D3D10DDI_HBLENDSTATE handle,
                                     D3D10DDI_HRTBLENDSTATE runtime)
{
    AV_SCOPE("D3d11Device::create_blend_state");
    if (context_local(handle.pDrvPrivate, runtime.handle, sizeof(ObjectRecord))) return;
    ObjectRecord &state = *static_cast<ObjectRecord *>(handle.pDrvPrivate);
    state = ObjectRecord{d3d::RecordStamp(ObjectRecord::tag, next_id()), 0u, 0u};
    d3dproto::BlendDesc desc = {};
    desc.alpha_to_coverage = args->AlphaToCoverageEnable != FALSE;
    desc.independent_blend = args->IndependentBlendEnable != FALSE;
    for (uint32_t target = 0u; target < 8u; target++) {
        const D3D11_1_DDI_RENDER_TARGET_BLEND_DESC &in = args->RenderTarget[target];
        desc.targets[target] = d3dproto::RenderTargetBlend{
            static_cast<uint32_t>(in.BlendEnable != FALSE), static_cast<uint32_t>(in.LogicOpEnable != FALSE),
            static_cast<uint32_t>(in.SrcBlend),            static_cast<uint32_t>(in.DestBlend),
            static_cast<uint32_t>(in.BlendOp),             static_cast<uint32_t>(in.SrcBlendAlpha),
            static_cast<uint32_t>(in.DestBlendAlpha),      static_cast<uint32_t>(in.BlendOpAlpha),
            static_cast<uint32_t>(in.LogicOp),             in.RenderTargetWriteMask};
    }
    if (!d3dproto::av_d3d_CreateBlendState(wire(), state.stamp.id(), desc)) set_error(E_OUTOFMEMORY);
}

VOID D3d11Device::destroy_blend_state(D3D10DDI_HBLENDSTATE handle)
{
    AV_SCOPE("D3d11Device::destroy_blend_state");
    /* A deferred context's handle is a copy: the object is the immediate one's. */
    if (parent_ != nullptr) return;
    destroy_object(id_of(handle.pDrvPrivate));
}

SIZE_T D3d11Device::calc_private_depth_stencil_state_size(const D3D10_DDI_DEPTH_STENCIL_DESC *)
{
    AV_SCOPE("D3d11Device::calc_private_depth_stencil_state_size");
    return sizeof(ObjectRecord);
}

VOID D3d11Device::create_depth_stencil_state(const D3D10_DDI_DEPTH_STENCIL_DESC *args,
                                             D3D10DDI_HDEPTHSTENCILSTATE handle, D3D10DDI_HRTDEPTHSTENCILSTATE runtime)
{
    AV_SCOPE("D3d11Device::create_depth_stencil_state");
    if (context_local(handle.pDrvPrivate, runtime.handle, sizeof(ObjectRecord))) return;
    ObjectRecord &state = *static_cast<ObjectRecord *>(handle.pDrvPrivate);
    state = ObjectRecord{d3d::RecordStamp(ObjectRecord::tag, next_id()), 0u, 0u};
    d3dproto::DepthStencilDesc desc = {};
    desc.depth_enable = args->DepthEnable != FALSE;
    desc.depth_write_mask = static_cast<uint32_t>(args->DepthWriteMask);
    desc.depth_func = static_cast<uint32_t>(args->DepthFunc);
    desc.stencil_enable = args->StencilEnable != FALSE;
    desc.stencil_read_mask = args->StencilReadMask;
    desc.stencil_write_mask = args->StencilWriteMask;
    desc.front = d3dproto::StencilOp{static_cast<uint32_t>(args->FrontFace.StencilFailOp),
                                     static_cast<uint32_t>(args->FrontFace.StencilDepthFailOp),
                                     static_cast<uint32_t>(args->FrontFace.StencilPassOp),
                                     static_cast<uint32_t>(args->FrontFace.StencilFunc)};
    desc.back = d3dproto::StencilOp{static_cast<uint32_t>(args->BackFace.StencilFailOp),
                                    static_cast<uint32_t>(args->BackFace.StencilDepthFailOp),
                                    static_cast<uint32_t>(args->BackFace.StencilPassOp),
                                    static_cast<uint32_t>(args->BackFace.StencilFunc)};
    if (!d3dproto::av_d3d_CreateDepthStencilState(wire(), state.stamp.id(), desc)) set_error(E_OUTOFMEMORY);
}

VOID D3d11Device::destroy_depth_stencil_state(D3D10DDI_HDEPTHSTENCILSTATE handle)
{
    AV_SCOPE("D3d11Device::destroy_depth_stencil_state");
    /* A deferred context's handle is a copy: the object is the immediate one's. */
    if (parent_ != nullptr) return;
    destroy_object(id_of(handle.pDrvPrivate));
}

SIZE_T D3d11Device::calc_private_rasterizer_state_size(const D3DWDDM2_0DDI_RASTERIZER_DESC *)
{
    AV_SCOPE("D3d11Device::calc_private_rasterizer_state_size");
    return sizeof(ObjectRecord);
}

VOID D3d11Device::create_rasterizer_state(const D3DWDDM2_0DDI_RASTERIZER_DESC *args, D3D10DDI_HRASTERIZERSTATE handle,
                                          D3D10DDI_HRTRASTERIZERSTATE runtime)
{
    AV_SCOPE("D3d11Device::create_rasterizer_state");
    if (context_local(handle.pDrvPrivate, runtime.handle, sizeof(ObjectRecord))) return;
    ObjectRecord &state = *static_cast<ObjectRecord *>(handle.pDrvPrivate);
    state = ObjectRecord{d3d::RecordStamp(ObjectRecord::tag, next_id()), 0u, 0u};
    const d3dproto::RasterizerDesc desc = {static_cast<uint32_t>(args->FillMode),
                                           static_cast<uint32_t>(args->CullMode),
                                           static_cast<uint32_t>(args->FrontCounterClockwise != FALSE),
                                           args->DepthBias,
                                           args->DepthBiasClamp,
                                           args->SlopeScaledDepthBias,
                                           static_cast<uint32_t>(args->DepthClipEnable != FALSE),
                                           static_cast<uint32_t>(args->ScissorEnable != FALSE),
                                           static_cast<uint32_t>(args->MultisampleEnable != FALSE),
                                           static_cast<uint32_t>(args->AntialiasedLineEnable != FALSE),
                                           args->ForcedSampleCount,
                                           static_cast<uint32_t>(args->ConservativeRasterizationMode)};
    if (!d3dproto::av_d3d_CreateRasterizerState(wire(), state.stamp.id(), desc)) set_error(E_OUTOFMEMORY);
}

VOID D3d11Device::destroy_rasterizer_state(D3D10DDI_HRASTERIZERSTATE handle)
{
    AV_SCOPE("D3d11Device::destroy_rasterizer_state");
    /* A deferred context's handle is a copy: the object is the immediate one's. */
    if (parent_ != nullptr) return;
    destroy_object(id_of(handle.pDrvPrivate));
}

SIZE_T D3d11Device::calc_private_sampler_size(const D3D10_DDI_SAMPLER_DESC *)
{
    AV_SCOPE("D3d11Device::calc_private_sampler_size");
    return sizeof(ObjectRecord);
}

VOID D3d11Device::create_sampler(const D3D10_DDI_SAMPLER_DESC *args, D3D10DDI_HSAMPLER handle, D3D10DDI_HRTSAMPLER runtime)
{
    AV_SCOPE("D3d11Device::create_sampler");
    if (context_local(handle.pDrvPrivate, runtime.handle, sizeof(ObjectRecord))) return;
    ObjectRecord &sampler = *static_cast<ObjectRecord *>(handle.pDrvPrivate);
    sampler = ObjectRecord{d3d::RecordStamp(ObjectRecord::tag, next_id()), 0u, 0u};
    d3dproto::SamplerDesc desc = {};
    desc.filter = static_cast<uint32_t>(args->Filter);
    desc.address_u = static_cast<uint32_t>(args->AddressU);
    desc.address_v = static_cast<uint32_t>(args->AddressV);
    desc.address_w = static_cast<uint32_t>(args->AddressW);
    desc.mip_lod_bias = args->MipLODBias;
    desc.max_anisotropy = args->MaxAnisotropy;
    desc.comparison = static_cast<uint32_t>(args->ComparisonFunc);
    for (uint32_t index = 0u; index < 4u; index++) desc.border_color[index] = args->BorderColor[index];
    desc.min_lod = args->MinLOD;
    desc.max_lod = args->MaxLOD;
    if (!d3dproto::av_d3d_CreateSampler(wire(), sampler.stamp.id(), desc)) set_error(E_OUTOFMEMORY);
}

VOID D3d11Device::destroy_sampler(D3D10DDI_HSAMPLER handle)
{
    AV_SCOPE("D3d11Device::destroy_sampler");
    /* A deferred context's handle is a copy: the object is the immediate one's. */
    if (parent_ != nullptr) return;
    destroy_object(id_of(handle.pDrvPrivate));
}

SIZE_T D3d11Device::calc_private_query_size(const D3DWDDM2_0DDIARG_CREATEQUERY *)
{
    AV_SCOPE("D3d11Device::calc_private_query_size");
    return sizeof(QueryRecord);
}

VOID D3d11Device::create_query(const D3DWDDM2_0DDIARG_CREATEQUERY *args, D3D10DDI_HQUERY handle, D3D10DDI_HRTQUERY runtime)
{
    AV_SCOPE("D3d11Device::create_query");
    if (context_local(handle.pDrvPrivate, runtime.handle, sizeof(QueryRecord))) return;
    QueryRecord &query = *static_cast<QueryRecord *>(handle.pDrvPrivate);
    /* The answer's size by D3D10DDI_QUERY: an event's BOOL, an occlusion
     * count, a timestamp, the disjoint pair, the pipeline statistics, a
     * predicate's BOOL, the stream-output statistics. */
    constexpr uint32_t sizes[] = {4u, 8u, 8u, 16u, 88u, 4u, 16u, 4u, 16u, 16u, 16u, 16u, 4u, 4u, 4u, 4u};
    const uint32_t type = static_cast<uint32_t>(args->Query);
    query = QueryRecord{d3d::RecordStamp(QueryRecord::tag, next_id()), type, type < sizeof(sizes) / sizeof(sizes[0]) ? sizes[type] : 0u,
                        0u, 0u, 0u, 0u};
    if (query.data_bytes == 0u) {
        set_error(E_INVALIDARG);
        return;
    }
    if (!d3dproto::av_d3d_CreateQuery(wire(), query.stamp.id(), type, args->MiscFlags)) set_error(E_OUTOFMEMORY);
}

VOID D3d11Device::destroy_query(D3D10DDI_HQUERY handle)
{
    AV_SCOPE("D3d11Device::destroy_query");
    /* A deferred context's handle is a copy: the object is the immediate one's. */
    if (parent_ != nullptr) return;
    destroy_object(id_of(handle.pDrvPrivate));
}

}  // namespace av::d3d11
