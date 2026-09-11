/* What one context's setup bound it to (docs/protocol.md, Context setup): the
 * negotiated protocol version, its sub-API, its ring and fence resources, and
 * the ring's order. The bound blobs stay pinned until the binding lets go. */
#ifndef AV_CONTEXT_BINDING_HPP
#define AV_CONTEXT_BINDING_HPP

#include "core/support/status.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"

#include <stdint.h>

namespace av {

class BlobTable;
class CapsetInfo;
class ContextSetup;
class Host;

class ContextBinding {
public:
    ContextBinding(Host &host, BlobTable &blobs);
    ~ContextBinding();
    ContextBinding(const ContextBinding &) = delete;
    ContextBinding &operator=(const ContextBinding &) = delete;

    /* Main loop. invalid_state once bound; not_supported for a setup the
     * capset does not allow; not_found for a missing resource; invalid_parameter
     * for a ring or fence blob too small for its role. Nothing is pinned
     * unless ok. */
    [[nodiscard]] Status bind(const ContextSetup &setup, const CapsetInfo &capset);
    /* Main loop. */
    void                 unbind();

    bool                   bound() const;
    const ProtocolVersion &version() const;
    SubApi                 sub_api() const;
    uint32_t               ring_resource() const;
    uint8_t                ring_order() const;
    uint32_t               fence_resource() const;

private:
    Host           &host_;
    BlobTable      &blobs_;
    ProtocolVersion version_;
    SubApi          sub_api_;
    uint32_t        ring_resource_;
    uint8_t         ring_order_;
    uint32_t        fence_resource_;
    bool            bound_;
};

}  // namespace av

#endif  // AV_CONTEXT_BINDING_HPP
