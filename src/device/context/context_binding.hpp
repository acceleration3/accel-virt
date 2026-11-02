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
    static constexpr uint64_t min_reply_bytes = 64u;

    ContextBinding(Host &host, BlobTable &blobs);
    ~ContextBinding();
    ContextBinding(const ContextBinding &) = delete;
    ContextBinding &operator=(const ContextBinding &) = delete;

    [[nodiscard]] Status bind(const ContextSetup &setup, const CapsetInfo &capset);
    void                 unbind();

    bool                   bound() const;
    const ProtocolVersion &version() const;
    SubApi                 sub_api() const;
    uint32_t               ring_resource() const;
    uint8_t                ring_order() const;
    uint32_t               fence_resource() const;
    uint32_t               doorbell() const;
    RingWindow             ring_window();
    FenceWindow            fence_window();
    uint32_t               reply_resource() const;
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

}

#endif
