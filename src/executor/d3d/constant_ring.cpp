#include "executor/d3d/constant_ring.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

ConstantRing::ConstantRing() : ring_(nullptr), used_(0u), wrapped_(false), kept_{}, kept_count_(0u), devices_{}, device_count_(0u)
{
    AV_SCOPE("ConstantRing::ConstantRing");
}

ConstantRing::~ConstantRing()
{
    AV_SCOPE("ConstantRing::~ConstantRing");
    close();
}

void ConstantRing::close()
{
    AV_SCOPE("ConstantRing::close");
    if (ring_ != nullptr) ring_->Release();
    ring_ = nullptr;
    used_ = 0u;
    kept_count_ = 0u;
    device_count_ = 0u;
}

uint32_t ConstantRing::kept_index(uint32_t id) const
{
    AV_SCOPE("ConstantRing::kept_index");
    for (uint32_t index = 0u; index < kept_count_; index++)
        if (kept_[index].id_ == id) return index;
    return none;
}

ConstantRing::DeviceBindings *ConstantRing::bindings_of(uint32_t device_id, bool make)
{
    AV_SCOPE("ConstantRing::bindings_of");
    for (uint32_t index = 0u; index < device_count_; index++)
        if (devices_[index].device_ == device_id) return &devices_[index];
    if (!make || device_count_ == max_devices) return nullptr;
    DeviceBindings &made = devices_[device_count_++];
    made.device_ = device_id;
    for (auto &stage : made.slots_)
        for (Binding &slot : stage) slot = Binding{0u, none, 0u, 0u, nullptr};
    return &made;
}

/* The window a binding sees of a kept buffer: the guest's own window of it,
 * moved to where the buffer is in the ring. Both ends in whole 16-constant
 * blocks, which is what SetConstantBuffers1 takes. */
void ConstantRing::window(const Kept &kept, const Binding &binding, uint32_t &first, uint32_t &count) const
{
    AV_SCOPE("ConstantRing::window");
    constexpr uint32_t block = window_bytes / bytes_per_constant;
    const uint32_t     whole = (kept.bytes_ / bytes_per_constant + block - 1u) / block * block;
    first = kept.offset_ / bytes_per_constant + binding.first_;
    count = binding.count_ == 0u || binding.count_ > whole ? whole : binding.count_;
}

/* The buffer's shadow, appended to the ring. A full ring is discarded and
 * every kept buffer written again from its start, since the windows onto the
 * old contents read the new storage from then on. */
bool ConstantRing::write(ID3D11Device5 *device, ID3D11DeviceContext4 *context, Kept &kept)
{
    AV_SCOPE("ConstantRing::write");
    if (ring_ == nullptr) {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = ring_bytes;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(device->CreateBuffer(&desc, nullptr, &ring_))) {
            ring_ = nullptr;
            return false;
        }
        used_ = ring_bytes;
    }
    const uint32_t span = (kept.bytes_ + window_bytes - 1u) / window_bytes * window_bytes;
    wrapped_ = false;
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (used_ + span <= ring_bytes) {
        if (FAILED(context->Map(ring_, 0u, D3D11_MAP_WRITE_NO_OVERWRITE, 0u, &mapped))) return false;
        kept.offset_ = used_;
        __builtin_memcpy(static_cast<uint8_t *>(mapped.pData) + used_, kept.shadow_, kept.bytes_);
        used_ += span;
        context->Unmap(ring_, 0u);
        return true;
    }
    if (FAILED(context->Map(ring_, 0u, D3D11_MAP_WRITE_DISCARD, 0u, &mapped))) return false;
    wrapped_ = true;
    used_ = 0u;
    for (uint32_t index = 0u; index < kept_count_; index++) {
        Kept &other = kept_[index];
        other.offset_ = used_;
        __builtin_memcpy(static_cast<uint8_t *>(mapped.pData) + used_, other.shadow_, other.bytes_);
        used_ += (other.bytes_ + window_bytes - 1u) / window_bytes * window_bytes;
    }
    context->Unmap(ring_, 0u);
    return true;
}

bool ConstantRing::update(ID3D11Device5 *device, ID3D11DeviceContext4 *context, uint32_t device_id, uint32_t id,
                          ID3D11Buffer *buffer, const uint8_t *data, uint32_t bytes)
{
    AV_SCOPE("ConstantRing::update");
    D3D11_BUFFER_DESC desc = {};
    buffer->GetDesc(&desc);
    if (desc.Usage != D3D11_USAGE_DYNAMIC || desc.BindFlags != D3D11_BIND_CONSTANT_BUFFER || desc.ByteWidth != bytes ||
        bytes > largest || bytes == 0u)
        return false;
    uint32_t index = kept_index(id);
    if (index == none) {
        if (kept_count_ == max_buffers) return false;
        index = kept_count_++;
        kept_[index].id_ = id;
        kept_[index].offset_ = none;
    }
    Kept &kept = kept_[index];
    kept.bytes_ = bytes;
    __builtin_memcpy(kept.shadow_, data, bytes);
    if (!write(device, context, kept)) {
        kept_[index] = kept_[--kept_count_];
        return false;
    }
    DeviceBindings *const current = bindings_of(device_id, false);
    if (current == nullptr) return true;
    if (wrapped_) {
        rebind(context, *current);
        return true;
    }
    /* Only this buffer moved: its slots are found by id, without a search of
     * the kept list per slot. */
    for (uint32_t stage = 0u; stage < stage_count; stage++) {
        for (uint32_t slot = 0u; slot < slot_count; slot++) {
            Binding &binding = current->slots_[stage][slot];
            if (binding.id_ != id) continue;
            uint32_t first = 0u, count = 0u;
            window(kept, binding, first, count);
            set_one(context, stage, slot, ring_, first, count);
            binding.offset_ = kept.offset_;
        }
    }
    return true;
}

void ConstantRing::bind(uint32_t device_id, uint32_t stage, uint32_t start, uint32_t count, const uint32_t *ids,
                        ID3D11Buffer **buffers, uint32_t *first, uint32_t *counts)
{
    AV_SCOPE("ConstantRing::bind");
    DeviceBindings *const device = bindings_of(device_id, true);
    if (device == nullptr || stage >= stage_count) return;
    for (uint32_t index = 0u; index < count && start + index < slot_count; index++) {
        Binding &slot = device->slots_[stage][start + index];
        slot = Binding{ids[index], none, first[index], counts[index], buffers[index]};
        const uint32_t kept = ids[index] == 0u ? none : kept_index(ids[index]);
        if (kept == none || ring_ == nullptr) continue;
        window(kept_[kept], slot, first[index], counts[index]);
        buffers[index] = ring_;
        slot.offset_ = kept_[kept].offset_;
    }
}

/* Every slot of `device` whose window is not where its buffer is now: onto
 * the ring where it moved, or back to its own storage once let go. */
void ConstantRing::rebind(ID3D11DeviceContext4 *context, DeviceBindings &device)
{
    AV_SCOPE("ConstantRing::rebind");
    for (uint32_t stage = 0u; stage < stage_count; stage++) {
        for (uint32_t slot = 0u; slot < slot_count; slot++) {
            Binding &binding = device.slots_[stage][slot];
            if (binding.id_ == 0u) continue;
            const uint32_t kept = kept_index(binding.id_);
            if (kept != none && binding.offset_ != kept_[kept].offset_) {
                uint32_t first = 0u, count = 0u;
                window(kept_[kept], binding, first, count);
                set_one(context, stage, slot, ring_, first, count);
                binding.offset_ = kept_[kept].offset_;
            } else if (kept == none && binding.offset_ != none) {
                set_one(context, stage, slot, binding.buffer_, binding.first_, binding.count_);
                binding.offset_ = none;
            }
        }
    }
}

void ConstantRing::set_one(ID3D11DeviceContext4 *context, uint32_t stage, uint32_t slot, ID3D11Buffer *buffer,
                           uint32_t first, uint32_t count) const
{
    AV_SCOPE("ConstantRing::set_one");
    const UINT first_constant = first;
    const UINT constants = count;
    switch (stage) {
    case 0u: context->VSSetConstantBuffers1(slot, 1u, &buffer, &first_constant, &constants); break;
    case 1u: context->HSSetConstantBuffers1(slot, 1u, &buffer, &first_constant, &constants); break;
    case 2u: context->DSSetConstantBuffers1(slot, 1u, &buffer, &first_constant, &constants); break;
    case 3u: context->GSSetConstantBuffers1(slot, 1u, &buffer, &first_constant, &constants); break;
    case 4u: context->PSSetConstantBuffers1(slot, 1u, &buffer, &first_constant, &constants); break;
    default: context->CSSetConstantBuffers1(slot, 1u, &buffer, &first_constant, &constants); break;
    }
}

void ConstantRing::selected(ID3D11DeviceContext4 *context, uint32_t device_id)
{
    AV_SCOPE("ConstantRing::selected");
    DeviceBindings *const device = bindings_of(device_id, false);
    if (device != nullptr) rebind(context, *device);
}

void ConstantRing::cleared(uint32_t device_id)
{
    AV_SCOPE("ConstantRing::cleared");
    DeviceBindings *const device = bindings_of(device_id, false);
    if (device == nullptr) return;
    for (auto &stage : device->slots_)
        for (Binding &slot : stage) slot = Binding{0u, none, 0u, 0u, nullptr};
}

void ConstantRing::release(ID3D11DeviceContext4 *context, uint32_t device_id, uint32_t id, ID3D11Buffer *buffer)
{
    AV_SCOPE("ConstantRing::release");
    const uint32_t index = kept_index(id);
    if (index == none) return;
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(context->Map(buffer, 0u, D3D11_MAP_WRITE_DISCARD, 0u, &mapped))) {
        __builtin_memcpy(mapped.pData, kept_[index].shadow_, kept_[index].bytes_);
        context->Unmap(buffer, 0u);
    }
    kept_[index] = kept_[--kept_count_];
    DeviceBindings *const current = bindings_of(device_id, false);
    if (current != nullptr) rebind(context, *current);
}

void ConstantRing::forget(uint32_t id)
{
    AV_SCOPE("ConstantRing::forget");
    const uint32_t index = kept_index(id);
    if (index != none) kept_[index] = kept_[--kept_count_];
    for (uint32_t device = 0u; device < device_count_; device++)
        for (auto &stage : devices_[device].slots_)
            for (Binding &slot : stage)
                if (slot.id_ == id) slot = Binding{0u, none, 0u, 0u, nullptr};
}

void ConstantRing::forget_device(uint32_t device_id)
{
    AV_SCOPE("ConstantRing::forget_device");
    for (uint32_t index = 0u; index < device_count_; index++) {
        if (devices_[index].device_ != device_id) continue;
        devices_[index] = devices_[--device_count_];
        return;
    }
}

}  // namespace av
