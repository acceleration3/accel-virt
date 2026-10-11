#include "um/opencl/windows_d3d11_bridge.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/opencl/d3d11_resource.hpp"
#include "icd/opencl/d3d11_rows.hpp"

namespace av {

using clicd::D3d11Resource;

WindowsD3d11Bridge::WindowsD3d11Bridge()
    : device_iid_(__uuidof(ID3D11Device)), resource_iid_(__uuidof(ID3D11Resource)),
      multithread_iid_(__uuidof(ID3D10Multithread))
{
    AV_SCOPE("WindowsD3d11Bridge::WindowsD3d11Bridge");
}

bool WindowsD3d11Bridge::device(void *object)
{
    AV_SCOPE("WindowsD3d11Bridge::device");
    ID3D11Device *const asked = as_device(object);
    if (asked == nullptr) return false;
    asked->Release();
    return true;
}

bool WindowsD3d11Bridge::describe(void *object, void *device, D3d11Resource &described)
{
    AV_SCOPE("WindowsD3d11Bridge::describe");
    described = D3d11Resource();
    ID3D11Resource *const resource = as_resource(object);
    if (resource == nullptr) return false;
    ID3D11Device *const owner = as_device(device);
    const bool          ours = owner != nullptr && made_on(*resource, *owner);
    if (ours) described = description(*resource);
    if (owner != nullptr) owner->Release();
    resource->Release();
    return ours;
}

void WindowsD3d11Bridge::hold(void *object)
{
    AV_SCOPE("WindowsD3d11Bridge::hold");
    if (object != nullptr) static_cast<IUnknown *>(object)->AddRef();
}

void WindowsD3d11Bridge::let_go(void *object)
{
    AV_SCOPE("WindowsD3d11Bridge::let_go");
    if (object != nullptr) static_cast<IUnknown *>(object)->Release();
}

cl_int WindowsD3d11Bridge::read(void *device, void *resource, uint32_t subresource, clicd::D3d11Rows &rows)
{
    AV_SCOPE("WindowsD3d11Bridge::read");
    ID3D11Device *const   owner = as_device(device);
    ID3D11Resource *const source = as_resource(resource);
    ID3D11Resource *const staging =
        owner != nullptr && source != nullptr ? staging_for(*owner, *source, subresource, D3D11_CPU_ACCESS_READ)
                                              : nullptr;
    cl_int result = CL_OUT_OF_RESOURCES;
    if (staging != nullptr) {
        ID3D11DeviceContext *context = nullptr;
        owner->GetImmediateContext(&context);
        ID3D10Multithread *const section = section_of(*owner);
        enter(section);
        context->CopySubresourceRegion(staging, 0u, 0u, 0u, 0u, source, subresource, nullptr);
        /* The map waits for the copy, and the copy for every command made
         * before it: Direct3D's work on the resource is done. */
        result = visit_mapped(*context, section, *staging, D3D11_MAP_READ, rows);
        leave(section);
        if (section != nullptr) section->Release();
        context->Release();
        staging->Release();
    }
    if (source != nullptr) source->Release();
    if (owner != nullptr) owner->Release();
    return result;
}

cl_int WindowsD3d11Bridge::write(void *device, void *resource, uint32_t subresource, clicd::D3d11Rows &rows)
{
    AV_SCOPE("WindowsD3d11Bridge::write");
    ID3D11Device *const   owner = as_device(device);
    ID3D11Resource *const target = as_resource(resource);
    ID3D11Resource *const staging =
        owner != nullptr && target != nullptr ? staging_for(*owner, *target, subresource, D3D11_CPU_ACCESS_WRITE)
                                              : nullptr;
    cl_int result = CL_OUT_OF_RESOURCES;
    if (staging != nullptr) {
        ID3D11DeviceContext *context = nullptr;
        owner->GetImmediateContext(&context);
        ID3D10Multithread *const section = section_of(*owner);
        enter(section);
        result = visit_mapped(*context, section, *staging, D3D11_MAP_WRITE, rows);
        /* On the immediate context, so every command the application makes
         * on it from here is ordered after the copy. */
        if (result == CL_SUCCESS) context->CopySubresourceRegion(target, subresource, 0u, 0u, 0u, staging, 0u, nullptr);
        leave(section);
        if (section != nullptr) section->Release();
        context->Release();
        staging->Release();
    }
    if (target != nullptr) target->Release();
    if (owner != nullptr) owner->Release();
    return result;
}

cl_int WindowsD3d11Bridge::visit_mapped(ID3D11DeviceContext &context, ID3D10Multithread *section,
                                        ID3D11Resource &staging, D3D11_MAP type, clicd::D3d11Rows &rows)
{
    AV_SCOPE("WindowsD3d11Bridge::visit_mapped");
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(context.Map(&staging, 0u, type, 0u, &mapped)) || mapped.pData == nullptr) return CL_OUT_OF_RESOURCES;
    leave(section);
    const cl_int visited = rows.visit(static_cast<uint8_t *>(mapped.pData), mapped.RowPitch, mapped.DepthPitch);
    enter(section);
    context.Unmap(&staging, 0u);
    return visited;
}

ID3D11Device *WindowsD3d11Bridge::as_device(void *object)
{
    AV_SCOPE("WindowsD3d11Bridge::as_device");
    ID3D11Device *asked = nullptr;
    if (object == nullptr ||
        FAILED(static_cast<IUnknown *>(object)->QueryInterface(device_iid_, reinterpret_cast<void **>(&asked))))
        return nullptr;
    return asked;
}

ID3D11Resource *WindowsD3d11Bridge::as_resource(void *object)
{
    AV_SCOPE("WindowsD3d11Bridge::as_resource");
    ID3D11Resource *asked = nullptr;
    if (object == nullptr ||
        FAILED(static_cast<IUnknown *>(object)->QueryInterface(resource_iid_, reinterpret_cast<void **>(&asked))))
        return nullptr;
    return asked;
}

bool WindowsD3d11Bridge::made_on(ID3D11Resource &resource, ID3D11Device &device)
{
    AV_SCOPE("WindowsD3d11Bridge::made_on");
    ID3D11Device *owner = nullptr;
    resource.GetDevice(&owner);
    /* Both are the device's ID3D11Device, which COM answers as one pointer. */
    const bool same = owner == &device;
    if (owner != nullptr) owner->Release();
    return same;
}

/* The resource is the interface its dimension says, which its own GetType
 * answers, and an ID3D11Resource of a buffer or texture is that interface's
 * base: a cast down to it is the object itself. */
D3d11Resource WindowsD3d11Bridge::description(ID3D11Resource &resource)
{
    AV_SCOPE("WindowsD3d11Bridge::description");
    D3D11_RESOURCE_DIMENSION dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    resource.GetType(&dimension);
    if (dimension == D3D11_RESOURCE_DIMENSION_BUFFER) {
        D3D11_BUFFER_DESC desc = {};
        static_cast<ID3D11Buffer &>(resource).GetDesc(&desc);
        return D3d11Resource(D3d11Resource::Kind::buffer, 0u, desc.ByteWidth, 0u, 0u, 0u, 1u, 1u, 1u);
    }
    if (dimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
        D3D11_TEXTURE2D_DESC desc = {};
        static_cast<ID3D11Texture2D &>(resource).GetDesc(&desc);
        return D3d11Resource(D3d11Resource::Kind::texture2d, static_cast<uint32_t>(desc.Format), 0u, desc.Width, desc.Height, 1u,
                             desc.MipLevels, desc.ArraySize, desc.SampleDesc.Count);
    }
    if (dimension == D3D11_RESOURCE_DIMENSION_TEXTURE3D) {
        D3D11_TEXTURE3D_DESC desc = {};
        static_cast<ID3D11Texture3D &>(resource).GetDesc(&desc);
        return D3d11Resource(D3d11Resource::Kind::texture3d, static_cast<uint32_t>(desc.Format), 0u, desc.Width, desc.Height, desc.Depth,
                             desc.MipLevels, 1u, 1u);
    }
    return D3d11Resource();
}

ID3D11Resource *WindowsD3d11Bridge::staging_for(ID3D11Device &device, ID3D11Resource &resource, uint32_t subresource,
                                                UINT cpu_access)
{
    AV_SCOPE("WindowsD3d11Bridge::staging_for");
    D3D11_RESOURCE_DIMENSION dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    resource.GetType(&dimension);
    if (dimension != D3D11_RESOURCE_DIMENSION_BUFFER) return staging_texture(device, resource, subresource, cpu_access);
    D3D11_BUFFER_DESC desc = {};
    static_cast<ID3D11Buffer &>(resource).GetDesc(&desc);
    D3D11_BUFFER_DESC staged = {};
    staged.ByteWidth = desc.ByteWidth;
    staged.Usage = D3D11_USAGE_STAGING;
    staged.CPUAccessFlags = cpu_access;
    ID3D11Buffer *made = nullptr;
    return SUCCEEDED(device.CreateBuffer(&staged, nullptr, &made)) ? made : nullptr;
}

ID3D11Resource *WindowsD3d11Bridge::staging_texture(ID3D11Device &device, ID3D11Resource &resource,
                                                    uint32_t subresource, UINT cpu_access)
{
    AV_SCOPE("WindowsD3d11Bridge::staging_texture");
    const D3d11Resource described = description(resource);
    if (described.kind() == D3d11Resource::Kind::texture3d) {
        D3D11_TEXTURE3D_DESC staged = {};
        static_cast<ID3D11Texture3D &>(resource).GetDesc(&staged);
        staged.Width = described.width_of(subresource);
        staged.Height = described.height_of(subresource);
        staged.Depth = described.depth_of(subresource);
        staged.MipLevels = 1u;
        staged.Usage = D3D11_USAGE_STAGING;
        staged.BindFlags = 0u;
        staged.CPUAccessFlags = cpu_access;
        staged.MiscFlags = 0u;
        ID3D11Texture3D *made = nullptr;
        return SUCCEEDED(device.CreateTexture3D(&staged, nullptr, &made)) ? made : nullptr;
    }
    if (described.kind() != D3d11Resource::Kind::texture2d) return nullptr;
    D3D11_TEXTURE2D_DESC staged = {};
    static_cast<ID3D11Texture2D &>(resource).GetDesc(&staged);
    staged.Width = described.width_of(subresource);
    staged.Height = described.height_of(subresource);
    staged.MipLevels = 1u;
    staged.ArraySize = 1u;
    staged.SampleDesc.Count = 1u;
    staged.SampleDesc.Quality = 0u;
    staged.Usage = D3D11_USAGE_STAGING;
    staged.BindFlags = 0u;
    staged.CPUAccessFlags = cpu_access;
    staged.MiscFlags = 0u;
    ID3D11Texture2D *made = nullptr;
    return SUCCEEDED(device.CreateTexture2D(&staged, nullptr, &made)) ? made : nullptr;
}

ID3D10Multithread *WindowsD3d11Bridge::section_of(ID3D11Device &device)
{
    AV_SCOPE("WindowsD3d11Bridge::section_of");
    ID3D10Multithread *section = nullptr;
    if (FAILED(device.QueryInterface(multithread_iid_, reinterpret_cast<void **>(&section)))) return nullptr;
    return section;
}

void WindowsD3d11Bridge::enter(ID3D10Multithread *section)
{
    AV_SCOPE("WindowsD3d11Bridge::enter");
    if (section != nullptr) section->Enter();
}

void WindowsD3d11Bridge::leave(ID3D10Multithread *section)
{
    AV_SCOPE("WindowsD3d11Bridge::leave");
    if (section != nullptr) section->Leave();
}

}  // namespace av
