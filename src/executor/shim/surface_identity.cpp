#include "executor/shim/surface_identity.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

namespace {

constexpr GUID surface_tag = {0x7a2c5e19, 0x4b3d, 0x4e81, {0x9f, 0x06, 0x2d, 0xc4, 0x58, 0xb1, 0x37, 0xe2}};

}  // namespace

SurfaceIdentity::SurfaceIdentity(uint64_t surface) : surface_(surface)
{
    AV_SCOPE("SurfaceIdentity::SurfaceIdentity");
}

SurfaceIdentity::SurfaceIdentity(ID3D11DeviceChild *resource) : surface_(0u)
{
    AV_SCOPE("SurfaceIdentity::SurfaceIdentity");
    UINT     size = sizeof surface_;
    uint64_t read = 0u;
    if (resource != nullptr && SUCCEEDED(resource->GetPrivateData(surface_tag, &size, &read)) && size == sizeof read)
        surface_ = read;
}

SurfaceIdentity::SurfaceIdentity(ID3D12Object *resource) : surface_(0u)
{
    AV_SCOPE("SurfaceIdentity::SurfaceIdentity");
    UINT     size = sizeof surface_;
    uint64_t read = 0u;
    if (resource != nullptr && SUCCEEDED(resource->GetPrivateData(surface_tag, &size, &read)) && size == sizeof read)
        surface_ = read;
}

uint64_t SurfaceIdentity::surface() const
{
    AV_SCOPE("SurfaceIdentity::surface");
    return surface_;
}

bool SurfaceIdentity::mark(ID3D11DeviceChild *resource) const
{
    AV_SCOPE("SurfaceIdentity::mark");
    return resource != nullptr && surface_ != 0u &&
           SUCCEEDED(resource->SetPrivateData(surface_tag, sizeof surface_, &surface_));
}

bool SurfaceIdentity::mark(ID3D12Object *resource) const
{
    AV_SCOPE("SurfaceIdentity::mark");
    return resource != nullptr && surface_ != 0u &&
           SUCCEEDED(resource->SetPrivateData(surface_tag, sizeof surface_, &surface_));
}

}  // namespace av
