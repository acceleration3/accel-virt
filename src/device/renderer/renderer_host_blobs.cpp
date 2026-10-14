#include "device/renderer/renderer_host_blobs.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"

namespace av {

RendererHostBlobs::RendererHostBlobs(Host &host) : host_(host), lock_(nullptr), owners_{}
{
    AV_SCOPE("RendererHostBlobs::RendererHostBlobs");
}

RendererHostBlobs::~RendererHostBlobs()
{
    AV_SCOPE("RendererHostBlobs::~RendererHostBlobs");
    if (lock_ != nullptr) host_.destroy_mutex(lock_);
}

Status RendererHostBlobs::initialize()
{
    AV_SCOPE("RendererHostBlobs::initialize");
    if (lock_ != nullptr) return Status::invalid_state;
    return host_.create_mutex(lock_) == Status::ok ? Status::ok : Status::no_memory;
}

Status RendererHostBlobs::adopt(uint32_t id, uint32_t context)
{
    AV_SCOPE("RendererHostBlobs::adopt");
    host_.require_main_loop("RendererHostBlobs::adopt");
    const HostBlobId named(id);
    if (!named.valid() || context == 0u || lock_ == nullptr) return Status::invalid_parameter;
    host_.lock_mutex(lock_);
    owners_[named.slot()] = context;
    host_.unlock_mutex(lock_);
    return Status::ok;
}

void RendererHostBlobs::forget(uint32_t id)
{
    AV_SCOPE("RendererHostBlobs::forget");
    host_.require_main_loop("RendererHostBlobs::forget");
    const HostBlobId named(id);
    if (!named.valid() || lock_ == nullptr) return;
    host_.lock_mutex(lock_);
    owners_[named.slot()] = 0u;
    host_.unlock_mutex(lock_);
}

void RendererHostBlobs::forget_all()
{
    AV_SCOPE("RendererHostBlobs::forget_all");
    host_.require_main_loop("RendererHostBlobs::forget_all");
    if (lock_ == nullptr) return;
    host_.lock_mutex(lock_);
    for (uint32_t &owner : owners_) owner = 0u;
    host_.unlock_mutex(lock_);
}

bool RendererHostBlobs::owns(uint32_t context, uint32_t id)
{
    AV_SCOPE("RendererHostBlobs::owns");
    const HostBlobId named(id);
    if (!named.valid() || context == 0u || lock_ == nullptr) return false;
    host_.lock_mutex(lock_);
    ScopeExit unlock([this] { host_.unlock_mutex(lock_); });
    return owners_[named.slot()] == context;
}

uint32_t RendererHostBlobs::live() const
{
    AV_SCOPE("RendererHostBlobs::live");
    uint32_t count = 0u;
    for (const uint32_t owner : owners_) count += owner != 0u ? 1u : 0u;
    return count;
}

}  // namespace av
