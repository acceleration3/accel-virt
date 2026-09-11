/* What one context's setup bound it to (docs/protocol.md, Context setup): the
 * negotiated protocol version, its sub-API, its ring and fence resources, the
 * ring's order and its doorbell. The bound blobs stay pinned until the binding
 * lets go. */
#ifndef AV_CONTEXT_BINDING_HPP
#define AV_CONTEXT_BINDING_HPP

#include "core/support/status.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"

#include <stdint.h>

namespace av {

class BlobTable;
class BlobWindow;
class FenceWindow;
class RingWindow;
class CapsetInfo;
class ContextSetup;
class Host;

class ContextBinding {
public:
    /* A reply header and at least as many bytes of answer. */
    static constexpr uint64_t min_reply_bytes = 64u;

    ContextBinding(Host &host, BlobTable &blobs);
    ~ContextBinding();
    ContextBinding(const ContextBinding &) = delete;
    ContextBinding &operator=(const ContextBinding &) = delete;

    /* Main loop. invalid_state once bound; not_supported for a setup the
     * capset does not allow; not_found for a missing resource; invalid_parameter
     * for a ring, fence or reply blob too small for its role. Nothing is
     * pinned unless ok. */
    [[nodiscard]] Status bind(const ContextSetup &setup, const CapsetInfo &capset);
    /* Main loop. */
    void                 unbind();

    bool                   bound() const;
    const ProtocolVersion &version() const;
    SubApi                 sub_api() const;
    uint32_t               ring_resource() const;
    uint8_t                ring_order() const;
    uint32_t               fence_resource() const;
    uint32_t               doorbell() const;
    /* Main loop. The bound ring blob as a ring; a window over no ring unbound. */
    RingWindow             ring_window();
    /* Main loop. The bound fence blob; a window over nothing unbound. */
    FenceWindow            fence_window();
    /* 0 when the context has no reply slot. */
    uint32_t               reply_resource() const;
    /* Main loop. The bound reply blob; an empty window without one. */
    BlobWindow             reply_window();

private:
    Host           &host_;
    BlobTable      &blobs_;
    ProtocolVersion version_;
    SubApi          sub_api_;
    uint32_t        ring_resource_;
    uint8_t         ring_order_;
    uint32_t        fence_resource_;
    uint32_t        doorbell_;
    uint32_t        reply_resource_;
    bool            bound_;
};

}  // namespace av

#endif  // AV_CONTEXT_BINDING_HPP
