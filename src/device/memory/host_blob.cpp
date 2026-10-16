#include "device/memory/host_blob.hpp"

#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"

namespace av {

HostBlob::HostBlob(Host &host, LentPages &lent, uint32_t context, const AvHostMemory &memory)
    : host_(host), size_(memory.size), offset_(0u), context_(context), fd_(memory.fd), caching_(memory.caching),
      mapped_(false), overlay_(host, lent)
{
    AV_SCOPE("HostBlob::HostBlob");
}

HostBlob::~HostBlob()
{
    AV_SCOPE("HostBlob::~HostBlob");
    overlay_.restore();
    if (mapped_) host_.remove_from_hostmem(offset_, size_);
    host_.close_descriptor(fd_);
}

uint32_t HostBlob::context() const
{
    AV_SCOPE("HostBlob::context");
    return context_;
}

uint64_t HostBlob::size() const
{
    AV_SCOPE("HostBlob::size");
    return size_;
}

uint32_t HostBlob::caching() const
{
    AV_SCOPE("HostBlob::caching");
    return caching_;
}

Status HostBlob::adopt(ControlRequest &request, size_t offset, uint32_t entries)
{
    AV_SCOPE("HostBlob::adopt");
    return overlay_.add(request, offset, entries, fd_, size_);
}

uint64_t HostBlob::adopted() const
{
    AV_SCOPE("HostBlob::adopted");
    return overlay_.covered();
}

Status HostBlob::map(uint64_t offset)
{
    AV_SCOPE("HostBlob::map");
    host_.require_main_loop("HostBlob::map");
    if (mapped_) return Status::invalid_state;
    if (host_.place_in_hostmem(fd_, size_, offset) != Status::ok) return Status::no_memory;
    offset_ = offset;
    mapped_ = true;
    return Status::ok;
}

Status HostBlob::unmap()
{
    AV_SCOPE("HostBlob::unmap");
    host_.require_main_loop("HostBlob::unmap");
    if (!mapped_) return Status::invalid_state;
    host_.remove_from_hostmem(offset_, size_);
    mapped_ = false;
    offset_ = 0u;
    return Status::ok;
}

bool HostBlob::mapped() const
{
    AV_SCOPE("HostBlob::mapped");
    return mapped_;
}

uint64_t HostBlob::offset() const
{
    AV_SCOPE("HostBlob::offset");
    return offset_;
}

bool HostBlob::overlaps(uint64_t offset, uint64_t size) const
{
    AV_SCOPE("HostBlob::overlaps");
    return mapped_ && offset < offset_ + size_ && offset_ < offset + size;
}

}  // namespace av
