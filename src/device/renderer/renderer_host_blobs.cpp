#include "device/renderer/renderer_host_blobs.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"
#include "proto/resources/host_blob_id.hpp"

namespace av {

RendererHostBlobs::RendererHostBlobs(Host &host) : host_(host), lock_(nullptr), owners_(host, Pool::paged)
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
    ScopeExit       unlock([this] { host_.unlock_mutex(lock_); });
    uint32_t *const owner = owners_.find(id);
    if (owner != nullptr) {
        *owner = context;
        return Status::ok;
    }
    uint32_t slot = 0u;
    return owners_.insert(id, context, slot) == Status::ok ? Status::ok : Status::no_memory;
}

void RendererHostBlobs::forget(uint32_t id)
{
    AV_SCOPE("RendererHostBlobs::forget");
    host_.require_main_loop("RendererHostBlobs::forget");
    const HostBlobId named(id);
    if (!named.valid() || lock_ == nullptr) return;
    host_.lock_mutex(lock_);
    static_cast<void>(owners_.erase(id));
    host_.unlock_mutex(lock_);
}

void RendererHostBlobs::forget_all()
{
    AV_SCOPE("RendererHostBlobs::forget_all");
    host_.require_main_loop("RendererHostBlobs::forget_all");
    if (lock_ == nullptr) return;
    host_.lock_mutex(lock_);
    owners_.clear();
    host_.unlock_mutex(lock_);
}

bool RendererHostBlobs::owns(uint32_t context, uint32_t id)
{
    AV_SCOPE("RendererHostBlobs::owns");
    const HostBlobId named(id);
    if (!named.valid() || context == 0u || lock_ == nullptr) return false;
    host_.lock_mutex(lock_);
    ScopeExit unlock([this] { host_.unlock_mutex(lock_); });
    const uint32_t *const owner = owners_.find(id);
    return owner != nullptr && *owner == context;
}

uint32_t RendererHostBlobs::live() const
{
    AV_SCOPE("RendererHostBlobs::live");
    return owners_.count();
}

}  // namespace av
