#include "executor/shim/carried_share.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/shim/exportable_images.hpp"
#include "executor/shim/surface_identity.hpp"

#include <unistd.h>

namespace av {

CarriedShare::CarriedShare(uint32_t domain, uint64_t id)
    : id_(id), domain_(domain), dimension_(Dimension::none), width_(0u), height_(0u), depth_or_array_(0u), mips_(0u),
      format_(0u), samples_(0u), quality_(0u), bind_(0u), misc_(0u), stride_(0u), fd_(-1)
{
    AV_SCOPE("CarriedShare::CarriedShare");
}

CarriedShare::~CarriedShare()
{
    AV_SCOPE("CarriedShare::~CarriedShare");
    if (fd_ >= 0) ::close(fd_);
}

bool CarriedShare::record(IUnknown *resource, ExportableImages &images)
{
    AV_SCOPE("CarriedShare::record");
    ID3D11Resource *typed = nullptr;
    if (resource == nullptr ||
        FAILED(resource->QueryInterface(__uuidof(ID3D11Resource), reinterpret_cast<void **>(&typed))))
        return false;
    D3D11_RESOURCE_DIMENSION kind = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    typed->GetType(&kind);
    if (kind == D3D11_RESOURCE_DIMENSION_BUFFER) {
        D3D11_BUFFER_DESC desc = {};
        static_cast<ID3D11Buffer *>(typed)->GetDesc(&desc);
        dimension_ = Dimension::buffer;
        width_ = desc.ByteWidth;
        bind_ = desc.BindFlags;
        misc_ = desc.MiscFlags;
        stride_ = desc.StructureByteStride;
    } else if (kind == D3D11_RESOURCE_DIMENSION_TEXTURE1D) {
        D3D11_TEXTURE1D_DESC desc = {};
        static_cast<ID3D11Texture1D *>(typed)->GetDesc(&desc);
        dimension_ = Dimension::texture1d;
        width_ = desc.Width;
        mips_ = desc.MipLevels;
        depth_or_array_ = desc.ArraySize;
        format_ = static_cast<uint32_t>(desc.Format);
        bind_ = desc.BindFlags;
        misc_ = desc.MiscFlags;
    } else if (kind == D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
        D3D11_TEXTURE2D_DESC desc = {};
        ID3D11Texture2D *const texture = static_cast<ID3D11Texture2D *>(typed);
        texture->GetDesc(&desc);
        dimension_ = Dimension::texture2d;
        width_ = desc.Width;
        height_ = desc.Height;
        mips_ = desc.MipLevels;
        depth_or_array_ = desc.ArraySize;
        format_ = static_cast<uint32_t>(desc.Format);
        samples_ = desc.SampleDesc.Count;
        quality_ = desc.SampleDesc.Quality;
        bind_ = desc.BindFlags;
        misc_ = desc.MiscFlags;
        if (!images.export_memory(texture, fd_)) fd_ = -1;
    } else if (kind == D3D11_RESOURCE_DIMENSION_TEXTURE3D) {
        D3D11_TEXTURE3D_DESC desc = {};
        static_cast<ID3D11Texture3D *>(typed)->GetDesc(&desc);
        dimension_ = Dimension::texture3d;
        width_ = desc.Width;
        height_ = desc.Height;
        depth_or_array_ = desc.Depth;
        mips_ = desc.MipLevels;
        format_ = static_cast<uint32_t>(desc.Format);
        bind_ = desc.BindFlags;
        misc_ = desc.MiscFlags;
    }
    typed->Release();
    return dimension_ != Dimension::none;
}

/* Over the same memory where there is some, so what was drawn there is still
 * there; a texture whose memory will not be taken again is made anew. */
IUnknown *CarriedShare::remake_texture2d(ID3D11Device5 *device, ExportableImages &images)
{
    AV_SCOPE("CarriedShare::remake_texture2d");
    D3D11_TEXTURE2D_DESC1 desc = {};
    desc.Width = width_;
    desc.Height = height_;
    desc.MipLevels = mips_;
    desc.ArraySize = depth_or_array_;
    desc.Format = static_cast<DXGI_FORMAT>(format_);
    desc.SampleDesc = {samples_, quality_};
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = bind_;
    desc.MiscFlags = misc_;
    if (fd_ >= 0) {
        ID3D11Texture2D *const adopted = images.adopt(fd_, desc);
        fd_ = -1;
        if (adopted != nullptr) return adopted;
    }
    const D3D11_TEXTURE2D_DESC plain = {width_, height_, mips_, depth_or_array_, desc.Format, desc.SampleDesc,
                                        D3D11_USAGE_DEFAULT, bind_, 0u, misc_};
    ID3D11Texture2D *made = nullptr;
    return SUCCEEDED(device->CreateTexture2D(&plain, nullptr, &made)) ? made : nullptr;
}

IUnknown *CarriedShare::remake(ID3D11Device5 *device, ExportableImages &images)
{
    AV_SCOPE("CarriedShare::remake");
    IUnknown *made = nullptr;
    if (device == nullptr) return nullptr;
    if (dimension_ == Dimension::buffer) {
        const D3D11_BUFFER_DESC desc = {width_, D3D11_USAGE_DEFAULT, bind_, 0u, misc_, stride_};
        ID3D11Buffer           *buffer = nullptr;
        if (SUCCEEDED(device->CreateBuffer(&desc, nullptr, &buffer))) made = buffer;
    } else if (dimension_ == Dimension::texture1d) {
        const D3D11_TEXTURE1D_DESC desc = {width_, mips_, depth_or_array_, static_cast<DXGI_FORMAT>(format_),
                                           D3D11_USAGE_DEFAULT, bind_, 0u, misc_};
        ID3D11Texture1D *texture = nullptr;
        if (SUCCEEDED(device->CreateTexture1D(&desc, nullptr, &texture))) made = texture;
    } else if (dimension_ == Dimension::texture2d) {
        made = remake_texture2d(device, images);
    } else if (dimension_ == Dimension::texture3d) {
        const D3D11_TEXTURE3D_DESC desc = {width_, height_, depth_or_array_, mips_, static_cast<DXGI_FORMAT>(format_),
                                           D3D11_USAGE_DEFAULT, bind_, 0u, misc_};
        ID3D11Texture3D *texture = nullptr;
        if (SUCCEEDED(device->CreateTexture3D(&desc, nullptr, &texture))) made = texture;
    }
    ID3D11DeviceChild *child = nullptr;
    if (made != nullptr &&
        SUCCEEDED(made->QueryInterface(__uuidof(ID3D11DeviceChild), reinterpret_cast<void **>(&child)))) {
        static_cast<void>(SurfaceIdentity(id_).mark(child));
        child->Release();
    }
    return made;
}

uint32_t CarriedShare::domain() const
{
    AV_SCOPE("CarriedShare::domain");
    return domain_;
}

uint64_t CarriedShare::id() const
{
    AV_SCOPE("CarriedShare::id");
    return id_;
}

/* Only a texture's memory is carried, and only one recorded has a kind. */
bool CarriedShare::invariants_hold() const
{
    AV_SCOPE("CarriedShare::invariants_hold");
    return (fd_ < 0 || dimension_ == Dimension::texture2d) && id_ != 0u;
}

}  // namespace av
