#include "device/memory/host_memory_block.hpp"

#include "device/host/host.hpp"
#include "device/memory/blob_window.hpp"

namespace av {

HostMemoryBlock::HostMemoryBlock(Host &host) : host_(host), handle_(nullptr), view_(nullptr), size_(0u) {}

HostMemoryBlock::~HostMemoryBlock() { release(); }

Status HostMemoryBlock::create(uint64_t size)
{
    host_.require_main_loop("HostMemoryBlock::create");
    if (handle_ != nullptr) return Status::invalid_state;
    if (size == 0u || size % page_size != 0u) return Status::invalid_parameter;
    handle_ = host_.create_host_memory(size, view_);
    if (handle_ == nullptr) return Status::no_memory;
    size_ = size;
    return Status::ok;
}

void HostMemoryBlock::release()
{
    host_.require_main_loop("HostMemoryBlock::release");
    if (handle_ == nullptr) return;
    host_.destroy_host_memory(handle_);
    handle_ = nullptr;
    view_ = nullptr;
    size_ = 0u;
}

bool HostMemoryBlock::created() const
{
    return handle_ != nullptr;
}

uint64_t HostMemoryBlock::size() const
{
    return size_;
}

BlobWindow HostMemoryBlock::window() const
{
    return handle_ != nullptr ? BlobWindow(&view_, &size_, 1u, size_) : BlobWindow();
}

void *HostMemoryBlock::handle() const
{
    return handle_;
}

}  // namespace av
