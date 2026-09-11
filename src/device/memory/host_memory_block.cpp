#include "device/memory/host_memory_block.hpp"

#include "device/host/host.hpp"

namespace av {

HostMemoryBlock::HostMemoryBlock(Host &host) : host_(host), handle_(nullptr), size_(0u) {}

HostMemoryBlock::~HostMemoryBlock() { release(); }

Status HostMemoryBlock::create(uint64_t size)
{
    host_.require_main_loop("HostMemoryBlock::create");
    if (handle_ != nullptr) return Status::invalid_state;
    if (size == 0u || size % page_size != 0u) return Status::invalid_parameter;
    void *device_view = nullptr;
    handle_ = host_.create_host_memory(size, device_view);
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

void *HostMemoryBlock::handle() const
{
    return handle_;
}

}  // namespace av
