#include "um/probe/d3d/windows_d3d11_shared_entry.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr HRESULT  invalid_argument = static_cast<HRESULT>(0x80070057u);
constexpr uint32_t channels = 4u;
constexpr float    full = 255.0F;

}  // namespace

WindowsD3d11SharedEntry::WindowsD3d11SharedEntry()
    : device_(nullptr), context_(nullptr), textures_{}, views_{}, staging_(nullptr)
{
    AV_SCOPE("WindowsD3d11SharedEntry::WindowsD3d11SharedEntry");
}

WindowsD3d11SharedEntry::~WindowsD3d11SharedEntry()
{
    AV_SCOPE("WindowsD3d11SharedEntry::~WindowsD3d11SharedEntry");
    release();
}

/* The staging texel is made with the device: every target is read through
 * it. */
int32_t WindowsD3d11SharedEntry::create_device()
{
    AV_SCOPE("WindowsD3d11SharedEntry::create_device");
    const HRESULT created = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0u, nullptr, 0u,
                                              D3D11_SDK_VERSION, &device_, nullptr, &context_);
    if (created < 0) return created;
    D3D11_TEXTURE2D_DESC described = {};
    described.Width = 1u;
    described.Height = 1u;
    described.MipLevels = 1u;
    described.ArraySize = 1u;
    described.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    described.SampleDesc.Count = 1u;
    described.Usage = D3D11_USAGE_STAGING;
    described.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    return device_->CreateTexture2D(&described, nullptr, &staging_);
}

int32_t WindowsD3d11SharedEntry::create_target(uint32_t target, uint32_t width, uint32_t height)
{
    AV_SCOPE("WindowsD3d11SharedEntry::create_target");
    if (target >= most_targets || textures_[target] != nullptr) return invalid_argument;
    D3D11_TEXTURE2D_DESC described = {};
    described.Width = width;
    described.Height = height;
    described.MipLevels = 1u;
    described.ArraySize = 1u;
    described.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    described.SampleDesc.Count = 1u;
    described.Usage = D3D11_USAGE_DEFAULT;
    described.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    described.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
    const HRESULT made = device_->CreateTexture2D(&described, nullptr, &textures_[target]);
    if (made < 0) return made;
    return device_->CreateRenderTargetView(textures_[target], nullptr, &views_[target]);
}

void WindowsD3d11SharedEntry::clear(uint32_t target, const uint8_t *colour)
{
    AV_SCOPE("WindowsD3d11SharedEntry::clear");
    if (target >= most_targets || views_[target] == nullptr) return;
    const float asked[channels] = {static_cast<float>(colour[0]) / full, static_cast<float>(colour[1]) / full,
                                   static_cast<float>(colour[2]) / full, static_cast<float>(colour[3]) / full};
    context_->ClearRenderTargetView(views_[target], asked);
}

int32_t WindowsD3d11SharedEntry::read_texel(uint32_t target, uint32_t x, uint32_t y, uint8_t *texel)
{
    AV_SCOPE("WindowsD3d11SharedEntry::read_texel");
    if (target >= most_targets || textures_[target] == nullptr) return invalid_argument;
    const D3D11_BOX one = {x, y, 0u, x + 1u, y + 1u, 1u};
    context_->CopySubresourceRegion(staging_, 0u, 0u, 0u, 0u, textures_[target], 0u, &one);
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    const HRESULT            asked = context_->Map(staging_, 0u, D3D11_MAP_READ, 0u, &mapped);
    if (asked < 0) return asked;
    for (uint32_t byte = 0u; byte < channels; byte++) texel[byte] = static_cast<const uint8_t *>(mapped.pData)[byte];
    context_->Unmap(staging_, 0u);
    return asked;
}

int32_t WindowsD3d11SharedEntry::removed_reason()
{
    AV_SCOPE("WindowsD3d11SharedEntry::removed_reason");
    return device_->GetDeviceRemovedReason();
}

void WindowsD3d11SharedEntry::release()
{
    AV_SCOPE("WindowsD3d11SharedEntry::release");
    for (uint32_t target = 0u; target < most_targets; target++) {
        if (views_[target] != nullptr) views_[target]->Release();
        if (textures_[target] != nullptr) textures_[target]->Release();
        views_[target] = nullptr;
        textures_[target] = nullptr;
    }
    if (staging_ != nullptr) staging_->Release();
    if (context_ != nullptr) context_->Release();
    if (device_ != nullptr) device_->Release();
    staging_ = nullptr;
    context_ = nullptr;
    device_ = nullptr;
}

}  // namespace av
