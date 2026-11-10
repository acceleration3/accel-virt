#include "um/opencl/windows_d3d10_path.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/opencl/d3d11_resource.hpp"
#include "icd/opencl/d3d11_rows.hpp"

namespace av {

using clicd::D3d11Resource;

WindowsD3d10Path::WindowsD3d10Path()
    : device_iid_(__uuidof(ID3D10Device)), resource_iid_(__uuidof(ID3D10Resource)),
      multithread_iid_(__uuidof(ID3D10Multithread))
{
    AV_SCOPE("WindowsD3d10Path::WindowsD3d10Path");
}

bool WindowsD3d10Path::device(void *object)
{
    AV_SCOPE("WindowsD3d10Path::device");
    ID3D10Device *const asked = as_device(object);
    if (asked == nullptr) return false;
    asked->Release();
    return true;
}

bool WindowsD3d10Path::describe(void *object, void *device, D3d11Resource &described)
{
    AV_SCOPE("WindowsD3d10Path::describe");
    described = D3d11Resource();
    ID3D10Resource *const resource = as_resource(object);
    if (resource == nullptr) return false;
    ID3D10Device *const owner = as_device(device);
    const bool          ours = owner != nullptr && made_on(*resource, *owner);
    if (ours) described = description(*resource);
    if (owner != nullptr) owner->Release();
    resource->Release();
    return ours;
}

cl_int WindowsD3d10Path::read(void *device, void *resource, uint32_t subresource, clicd::D3d11Rows &rows)
{
    AV_SCOPE("WindowsD3d10Path::read");
    ID3D10Device *const   owner = as_device(device);
    ID3D10Resource *const source = as_resource(resource);
    ID3D10Resource *const staging =
        owner != nullptr && source != nullptr ? staging_for(*owner, *source, subresource, D3D10_CPU_ACCESS_READ)
                                              : nullptr;
    cl_int result = CL_OUT_OF_RESOURCES;
    if (staging != nullptr) {
        ID3D10Multithread *const section = section_of(*owner);
        enter(section);
        owner->CopySubresourceRegion(staging, 0u, 0u, 0u, 0u, source, subresource, nullptr);
        result = visit_mapped(*staging, section, D3D10_MAP_READ, rows);
        leave(section);
        if (section != nullptr) section->Release();
        staging->Release();
    }
    if (source != nullptr) source->Release();
    if (owner != nullptr) owner->Release();
    return result;
}

cl_int WindowsD3d10Path::write(void *device, void *resource, uint32_t subresource, clicd::D3d11Rows &rows)
{
    AV_SCOPE("WindowsD3d10Path::write");
    ID3D10Device *const   owner = as_device(device);
    ID3D10Resource *const target = as_resource(resource);
    ID3D10Resource *const staging =
        owner != nullptr && target != nullptr ? staging_for(*owner, *target, subresource, D3D10_CPU_ACCESS_WRITE)
                                              : nullptr;
    cl_int result = CL_OUT_OF_RESOURCES;
    if (staging != nullptr) {
        ID3D10Multithread *const section = section_of(*owner);
        enter(section);
        result = visit_mapped(*staging, section, D3D10_MAP_WRITE, rows);
        if (result == CL_SUCCESS) owner->CopySubresourceRegion(target, subresource, 0u, 0u, 0u, staging, 0u, nullptr);
        leave(section);
        if (section != nullptr) section->Release();
        staging->Release();
    }
    if (target != nullptr) target->Release();
    if (owner != nullptr) owner->Release();
    return result;
}

cl_int WindowsD3d10Path::visit_mapped(ID3D10Resource &staging, ID3D10Multithread *section, D3D10_MAP type,
                                      clicd::D3d11Rows &rows)
{
    AV_SCOPE("WindowsD3d10Path::visit_mapped");
    D3D10_RESOURCE_DIMENSION dimension = D3D10_RESOURCE_DIMENSION_UNKNOWN;
    staging.GetType(&dimension);
    uint8_t *data = nullptr;
    UINT     row_pitch = 0u;
    UINT     depth_pitch = 0u;
    bool     mapped = false;
    if (dimension == D3D10_RESOURCE_DIMENSION_BUFFER) {
        void *bytes = nullptr;
        mapped = SUCCEEDED(static_cast<ID3D10Buffer &>(staging).Map(type, 0u, &bytes));
        data = static_cast<uint8_t *>(bytes);
    } else if (dimension == D3D10_RESOURCE_DIMENSION_TEXTURE3D) {
        D3D10_MAPPED_TEXTURE3D volume = {};
        mapped = SUCCEEDED(static_cast<ID3D10Texture3D &>(staging).Map(0u, type, 0u, &volume));
        data = static_cast<uint8_t *>(volume.pData);
        row_pitch = volume.RowPitch;
        depth_pitch = volume.DepthPitch;
    } else {
        D3D10_MAPPED_TEXTURE2D plane = {};
        mapped = SUCCEEDED(static_cast<ID3D10Texture2D &>(staging).Map(0u, type, 0u, &plane));
        data = static_cast<uint8_t *>(plane.pData);
        row_pitch = plane.RowPitch;
    }
    if (!mapped || data == nullptr) return CL_OUT_OF_RESOURCES;
    leave(section);
    const cl_int visited = rows.visit(data, row_pitch, depth_pitch);
    enter(section);
    if (dimension == D3D10_RESOURCE_DIMENSION_BUFFER) static_cast<ID3D10Buffer &>(staging).Unmap();
    else if (dimension == D3D10_RESOURCE_DIMENSION_TEXTURE3D) static_cast<ID3D10Texture3D &>(staging).Unmap(0u);
    else static_cast<ID3D10Texture2D &>(staging).Unmap(0u);
    return visited;
}

ID3D10Device *WindowsD3d10Path::as_device(void *object)
{
    AV_SCOPE("WindowsD3d10Path::as_device");
    ID3D10Device *asked = nullptr;
    if (object == nullptr ||
        FAILED(static_cast<IUnknown *>(object)->QueryInterface(device_iid_, reinterpret_cast<void **>(&asked))))
        return nullptr;
    return asked;
}

ID3D10Resource *WindowsD3d10Path::as_resource(void *object)
{
    AV_SCOPE("WindowsD3d10Path::as_resource");
    ID3D10Resource *asked = nullptr;
    if (object == nullptr ||
        FAILED(static_cast<IUnknown *>(object)->QueryInterface(resource_iid_, reinterpret_cast<void **>(&asked))))
        return nullptr;
    return asked;
}

bool WindowsD3d10Path::made_on(ID3D10Resource &resource, ID3D10Device &device)
{
    AV_SCOPE("WindowsD3d10Path::made_on");
    ID3D10Device *owner = nullptr;
    resource.GetDevice(&owner);
    const bool same = owner == &device;
    if (owner != nullptr) owner->Release();
    return same;
}

D3d11Resource WindowsD3d10Path::description(ID3D10Resource &resource)
{
    AV_SCOPE("WindowsD3d10Path::description");
    D3D10_RESOURCE_DIMENSION dimension = D3D10_RESOURCE_DIMENSION_UNKNOWN;
    resource.GetType(&dimension);
    if (dimension == D3D10_RESOURCE_DIMENSION_BUFFER) {
        D3D10_BUFFER_DESC desc = {};
        static_cast<ID3D10Buffer &>(resource).GetDesc(&desc);
        return D3d11Resource(D3d11Resource::Kind::buffer, 0u, desc.ByteWidth, 0u, 0u, 0u, 1u, 1u, 1u);
    }
    if (dimension == D3D10_RESOURCE_DIMENSION_TEXTURE2D) {
        D3D10_TEXTURE2D_DESC desc = {};
        static_cast<ID3D10Texture2D &>(resource).GetDesc(&desc);
        return D3d11Resource(D3d11Resource::Kind::texture2d, static_cast<uint32_t>(desc.Format), 0u, desc.Width, desc.Height, 1u,
                             desc.MipLevels, desc.ArraySize, desc.SampleDesc.Count);
    }
    if (dimension == D3D10_RESOURCE_DIMENSION_TEXTURE3D) {
        D3D10_TEXTURE3D_DESC desc = {};
        static_cast<ID3D10Texture3D &>(resource).GetDesc(&desc);
        return D3d11Resource(D3d11Resource::Kind::texture3d, static_cast<uint32_t>(desc.Format), 0u, desc.Width, desc.Height, desc.Depth,
                             desc.MipLevels, 1u, 1u);
    }
    return D3d11Resource();
}

ID3D10Resource *WindowsD3d10Path::staging_for(ID3D10Device &device, ID3D10Resource &resource, uint32_t subresource,
                                              UINT cpu_access)
{
    AV_SCOPE("WindowsD3d10Path::staging_for");
    const D3d11Resource described = description(resource);
    if (described.kind() == D3d11Resource::Kind::buffer) {
        D3D10_BUFFER_DESC staged = {};
        staged.ByteWidth = static_cast<UINT>(described.bytes());
        staged.Usage = D3D10_USAGE_STAGING;
        staged.CPUAccessFlags = cpu_access;
        ID3D10Buffer *made = nullptr;
        return SUCCEEDED(device.CreateBuffer(&staged, nullptr, &made)) ? made : nullptr;
    }
    if (described.kind() == D3d11Resource::Kind::texture3d) {
        D3D10_TEXTURE3D_DESC staged = {};
        static_cast<ID3D10Texture3D &>(resource).GetDesc(&staged);
        staged.Width = described.width_of(subresource);
        staged.Height = described.height_of(subresource);
        staged.Depth = described.depth_of(subresource);
        staged.MipLevels = 1u;
        staged.Usage = D3D10_USAGE_STAGING;
        staged.BindFlags = 0u;
        staged.CPUAccessFlags = cpu_access;
        staged.MiscFlags = 0u;
        ID3D10Texture3D *made = nullptr;
        return SUCCEEDED(device.CreateTexture3D(&staged, nullptr, &made)) ? made : nullptr;
    }
    if (described.kind() != D3d11Resource::Kind::texture2d) return nullptr;
    D3D10_TEXTURE2D_DESC staged = {};
    static_cast<ID3D10Texture2D &>(resource).GetDesc(&staged);
    staged.Width = described.width_of(subresource);
    staged.Height = described.height_of(subresource);
    staged.MipLevels = 1u;
    staged.ArraySize = 1u;
    staged.SampleDesc.Count = 1u;
    staged.SampleDesc.Quality = 0u;
    staged.Usage = D3D10_USAGE_STAGING;
    staged.BindFlags = 0u;
    staged.CPUAccessFlags = cpu_access;
    staged.MiscFlags = 0u;
    ID3D10Texture2D *made = nullptr;
    return SUCCEEDED(device.CreateTexture2D(&staged, nullptr, &made)) ? made : nullptr;
}

ID3D10Multithread *WindowsD3d10Path::section_of(ID3D10Device &device)
{
    AV_SCOPE("WindowsD3d10Path::section_of");
    ID3D10Multithread *section = nullptr;
    if (FAILED(device.QueryInterface(multithread_iid_, reinterpret_cast<void **>(&section)))) return nullptr;
    return section;
}

void WindowsD3d10Path::enter(ID3D10Multithread *section)
{
    AV_SCOPE("WindowsD3d10Path::enter");
    if (section != nullptr) section->Enter();
}

void WindowsD3d10Path::leave(ID3D10Multithread *section)
{
    AV_SCOPE("WindowsD3d10Path::leave");
    if (section != nullptr) section->Leave();
}

}
