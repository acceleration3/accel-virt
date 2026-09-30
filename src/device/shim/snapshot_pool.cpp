#include "device/shim/snapshot_pool.hpp"

#include "core/trace/function_scope.hpp"

#include "device/shim/d3d_headers.hpp"

namespace av {

SnapshotPool::SnapshotPool() : slots_(), taken_(0u)
{
    AV_SCOPE("SnapshotPool::SnapshotPool");
}

SnapshotPool::~SnapshotPool()
{
    AV_SCOPE("SnapshotPool::~SnapshotPool");
    for (Slot &slot : slots_)
        if (slot.texture_ != nullptr) slot.texture_->Release();
}

SnapshotPool::Slot *SnapshotPool::find(uint64_t key)
{
    AV_SCOPE("SnapshotPool::find");
    for (Slot &slot : slots_)
        if (slot.key_ == key) return &slot;
    return nullptr;
}

SnapshotPool::Slot *SnapshotPool::room(ID3D11DeviceContext4 *context, uint32_t width, uint32_t height, uint32_t format)
{
    AV_SCOPE("SnapshotPool::room");
    Slot *free = nullptr;
    Slot *oldest = nullptr;
    for (Slot &slot : slots_) {
        if (slot.key_ == 0u && slot.texture_ != nullptr && slot.width_ == width && slot.height_ == height &&
            slot.format_ == format)
            return &slot;
        if (slot.key_ == 0u && free == nullptr) free = &slot;
        if (slot.key_ != 0u && !slot.mapped_ && (oldest == nullptr || slot.taken_ < oldest->taken_)) oldest = &slot;
    }
    Slot *const chosen = free != nullptr ? free : oldest;
    if (chosen != nullptr) forget(context, *chosen);
    return chosen;
}

void SnapshotPool::forget(ID3D11DeviceContext4 *context, Slot &slot)
{
    AV_SCOPE("SnapshotPool::forget");
    if (slot.mapped_) context->Unmap(slot.texture_, 0u);
    slot.key_ = 0u;
    slot.mapped_ = false;
}

bool SnapshotPool::take(ID3D11Device5 *device, ID3D11DeviceContext4 *context, uint64_t key, ID3D11Texture2D *source,
                        uint32_t subresource, uint32_t width, uint32_t height, uint32_t format)
{
    AV_SCOPE("SnapshotPool::take");
    if (key == 0u || find(key) != nullptr) return false;
    Slot *const slot = room(context, width, height, format);
    if (slot == nullptr) return false;
    const bool fits = slot->texture_ != nullptr && slot->width_ == width && slot->height_ == height &&
                      slot->format_ == format;
    if (!fits) {
        if (slot->texture_ != nullptr) slot->texture_->Release();
        slot->texture_ = nullptr;
        const D3D11_TEXTURE2D_DESC staging = {width, height, 1u, 1u, static_cast<DXGI_FORMAT>(format), {1u, 0u},
                                              D3D11_USAGE_STAGING, 0u, D3D11_CPU_ACCESS_READ, 0u};
        if (FAILED(device->CreateTexture2D(&staging, nullptr, &slot->texture_))) return false;
        slot->width_ = width;
        slot->height_ = height;
        slot->format_ = format;
    }
    context->CopySubresourceRegion(slot->texture_, 0u, 0u, 0u, 0u, source, subresource, nullptr);
    context->Flush();
    slot->key_ = key;
    slot->taken_ = ++taken_;
    return true;
}

SnapshotPool::Reading SnapshotPool::read(ID3D11DeviceContext4 *context, uint64_t key, RvkSnapshotView &view)
{
    AV_SCOPE("SnapshotPool::read");
    Slot *const slot = key != 0u ? find(key) : nullptr;
    if (slot == nullptr || slot->mapped_) return Reading::absent;
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    const HRESULT result = context->Map(slot->texture_, 0u, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);
    if (result == DXGI_ERROR_WAS_STILL_DRAWING) return Reading::copying;
    if (FAILED(result)) {
        forget(context, *slot);
        return Reading::absent;
    }
    slot->mapped_ = true;
    view.pixels = static_cast<const uint8_t *>(mapped.pData);
    view.row_pitch = mapped.RowPitch;
    view.width = slot->width_;
    view.height = slot->height_;
    view.reserved = 0u;
    return Reading::mapped;
}

void SnapshotPool::release(ID3D11DeviceContext4 *context, uint64_t key)
{
    AV_SCOPE("SnapshotPool::release");
    Slot *const slot = key != 0u ? find(key) : nullptr;
    if (slot != nullptr) forget(context, *slot);
}

void SnapshotPool::clear(ID3D11DeviceContext4 *context)
{
    AV_SCOPE("SnapshotPool::clear");
    for (Slot &slot : slots_) {
        forget(context, slot);
        if (slot.texture_ != nullptr) slot.texture_->Release();
        slot.texture_ = nullptr;
    }
}

}  // namespace av
