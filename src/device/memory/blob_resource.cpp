#include "device/memory/blob_resource.hpp"

#include "device/host/host.hpp"
#include "device/memory/blob_window.hpp"

namespace av {

BlobResource::BlobResource(Host &host, uint32_t id)
    : host_(host), id_(id), guest_(host), block_(host), offset_(0u), mapped_(false), pins_(0u), alias_(host),
      alias_holds_(0u) {}

/* A mapping is removed before its block is destroyed. */
BlobResource::~BlobResource()
{
    if (mapped_) host_.remove_from_hostmem(offset_, block_.size());
}

Status BlobResource::back_with_guest(ControlRequest &request, size_t offset, uint32_t entries, uint64_t size)
{
    host_.require_main_loop("BlobResource::back_with_guest");
    const Status status = guest_.map(request, offset, entries);
    if (status != Status::ok || guest_.length() >= size) return status;
    guest_.release();
    return Status::invalid_parameter;
}

Status BlobResource::back_with_host(uint64_t size)
{
    host_.require_main_loop("BlobResource::back_with_host");
    return block_.create(size);
}

Status BlobResource::map(uint64_t offset)
{
    host_.require_main_loop("BlobResource::map");
    if (!block_.created() || mapped_) return Status::invalid_parameter;
    const Status status = host_.place_in_hostmem(block_.handle(), offset);
    mapped_ = status == Status::ok;
    offset_ = offset;
    return status;
}

Status BlobResource::unmap()
{
    host_.require_main_loop("BlobResource::unmap");
    if (!mapped_) return Status::invalid_state;
    host_.remove_from_hostmem(offset_, block_.size());
    mapped_ = false;
    return Status::ok;
}

uint32_t BlobResource::id() const
{
    return id_;
}

uint64_t BlobResource::size() const
{
    return block_.created() ? block_.size() : guest_.length();
}

bool BlobResource::mapped() const
{
    return mapped_;
}

uint64_t BlobResource::offset() const
{
    return offset_;
}

bool BlobResource::overlaps(uint64_t offset, uint64_t size) const
{
    return mapped_ && offset < offset_ + block_.size() && offset_ < offset + size;
}

void BlobResource::pin()
{
    pins_++;
}

void BlobResource::unpin()
{
    pins_--;
}

bool BlobResource::pinned() const
{
    return pins_ != 0u;
}

BlobWindow BlobResource::window() const
{
    return block_.created() ? block_.window() : guest_.window();
}

Status BlobResource::hold_alias(const GuestPageAlias *&alias)
{
    alias = nullptr;
    if (block_.created()) return Status::invalid_parameter;
    const Status status = alias_.mapped() ? Status::ok : alias_.map(guest_.window());
    if (status != Status::ok) return status;
    alias_holds_++;
    alias = &alias_;
    return Status::ok;
}

void BlobResource::release_alias()
{
    if (alias_holds_ == 0u) return;
    alias_holds_--;
    if (alias_holds_ == 0u) alias_.release();
}

bool BlobResource::aliased() const
{
    return alias_holds_ != 0u;
}

}  // namespace av
