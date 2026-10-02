#include "miniport/virtio/dma_buffer.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

DmaBuffer::DmaBuffer(Platform &platform)
    : platform_(platform), bytes_(nullptr), physical_address_(0u), size_(0u)
    {
        AV_SCOPE("DmaBuffer::DmaBuffer");
    }

DmaBuffer::~DmaBuffer()
{
    AV_SCOPE("DmaBuffer::~DmaBuffer");
    if (bytes_ != nullptr) platform_.release_contiguous(bytes_, size_);
}

Status DmaBuffer::initialize(size_t bytes)
{
    AV_SCOPE("DmaBuffer::initialize");
    platform_.require_irql_at_most(Irql::passive, "DmaBuffer::initialize");
    if (bytes == 0u) return Status::invalid_parameter;
    if (bytes_ != nullptr) return Status::invalid_state;
    auto *block = static_cast<uint8_t *>(platform_.allocate_contiguous(bytes));
    if (block == nullptr) return Status::no_memory;
    platform_.zero(block, bytes);
    bytes_ = block;
    physical_address_ = platform_.physical_address(block);
    size_ = bytes;
    return Status::ok;
}

uint8_t *DmaBuffer::bytes() const
{
    AV_SCOPE("DmaBuffer::bytes");
    return bytes_;
}
uint64_t DmaBuffer::physical_address() const
{
    AV_SCOPE("DmaBuffer::physical_address");
    return physical_address_;
}
size_t   DmaBuffer::size() const
{
    AV_SCOPE("DmaBuffer::size");
    return size_;
}

bool DmaBuffer::invariants_hold() const
{
    AV_SCOPE("DmaBuffer::invariants_hold");
    return (bytes_ == nullptr) == (size_ == 0u) && (bytes_ != nullptr || physical_address_ == 0u);
}

}  // namespace av
