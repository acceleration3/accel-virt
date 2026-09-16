#include "device/context/context_binding.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"
#include "device/fence/fence_window.hpp"
#include "device/memory/blob_table.hpp"
#include "device/memory/blob_window.hpp"
#include "device/ring/ring_window.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/submission/fence_page.hpp"
#include "proto/submission/ring_header.hpp"

namespace av {

ContextBinding::ContextBinding(Host &host, BlobTable &blobs)
    : host_(host), blobs_(blobs), version_(0u, 0u), sub_api_(SubApi::vulkan), ring_resource_(0u), ring_order_(0u),
      fence_resource_(0u), doorbell_(0u), reply_resource_(0u), bound_(false)
      {
          AV_SCOPE("ContextBinding::ContextBinding");
      }

ContextBinding::~ContextBinding()
{
    AV_SCOPE("ContextBinding::~ContextBinding");
    unbind();
}

Status ContextBinding::bind(const ContextSetup &setup, const CapsetInfo &capset)
{
    AV_SCOPE("ContextBinding::bind");
    host_.require_main_loop("ContextBinding::bind");
    if (bound_) return Status::invalid_state;
    if (setup.validate_against(capset) != Status::ok) return Status::not_supported;
    const uint64_t ring_bytes = RingHeader::encoded_size + (uint64_t{1} << setup.ring_order());
    const Status ring = blobs_.pin(setup.ring_resource(), ring_bytes);
    if (ring != Status::ok) return ring;
    ScopeExit release_ring([this, &setup] { blobs_.unpin(setup.ring_resource()); });
    const Status fence = blobs_.pin(setup.fence_resource(), FencePage::encoded_size);
    if (fence != Status::ok) return fence;
    ScopeExit    release_fence([this, &setup] { blobs_.unpin(setup.fence_resource()); });
    const Status reply = setup.reply_resource() != 0u ? blobs_.pin(setup.reply_resource(), min_reply_bytes)
                                                      : Status::ok;
    if (reply != Status::ok) return reply;
    release_fence.dismiss();
    release_ring.dismiss();
    version_ = capset.version().negotiated_with(setup.version());
    sub_api_ = setup.sub_api();
    ring_resource_ = setup.ring_resource();
    ring_order_ = setup.ring_order();
    fence_resource_ = setup.fence_resource();
    doorbell_ = setup.doorbell();
    reply_resource_ = setup.reply_resource();
    bound_ = true;
    return Status::ok;
}

void ContextBinding::unbind()
{
    AV_SCOPE("ContextBinding::unbind");
    host_.require_main_loop("ContextBinding::unbind");
    if (!bound_) return;
    blobs_.unpin(ring_resource_);
    blobs_.unpin(fence_resource_);
    if (reply_resource_ != 0u) blobs_.unpin(reply_resource_);
    bound_ = false;
}

bool ContextBinding::bound() const
{
    AV_SCOPE("ContextBinding::bound");
    return bound_;
}

const ProtocolVersion &ContextBinding::version() const
{
    AV_SCOPE("ContextBinding::version");
    return version_;
}

SubApi ContextBinding::sub_api() const
{
    AV_SCOPE("ContextBinding::sub_api");
    return sub_api_;
}

uint32_t ContextBinding::ring_resource() const
{
    AV_SCOPE("ContextBinding::ring_resource");
    return ring_resource_;
}

uint8_t ContextBinding::ring_order() const
{
    AV_SCOPE("ContextBinding::ring_order");
    return ring_order_;
}

uint32_t ContextBinding::fence_resource() const
{
    AV_SCOPE("ContextBinding::fence_resource");
    return fence_resource_;
}

uint32_t ContextBinding::doorbell() const
{
    AV_SCOPE("ContextBinding::doorbell");
    return doorbell_;
}

/* A bound ring blob is pinned, so the table always has its window; unbound, the
 * empty window is what prepare refuses. */
RingWindow ContextBinding::ring_window()
{
    AV_SCOPE("ContextBinding::ring_window");
    host_.require_main_loop("ContextBinding::ring_window");
    BlobWindow blob;
    if (bound_) static_cast<void>(blobs_.window(ring_resource_, blob));
    return RingWindow(blob, bound_ ? ring_order_ : uint8_t{0});
}

uint32_t ContextBinding::reply_resource() const
{
    AV_SCOPE("ContextBinding::reply_resource");
    return reply_resource_;
}

BlobWindow ContextBinding::reply_window()
{
    AV_SCOPE("ContextBinding::reply_window");
    host_.require_main_loop("ContextBinding::reply_window");
    BlobWindow blob;
    if (bound_ && reply_resource_ != 0u) static_cast<void>(blobs_.window(reply_resource_, blob));
    return blob;
}

FenceWindow ContextBinding::fence_window()
{
    AV_SCOPE("ContextBinding::fence_window");
    host_.require_main_loop("ContextBinding::fence_window");
    BlobWindow blob;
    if (bound_) static_cast<void>(blobs_.window(fence_resource_, blob));
    return FenceWindow(blob);
}

}  // namespace av
