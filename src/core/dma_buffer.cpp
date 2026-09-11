#include "dma_buffer.hpp"

namespace av {

DmaBuffer::DmaBuffer(Platform &platform)
    : platform_(platform), bytes_(nullptr), physical_address_(0u), size_(0u) {}

DmaBuffer::~DmaBuffer()
{
    if (bytes_ != nullptr) platform_.release_contiguous(bytes_, size_);
}

Status DmaBuffer::initialize(size_t bytes)
{
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

uint8_t *DmaBuffer::bytes() const            { return bytes_; }
uint64_t DmaBuffer::physical_address() const { return physical_address_; }
size_t   DmaBuffer::size() const             { return size_; }

bool DmaBuffer::invariants_hold() const
{
    return (bytes_ == nullptr) == (size_ == 0u) && (bytes_ != nullptr || physical_address_ == 0u);
}

}  // namespace av
