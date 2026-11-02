#include "executor/shim/d3d12_frame_pool.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "proto/d3d/dxgi_format.hpp"
#include "proto/d3d/dxgi_format_layout.hpp"

namespace av {

D3d12FramePool::D3d12FramePool() : device_(nullptr), frames_()
{
    AV_SCOPE("D3d12FramePool::D3d12FramePool");
}

D3d12FramePool::~D3d12FramePool()
{
    AV_SCOPE("D3d12FramePool::~D3d12FramePool");
    bind(nullptr);
}

void D3d12FramePool::bind(ID3D12Device *device)
{
    AV_SCOPE("D3d12FramePool::bind");
    if (device == device_) return;
    if (device == nullptr || device_ == nullptr)
        frames_.each([](Frame &frame) { frame.drop(); });
    else
        frames_.each([](Frame &frame) { frame.forsake(); });
    device_ = device;
}

StagedFrame *D3d12FramePool::stage(ID3D12CommandQueue *queue, ID3D12Resource *source)
{
    AV_SCOPE("D3d12FramePool::stage");
    if (device_ == nullptr || queue == nullptr || source == nullptr) return nullptr;
    if (queue->GetDesc().Type != D3D12_COMMAND_LIST_TYPE_DIRECT) return nullptr;
    const D3D12_RESOURCE_DESC desc = source->GetDesc();
    const bool                resolve = desc.SampleDesc.Count > 1u;
    const uint32_t            typed = DxgiFormat(static_cast<uint32_t>(desc.Format)).resolved();
    if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || !DxgiFormatLayout(static_cast<uint32_t>(desc.Format)).word_texels() || (resolve && typed == 0u))
        return nullptr;
    D3D12_RESOURCE_DESC single = desc;
    single.MipLevels = 1u;
    single.DepthOrArraySize = 1u;
    single.SampleDesc = {1u, 0u};
    single.Alignment = 0u;
    single.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    single.Flags = D3D12_RESOURCE_FLAG_NONE;
    if (resolve) {
        single.Format = static_cast<DXGI_FORMAT>(typed);
        single.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    }
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    UINT                               rows = 0u;
    UINT64                             row_bytes = 0u, total = 0u;
    device_->GetCopyableFootprints(&single, 0u, 1u, 0u, &footprint, &rows, &row_bytes, &total);
    if (total == 0u || total == ~UINT64{0}) return nullptr;
    Frame *const frame = claim(total);
    if (frame == nullptr) return nullptr;
    if (!frame->prepare(device_, total, resolve ? &single : nullptr) ||
        !frame->copy(queue, source, &footprint, static_cast<uint32_t>(desc.Width), desc.Height, resolve)) {
        frame->release();
        return nullptr;
    }
    return frame;
}

D3d12FramePool::Frame *D3d12FramePool::claim(uint64_t bytes)
{
    AV_SCOPE("D3d12FramePool::claim");
    return frames_.claim([&](const Frame &frame) { return frame.holds(bytes); });
}

D3d12FramePool::Frame::Frame()
    : buffer_(nullptr), resolved_(nullptr), resolved_format_(0u), mapped_(nullptr), bytes_(0u), allocator_(nullptr), list_(nullptr), fence_(nullptr),
      value_(0u), offset_(0u), row_pitch_(0u), device_(nullptr)
{
    AV_SCOPE("D3d12FramePool::Frame::Frame");
}

D3d12FramePool::Frame::~Frame()
{
    AV_SCOPE("D3d12FramePool::Frame::~Frame");
    drop();
}

bool D3d12FramePool::Frame::claim()
{
    AV_SCOPE("D3d12FramePool::Frame::claim");
    return idle() && StagedFrame::claim();
}

bool D3d12FramePool::Frame::idle() const
{
    AV_SCOPE("D3d12FramePool::Frame::idle");
    return forsaken() || fence_ == nullptr || fence_->GetCompletedValue() >= value_;
}

bool D3d12FramePool::Frame::holds(uint64_t bytes) const
{
    AV_SCOPE("D3d12FramePool::Frame::holds");
    return buffer_ != nullptr && bytes_ >= bytes;
}

bool D3d12FramePool::Frame::prepare(ID3D12Device *device, uint64_t bytes, const void *resolved)
{
    AV_SCOPE("D3d12FramePool::Frame::prepare");
    if (forsaken()) {
        drop();
        readmit();
    }
    device_ = device;
    if (resolved != nullptr) {
        const auto &wanted = *static_cast<const D3D12_RESOURCE_DESC *>(resolved);
        const bool  fits = resolved_ != nullptr && resolved_format_ == static_cast<uint32_t>(wanted.Format) &&
                          resolved_->GetDesc().Width == wanted.Width && resolved_->GetDesc().Height == wanted.Height;
        if (!fits) {
            if (resolved_ != nullptr) resolved_->Release();
            resolved_ = nullptr;
            D3D12_HEAP_PROPERTIES heap = {};
            heap.Type = D3D12_HEAP_TYPE_DEFAULT;
            if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &wanted,
                                                       D3D12_RESOURCE_STATE_RESOLVE_DEST, nullptr,
                                                       __uuidof(ID3D12Resource), reinterpret_cast<void **>(&resolved_))))
                return false;
            resolved_format_ = static_cast<uint32_t>(wanted.Format);
        }
    }
    if (!holds(bytes)) {
        if (buffer_ != nullptr) buffer_->Release();
        buffer_ = nullptr;
        mapped_ = nullptr;
        bytes_ = 0u;
        D3D12_HEAP_PROPERTIES heap = {};
        heap.Type = D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = bytes;
        desc.Height = 1u;
        desc.DepthOrArraySize = 1u;
        desc.MipLevels = 1u;
        desc.SampleDesc.Count = 1u;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST,
                                                   nullptr, __uuidof(ID3D12Resource),
                                                   reinterpret_cast<void **>(&buffer_))))
            return false;
        void *pointer = nullptr;
        if (FAILED(buffer_->Map(0u, nullptr, &pointer)) || pointer == nullptr) {
            buffer_->Release();
            buffer_ = nullptr;
            return false;
        }
        mapped_ = static_cast<const uint8_t *>(pointer);
        bytes_ = bytes;
    }
    if (allocator_ == nullptr &&
        FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator),
                                              reinterpret_cast<void **>(&allocator_))))
        return false;
    if (list_ == nullptr) {
        if (FAILED(device->CreateCommandList(0u, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator_, nullptr,
                                             __uuidof(ID3D12GraphicsCommandList), reinterpret_cast<void **>(&list_))))
            return false;
        list_->Close();
    }
    return fence_ != nullptr || SUCCEEDED(device->CreateFence(0u, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence),
                                                              reinterpret_cast<void **>(&fence_)));
}

bool D3d12FramePool::Frame::copy(ID3D12CommandQueue *queue, ID3D12Resource *source, const void *footprint,
                                 uint32_t width, uint32_t height, bool resolve)
{
    AV_SCOPE("D3d12FramePool::Frame::copy");
    if (FAILED(allocator_->Reset()) || FAILED(list_->Reset(allocator_, nullptr))) return false;
    const auto &placed = *static_cast<const D3D12_PLACED_SUBRESOURCE_FOOTPRINT *>(footprint);
    if (resolve) record_resolve(source);
    D3D12_TEXTURE_COPY_LOCATION to = {};
    to.pResource = buffer_;
    to.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    to.PlacedFootprint = placed;
    D3D12_TEXTURE_COPY_LOCATION from = {};
    from.pResource = resolve ? resolved_ : source;
    from.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    from.SubresourceIndex = 0u;
    list_->CopyTextureRegion(&to, 0u, 0u, 0u, &from, nullptr);
    if (resolve) {
        D3D12_RESOURCE_BARRIER back = {};
        back.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        back.Transition.pResource = resolved_;
        back.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        back.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        back.Transition.StateAfter = D3D12_RESOURCE_STATE_RESOLVE_DEST;
        list_->ResourceBarrier(1u, &back);
    }
    if (FAILED(list_->Close())) return false;
    ID3D12CommandList *const lists[1] = {list_};
    queue->ExecuteCommandLists(1u, lists);
    if (FAILED(queue->Signal(fence_, value_ + 1u))) return false;
    value_++;
    offset_ = placed.Offset;
    row_pitch_ = placed.Footprint.RowPitch;
    size(width, height);
    return true;
}

void D3d12FramePool::Frame::record_resolve(ID3D12Resource *source)
{
    AV_SCOPE("D3d12FramePool::Frame::record_resolve");
    D3D12_RESOURCE_BARRIER barriers[2] = {};
    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition.pResource = source;
    barriers[0].Transition.Subresource = 0u;
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
    list_->ResourceBarrier(1u, barriers);
    list_->ResolveSubresource(resolved_, 0u, source, 0u, static_cast<DXGI_FORMAT>(resolved_format_));
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[1].Transition.pResource = resolved_;
    barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_RESOLVE_DEST;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    list_->ResourceBarrier(2u, barriers);
}

void D3d12FramePool::Frame::drop()
{
    AV_SCOPE("D3d12FramePool::Frame::drop");
    if (resolved_ != nullptr) resolved_->Release();
    resolved_ = nullptr;
    resolved_format_ = 0u;
    if (list_ != nullptr) list_->Release();
    if (allocator_ != nullptr) allocator_->Release();
    if (fence_ != nullptr) fence_->Release();
    if (buffer_ != nullptr) buffer_->Release();
    buffer_ = nullptr;
    mapped_ = nullptr;
    bytes_ = 0u;
    allocator_ = nullptr;
    list_ = nullptr;
    fence_ = nullptr;
    value_ = 0u;
}

StagedFrame::Reading D3d12FramePool::Frame::map_when_copied(const uint8_t *&pixels, uint32_t &row_pitch)
{
    AV_SCOPE("D3d12FramePool::Frame::map_when_copied");
    if (mapped_ == nullptr) return Reading::failed;
    if (!idle()) return Reading::copying;
    pixels = mapped_ + offset_;
    row_pitch = row_pitch_;
    return Reading::mapped;
}

void D3d12FramePool::Frame::unmap()
{
    AV_SCOPE("D3d12FramePool::Frame::unmap");
}

}
