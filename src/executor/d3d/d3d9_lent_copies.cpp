#include "executor/d3d/d3d9_lent_copies.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/owned_fd.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/source_memory.hpp"
#include "proto/d3d/d3d9_format_layout.hpp"

namespace av {

namespace {

constexpr uint64_t copy_deadline_nanoseconds = 5000000000u;

}

D3d9LentCopies::D3d9LentCopies(BlockSource &blocks) : copiers_(blocks), copier_of_(blocks, Pool::paged)
{
    AV_SCOPE("D3d9LentCopies::D3d9LentCopies");
}

D3d9LentCopies::~D3d9LentCopies()
{
    AV_SCOPE("D3d9LentCopies::~D3d9LentCopies");
    release();
}

Status D3d9LentCopies::copy(IDirect3DDevice9 *device, IDirect3DSurface9 *source, const d3d9proto::Rect &area, uint64_t handle,
                            SourceMemory &memory, uint32_t pitch, uint32_t x, uint32_t y)
{
    AV_SCOPE("D3d9LentCopies::copy");
    if (device == nullptr || source == nullptr || handle == 0u) return Status::invalid_parameter;
    D3DSURFACE_DESC desc = {};
    if (FAILED(source->GetDesc(&desc)) || desc.MultiSampleType != D3DMULTISAMPLE_NONE) return Status::not_supported;
    const D3d9FormatLayout layout(static_cast<uint32_t>(desc.Format));
    const uint32_t         texel = layout.element_bytes();
    if (area.left < 0 || area.top < 0 || area.right <= area.left || area.bottom <= area.top ||
        static_cast<uint32_t>(area.right) > desc.Width || static_cast<uint32_t>(area.bottom) > desc.Height ||
        layout.element_width() != 1u || layout.element_height() != 1u || texel == 0u || pitch % texel != 0u)
        return Status::invalid_parameter;
    const uint32_t wide = static_cast<uint32_t>(area.right - area.left);
    const uint32_t high = static_cast<uint32_t>(area.bottom - area.top);
    if (uint64_t{x} + wide > pitch / texel ||
        (uint64_t{y} + high - 1u) * pitch + (uint64_t{x} + wide) * texel > memory.size())
        return Status::invalid_parameter;
    Copier *const copier = copier_for(device);
    if (copier == nullptr) return Status::not_supported;
    const VulkanBuffer *const storage = storage_for(*copier, handle, memory);
    if (storage == nullptr) return Status::no_memory;
    VkImage       image = VK_NULL_HANDLE;
    VkImageLayout current = VK_IMAGE_LAYOUT_UNDEFINED;
    if (!copier->vulkan_.surface_image(source, image, current)) return Status::invalid_state;
    VkBufferImageCopy region = {};
    region.bufferOffset = uint64_t{y} * pitch + uint64_t{x} * texel;
    region.bufferRowLength = pitch / texel;
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
    region.imageOffset = {area.left, area.top, 0};
    region.imageExtent = {wide, high, 1u};
    const uint64_t value = ++copier->issued_;
    if (!copier->vulkan_.work().copy_to_buffer(image, current, storage->buffer(), region, copier->done_, value))
        return Status::invalid_state;
    return copier->done_.wait(value, copy_deadline_nanoseconds) ? Status::ok : Status::invalid_state;
}

D3d9LentCopies::Copier *D3d9LentCopies::copier_for(IDirect3DDevice9 *device)
{
    AV_SCOPE("D3d9LentCopies::copier_for");
    const uint64_t key = reinterpret_cast<uintptr_t>(device);
    Copier *const *known = copier_of_.find(key);
    if (known != nullptr) return (*known)->refused_ ? nullptr : *known;
    if (copier_of_.reserve(copier_of_.count() + 1u) != Status::ok) return nullptr;
    Copier *made = nullptr;
    if (copiers_.create(made) != Status::ok) return nullptr;
    made->issued_ = 0u;
    made->uses_ = 0u;
    made->refused_ = false;
    for (Storage &held : made->storages_) {
        held.memory_ = nullptr;
        held.handle_ = held.used_ = 0u;
    }
    uint32_t slot = 0u;
    static_cast<void>(copier_of_.insert(key, made, slot));
    if (made->vulkan_.connect(device) && made->vulkan_.table().imports_dma_bufs() &&
        made->done_.make_exportable(made->vulkan_.table()))
        return made;
    close(*made);
    made->refused_ = true;
    return nullptr;
}

const VulkanBuffer *D3d9LentCopies::storage_for(Copier &copier, uint64_t handle, SourceMemory &memory)
{
    AV_SCOPE("D3d9LentCopies::storage_for");
    Storage *oldest = &copier.storages_[0];
    for (Storage &held : copier.storages_) {
        if (held.buffer_.made() && held.handle_ == handle && held.memory_ == &memory) {
            held.used_ = ++copier.uses_;
            return &held.buffer_;
        }
        if (held.used_ < oldest->used_) oldest = &held;
    }
    if (oldest->buffer_.made()) {
        copier.vulkan_.work().idle();
        oldest->buffer_.release();
    }
    OwnedFd descriptor;
    if (!memory.share(descriptor) || !oldest->buffer_.import_dma_buf_to_draw(copier.vulkan_.table(), descriptor, memory.size()))
        return nullptr;
    oldest->memory_ = &memory;
    oldest->handle_ = handle;
    oldest->used_ = ++copier.uses_;
    return &oldest->buffer_;
}

void D3d9LentCopies::close(Copier &copier)
{
    AV_SCOPE("D3d9LentCopies::close");
    copier.vulkan_.work().idle();
    for (Storage &held : copier.storages_) {
        held.buffer_.release();
        held.memory_ = nullptr;
        held.handle_ = held.used_ = 0u;
    }
    copier.done_.release();
    copier.vulkan_.release();
}

void D3d9LentCopies::forget_device(IDirect3DDevice9 *device)
{
    AV_SCOPE("D3d9LentCopies::forget_device");
    Copier *going = nullptr;
    if (!copier_of_.take(reinterpret_cast<uintptr_t>(device), going)) return;
    close(*going);
    static_cast<void>(copiers_.destroy(going));
}

void D3d9LentCopies::release()
{
    AV_SCOPE("D3d9LentCopies::release");
    copier_of_.for_each([this](uint64_t, Copier *copier) { close(*copier); });
    copier_of_.clear();
    copiers_.destroy_all();
}

}
