#include "device/context/context_binding.hpp"

#include "core/support/scope_exit.hpp"
#include "device/host/host.hpp"
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
      fence_resource_(0u), doorbell_(0u), bound_(false) {}

ContextBinding::~ContextBinding()
{
    unbind();
}

Status ContextBinding::bind(const ContextSetup &setup, const CapsetInfo &capset)
{
    host_.require_main_loop("ContextBinding::bind");
    if (bound_) return Status::invalid_state;
    if (setup.validate_against(capset) != Status::ok) return Status::not_supported;
    const uint64_t ring_bytes = RingHeader::encoded_size + (uint64_t{1} << setup.ring_order());
    const Status ring = blobs_.pin(setup.ring_resource(), ring_bytes);
    if (ring != Status::ok) return ring;
    ScopeExit release_ring([this, &setup] { blobs_.unpin(setup.ring_resource()); });
    const Status fence = blobs_.pin(setup.fence_resource(), FencePage::encoded_size);
    if (fence != Status::ok) return fence;
    release_ring.dismiss();
    version_ = capset.version().negotiated_with(setup.version());
    sub_api_ = setup.sub_api();
    ring_resource_ = setup.ring_resource();
    ring_order_ = setup.ring_order();
    fence_resource_ = setup.fence_resource();
    doorbell_ = setup.doorbell();
    bound_ = true;
    return Status::ok;
}

void ContextBinding::unbind()
{
    host_.require_main_loop("ContextBinding::unbind");
    if (!bound_) return;
    blobs_.unpin(ring_resource_);
    blobs_.unpin(fence_resource_);
    bound_ = false;
}

bool ContextBinding::bound() const
{
    return bound_;
}

const ProtocolVersion &ContextBinding::version() const
{
    return version_;
}

SubApi ContextBinding::sub_api() const
{
    return sub_api_;
}

uint32_t ContextBinding::ring_resource() const
{
    return ring_resource_;
}

uint8_t ContextBinding::ring_order() const
{
    return ring_order_;
}

uint32_t ContextBinding::fence_resource() const
{
    return fence_resource_;
}

uint32_t ContextBinding::doorbell() const
{
    return doorbell_;
}

/* A bound ring blob is pinned, so the table always has its window; unbound, the
 * empty window is what prepare refuses. */
RingWindow ContextBinding::ring_window()
{
    host_.require_main_loop("ContextBinding::ring_window");
    BlobWindow blob;
    if (bound_) static_cast<void>(blobs_.window(ring_resource_, blob));
    return RingWindow(blob, bound_ ? ring_order_ : uint8_t{0});
}

}  // namespace av
