#include "um/opencl/windows_d3d11_bridge.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/opencl/d3d11_resource.hpp"
#include "icd/opencl/d3d11_rows.hpp"
#include "um/common/shared_rows_texture.hpp"

namespace av {

using clicd::D3d11Resource;

WindowsD3d11Bridge::WindowsD3d11Bridge(Kmt &kmt)
    : kmt_(kmt), device_iid_(__uuidof(ID3D11Device)), resource_iid_(__uuidof(ID3D11Resource)),
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

bool WindowsD3d11Bridge::d3d10_device(void *object)
{
    AV_SCOPE("WindowsD3d11Bridge::d3d10_device");
    return ten_.device(object);
}

bool WindowsD3d11Bridge::describe(void *object, void *device, D3d11Resource &described)
{
    AV_SCOPE("WindowsD3d11Bridge::describe");
    if (ten_.device(device)) return ten_.describe(object, device, described);
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
    if (ten_.device(device)) return ten_.read(device, resource, subresource, rows);
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
    if (ten_.device(device)) return ten_.write(device, resource, subresource, rows);
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
    const bool same = owner == &device;
    if (owner != nullptr) owner->Release();
    return same;
}

void *WindowsD3d11Bridge::twin(void *device, void *resource, uint32_t subresource, uint32_t kmt_device,
                               uint64_t &handle, uint64_t &bytes)
{
    AV_SCOPE("WindowsD3d11Bridge::twin");
    handle = 0u;
    bytes = 0u;
    if (ten_.device(device)) return nullptr;
    ID3D11Device *const   owner = as_device(device);
    ID3D11Resource *const source = as_resource(resource);
    ID3D11Texture2D      *made = nullptr;
    const D3d11Resource   described = source != nullptr ? description(*source) : D3d11Resource();
    if (owner != nullptr && described.kind() == D3d11Resource::Kind::texture2d && !described.multisampled()) {
        D3D11_TEXTURE2D_DESC shaped = {};
        shaped.Width = described.width_of(subresource);
        shaped.Height = described.height_of(subresource);
        shaped.MipLevels = 1u;
        shaped.ArraySize = 1u;
        shaped.Format = static_cast<DXGI_FORMAT>(described.format());
        shaped.SampleDesc.Count = 1u;
        shaped.Usage = D3D11_USAGE_DEFAULT;
        shaped.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        SharedRowsTexture twin(kmt_);
        if (twin.make(*owner, kmt_device, shaped)) {
            bytes = twin.bytes();
            handle = twin.take_handle();
            made = twin.take_texture();
        }
    }
    if (source != nullptr) source->Release();
    if (owner != nullptr) owner->Release();
    return made;
}

void WindowsD3d11Bridge::close_handle(uint64_t handle)
{
    AV_SCOPE("WindowsD3d11Bridge::close_handle");
    if (handle != 0u) CloseHandle(reinterpret_cast<HANDLE>(handle));
}

cl_int WindowsD3d11Bridge::to_twin(void *device, void *resource, uint32_t subresource, void *twin)
{
    AV_SCOPE("WindowsD3d11Bridge::to_twin");
    ID3D11Device *const   owner = as_device(device);
    ID3D11Resource *const source = as_resource(resource);
    ID3D11Resource *const target = as_resource(twin);
    cl_int                result = CL_OUT_OF_RESOURCES;
    if (owner != nullptr && source != nullptr && target != nullptr) {
        ID3D11DeviceContext *context = nullptr;
        owner->GetImmediateContext(&context);
        ID3D10Multithread *const section = section_of(*owner);
        enter(section);
        context->CopySubresourceRegion(target, 0u, 0u, 0u, 0u, source, subresource, nullptr);
        result = drain(*owner, *context, *target);
        leave(section);
        if (section != nullptr) section->Release();
        context->Release();
    }
    if (target != nullptr) target->Release();
    if (source != nullptr) source->Release();
    if (owner != nullptr) owner->Release();
    return result;
}

cl_int WindowsD3d11Bridge::from_twin(void *device, void *twin, void *resource, uint32_t subresource)
{
    AV_SCOPE("WindowsD3d11Bridge::from_twin");
    ID3D11Device *const   owner = as_device(device);
    ID3D11Resource *const source = as_resource(twin);
    ID3D11Resource *const target = as_resource(resource);
    cl_int                result = CL_OUT_OF_RESOURCES;
    if (owner != nullptr && source != nullptr && target != nullptr) {
        ID3D11DeviceContext *context = nullptr;
        owner->GetImmediateContext(&context);
        ID3D10Multithread *const section = section_of(*owner);
        enter(section);
        context->CopySubresourceRegion(target, subresource, 0u, 0u, 0u, source, 0u, nullptr);
        leave(section);
        if (section != nullptr) section->Release();
        context->Release();
        result = CL_SUCCESS;
    }
    if (target != nullptr) target->Release();
    if (source != nullptr) source->Release();
    if (owner != nullptr) owner->Release();
    return result;
}

cl_int WindowsD3d11Bridge::drain(ID3D11Device &device, ID3D11DeviceContext &context, ID3D11Resource &twin)
{
    AV_SCOPE("WindowsD3d11Bridge::drain");
    D3D11_TEXTURE2D_DESC probed = {};
    static_cast<ID3D11Texture2D &>(twin).GetDesc(&probed);
    probed.Width = 1u;
    probed.Height = 1u;
    probed.Usage = D3D11_USAGE_STAGING;
    probed.BindFlags = 0u;
    probed.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    probed.MiscFlags = 0u;
    ID3D11Texture2D *probe = nullptr;
    if (FAILED(device.CreateTexture2D(&probed, nullptr, &probe))) return CL_OUT_OF_RESOURCES;
    const D3D11_BOX texel = {0u, 0u, 0u, 1u, 1u, 1u};
    context.CopySubresourceRegion(probe, 0u, 0u, 0u, 0u, &twin, 0u, &texel);
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    const bool               waited = SUCCEEDED(context.Map(probe, 0u, D3D11_MAP_READ, 0u, &mapped));
    if (waited) context.Unmap(probe, 0u);
    probe->Release();
    return waited ? CL_SUCCESS : CL_OUT_OF_RESOURCES;
}

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

}
