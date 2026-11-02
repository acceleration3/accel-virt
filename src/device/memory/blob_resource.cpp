#include "device/memory/blob_resource.hpp"

#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"
#include "device/memory/blob_window.hpp"

namespace av {

BlobResource::BlobResource(Host &host, uint32_t id)
    : host_(host), id_(id), guest_(host), pins_(0u), alias_(host)
{
    AV_SCOPE("BlobResource::BlobResource");
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

uint32_t BlobResource::id() const
{
    AV_SCOPE("BlobResource::id");
    return id_;
}

uint64_t BlobResource::size() const
{
    AV_SCOPE("BlobResource::size");
    return guest_.length();
}

void BlobResource::pin()
{
    AV_SCOPE("BlobResource::pin");
    pins_++;
}

void BlobResource::unpin()
{
    AV_SCOPE("BlobResource::unpin");
    if (pins_ != 0u) pins_--;
}

bool BlobResource::pinned() const
{
    AV_SCOPE("BlobResource::pinned");
    return pins_ != 0u;
}

BlobWindow BlobResource::window() const
{
    AV_SCOPE("BlobResource::window");
    return guest_.window();
}

Status BlobResource::view(const uint8_t *&base)
{
    AV_SCOPE("BlobResource::view");
    host_.require_main_loop("BlobResource::view");
    base = nullptr;
    const Status status = alias_.mapped() ? Status::ok : alias_.map(guest_.window());
    if (status == Status::ok) base = static_cast<const uint8_t *>(alias_.base());
    return status;
}

}
