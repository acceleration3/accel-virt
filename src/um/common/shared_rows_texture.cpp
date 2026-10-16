#include "um/common/shared_rows_texture.hpp"

#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "proto/resources/shared_allocation_data.hpp"
#include "um/common/shared_rows_scope.hpp"

#include <dxgi1_2.h>
#include <windows.h>

namespace av {

SharedRowsTexture::SharedRowsTexture(Kmt &kmt)
    : kmt_(kmt), texture_(nullptr), handle_(0u), object_id_(0u), bytes_(0u)
{
    AV_SCOPE("SharedRowsTexture::SharedRowsTexture");
}

SharedRowsTexture::~SharedRowsTexture()
{
    AV_SCOPE("SharedRowsTexture::~SharedRowsTexture");
    drop();
}

void SharedRowsTexture::drop()
{
    AV_SCOPE("SharedRowsTexture::drop");
    if (handle_ != 0u) static_cast<void>(kmt_.close_shared_object(handle_));
    if (texture_ != nullptr) texture_->Release();
    texture_ = nullptr;
    handle_ = 0u;
    object_id_ = 0u;
    bytes_ = 0u;
}

bool SharedRowsTexture::make(ID3D11Device &device, uint32_t kmt_device, const D3D11_TEXTURE2D_DESC &shape)
{
    AV_SCOPE("SharedRowsTexture::make");
    drop();
    D3D11_TEXTURE2D_DESC shared = shape;
    shared.MiscFlags |= D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
    {
        const SharedRowsScope rows;
        if (FAILED(device.CreateTexture2D(&shared, nullptr, &texture_))) {
            texture_ = nullptr;
            return false;
        }
    }
    IDXGIResource1 *resource = nullptr;
    HANDLE          handle = nullptr;
    const bool      named =
        SUCCEEDED(texture_->QueryInterface(__uuidof(IDXGIResource1), reinterpret_cast<void **>(&resource))) &&
        SUCCEEDED(resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
                                               nullptr, &handle));
    if (resource != nullptr) resource->Release();
    handle_ = named ? reinterpret_cast<uint64_t>(handle) : 0u;
    if (handle_ != 0u && identify(kmt_device)) return true;
    drop();
    return false;
}

bool SharedRowsTexture::identify(uint32_t kmt_device)
{
    AV_SCOPE("SharedRowsTexture::identify");
    SharedAllocationData carried;
    uint32_t             written = 0u;
    uint32_t             opened = 0u;
    const Status         read = kmt_.open_shared_object(kmt_device, handle_, opened, carried.lend(),
                                                        static_cast<uint32_t>(SharedAllocationData::encoded_size),
                                                        written);
    if (read != Status::ok) return false;
    static_cast<void>(kmt_.destroy_shared_allocation(kmt_device, opened));
    if (carried.reread(written) != Status::ok || !carried.assigned()) return false;
    object_id_ = carried.object_id();
    bytes_ = carried.size();
    return true;
}

ID3D11Texture2D *SharedRowsTexture::take_texture()
{
    AV_SCOPE("SharedRowsTexture::take_texture");
    ID3D11Texture2D *const taken = texture_;
    texture_ = nullptr;
    return taken;
}

uint64_t SharedRowsTexture::take_handle()
{
    AV_SCOPE("SharedRowsTexture::take_handle");
    const uint64_t taken = handle_;
    handle_ = 0u;
    return taken;
}

uint64_t SharedRowsTexture::object_id() const
{
    AV_SCOPE("SharedRowsTexture::object_id");
    return object_id_;
}

uint64_t SharedRowsTexture::bytes() const
{
    AV_SCOPE("SharedRowsTexture::bytes");
    return bytes_;
}

}  // namespace av
