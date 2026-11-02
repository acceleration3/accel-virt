#include "device/memory/host_blob.hpp"

#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"

namespace av {

HostBlob::HostBlob(Host &host, uint32_t context, const AvHostMemory &memory)
    : host_(host), size_(memory.size), offset_(0u), context_(context), fd_(memory.fd), caching_(memory.caching),
      mapped_(false), waiting_(false), lender_(false), request_(0u), laid_(0u), lending_(nullptr)
{
    AV_SCOPE("HostBlob::HostBlob");
}

HostBlob::HostBlob(Host &host, uint32_t context, uint64_t size, HostMemoryRequest request)
    : host_(host), size_(size), offset_(0u), context_(context), fd_(-1), caching_(0u), mapped_(false),
      waiting_(true), lender_(false), request_(request.encoded()), laid_(0u), lending_(nullptr)
{
    AV_SCOPE("HostBlob::HostBlob");
}

HostBlob::~HostBlob()
{
    AV_SCOPE("HostBlob::~HostBlob");
    if (mapped_) host_.remove_from_hostmem(offset_, size_);
    if (fd_ >= 0) host_.close_descriptor(fd_);
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

bool HostBlob::waiting() const
{
    AV_SCOPE("HostBlob::waiting");
    return waiting_;
}

HostMemoryRequest HostBlob::request() const
{
    AV_SCOPE("HostBlob::request");
    return HostMemoryRequest(request_);
}

void HostBlob::lend(GuestRamLending &lending, bool made_here, const AvHostMemory &memory)
{
    AV_SCOPE("HostBlob::lend");
    lending_ = &lending;
    lender_ = made_here;
    waiting_ = false;
    caching_ = memory.caching;
    fd_ = -1;
}

GuestRamLending *HostBlob::lending() const
{
    AV_SCOPE("HostBlob::lending");
    return lending_;
}

bool HostBlob::lender() const
{
    AV_SCOPE("HostBlob::lender");
    return lender_;
}

uint32_t HostBlob::entries_laid() const
{
    AV_SCOPE("HostBlob::entries_laid");
    return laid_;
}

void HostBlob::laid(uint32_t entries)
{
    AV_SCOPE("HostBlob::laid");
    laid_ += entries;
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

}
