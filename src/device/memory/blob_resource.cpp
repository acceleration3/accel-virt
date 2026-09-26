#include "device/memory/blob_resource.hpp"

#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"
#include "device/memory/blob_window.hpp"
#include "device/memory/held_blob.hpp"

namespace av {

BlobResource::BlobResource(Host &host, uint32_t id)
    : host_(host), id_(id), guest_(host), block_(host), offset_(0u), mapped_(false), pins_(0u), alias_(host),
      holds_(0u)
      {
          AV_SCOPE("BlobResource::BlobResource");
      }

/* A mapping is removed before its block is destroyed. */
BlobResource::~BlobResource()
{
    AV_SCOPE("BlobResource::~BlobResource");
    if (mapped_) host_.remove_from_hostmem(offset_, block_.size());
}

Status BlobResource::back_with_guest(ControlRequest &request, size_t offset, uint32_t entries, uint64_t size)
{
    AV_SCOPE("BlobResource::back_with_guest");
    host_.require_main_loop("BlobResource::back_with_guest");
    const Status status = guest_.map(request, offset, entries);
    if (status != Status::ok || guest_.length() >= size) return status;
    guest_.release();
    return Status::invalid_parameter;
}

Status BlobResource::back_with_host(uint64_t size)
{
    AV_SCOPE("BlobResource::back_with_host");
    host_.require_main_loop("BlobResource::back_with_host");
    return block_.create(size);
}

Status BlobResource::map(uint64_t offset)
{
    AV_SCOPE("BlobResource::map");
    host_.require_main_loop("BlobResource::map");
    if (!block_.created() || mapped_) return Status::invalid_parameter;
    const Status status = host_.place_in_hostmem(block_.handle(), offset);
    mapped_ = status == Status::ok;
    offset_ = offset;
    return status;
}

Status BlobResource::unmap()
{
    AV_SCOPE("BlobResource::unmap");
    host_.require_main_loop("BlobResource::unmap");
    if (!mapped_) return Status::invalid_state;
    host_.remove_from_hostmem(offset_, block_.size());
    mapped_ = false;
    return Status::ok;
}

uint32_t BlobResource::id() const
{
    AV_SCOPE("BlobResource::id");
    return id_;
}

uint64_t BlobResource::size() const
{
    AV_SCOPE("BlobResource::size");
    return block_.created() ? block_.size() : guest_.length();
}

bool BlobResource::mapped() const
{
    AV_SCOPE("BlobResource::mapped");
    return mapped_;
}

uint64_t BlobResource::offset() const
{
    AV_SCOPE("BlobResource::offset");
    return offset_;
}

bool BlobResource::overlaps(uint64_t offset, uint64_t size) const
{
    AV_SCOPE("BlobResource::overlaps");
    return mapped_ && offset < offset_ + block_.size() && offset_ < offset + size;
}

void BlobResource::pin()
{
    AV_SCOPE("BlobResource::pin");
    pins_++;
}

void BlobResource::unpin()
{
    AV_SCOPE("BlobResource::unpin");
    pins_--;
}

bool BlobResource::pinned() const
{
    AV_SCOPE("BlobResource::pinned");
    return pins_ != 0u;
}

void *BlobResource::host_memory() const
{
    AV_SCOPE("BlobResource::host_memory");
    return block_.created() ? block_.handle() : nullptr;
}

BlobWindow BlobResource::window() const
{
    AV_SCOPE("BlobResource::window");
    return block_.created() ? block_.window() : guest_.window();
}

Status BlobResource::hold(HeldBlob &held)
{
    AV_SCOPE("BlobResource::hold");
    held = HeldBlob();
    if (block_.created()) {
        const BlobWindow view = block_.window();
        uint64_t         length = 0u;
        void *const      base = view.extent(0u, length);
        if (base == nullptr) return Status::no_memory;
        holds_++;
        held = HeldBlob(base, length, view, block_.handle());
        return Status::ok;
    }
    const Status status = alias_.mapped() ? Status::ok : alias_.map(guest_.window());
    if (status != Status::ok) return status;
    holds_++;
    held = HeldBlob(alias_.base(), alias_.length(), guest_.window(), nullptr);
    return Status::ok;
}

void BlobResource::release_hold()
{
    AV_SCOPE("BlobResource::release_hold");
    if (holds_ == 0u) return;
    holds_--;
    if (holds_ == 0u) alias_.release();
}

bool BlobResource::held() const
{
    AV_SCOPE("BlobResource::held");
    return holds_ != 0u;
}

}  // namespace av
