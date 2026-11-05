#ifndef AV_CONTEXT_SETUP_HPP
#define AV_CONTEXT_SETUP_HPP

#include "core/support/status.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class CapsetInfo;

class ContextSetup {
public:
    static constexpr size_t   encoded_size = 48u;

    ContextSetup();
    ContextSetup(const ProtocolVersion &version, SubApi sub_api, uint32_t ring_resource,
                 uint8_t ring_order, uint32_t fence_resource, uint32_t doorbell, uint32_t reply_resource,
                 const GlContextOptions &options = GlContextOptions());

    const ProtocolVersion &version() const;
    SubApi                 sub_api() const;
    uint32_t               ring_resource() const;
    uint8_t                ring_order() const;
    uint32_t               fence_resource() const;
    uint32_t               doorbell() const;
    uint32_t               reply_resource() const;
    GlContextOptions       options() const;
    bool                   options_fit() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);
    [[nodiscard]] Status validate_against(const CapsetInfo &capset) const;

private:
    static constexpr uint32_t magic         = 0x00435641u;
    static constexpr size_t   fixed_size    = 32u;
    static constexpr uint8_t  lowest_order  = 12u;
    static constexpr uint8_t  highest_order = 24u;

    bool fields_valid() const;

    ProtocolVersion version_;
    uint32_t        sub_api_;
    uint32_t        ring_resource_;
    uint8_t         ring_order_;
    uint32_t        fence_resource_;
    uint32_t        doorbell_;
    uint32_t        reply_resource_;
    GlContextOptions options_;
};

}

#endif
