/* Binds a new context to its ring, fence page, doorbell and reply slot
 * (docs/protocol.md). From protocol minor 1 it is 48 bytes and carries an
 * OpenGL context's options, its share context named by the device context id
 * (feature 050); a minor-0 setup is 32 bytes and means the defaults. */
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
    static constexpr size_t   base_size = 32u;
    /* The largest setup: the options' eight bytes and eight reserved. */
    static constexpr size_t   encoded_size = 48u;
    static constexpr uint16_t options_minor = 1u;

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
    /* 0 when the context sends no command that generates a reply. */
    uint32_t               reply_resource() const;
    /* The share context in it is the device's id for that context. */
    GlContextOptions       options() const;
    /* The bytes this setup is at its version: base_size or encoded_size. */
    size_t                 size() const;
    /* Its options are ones its version and sub-API can carry. */
    bool                   options_fit() const;

    /* too_large for less room than size(); invalid_parameter for options the
     * version or sub-API cannot carry. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* Checks what the bytes alone can prove, `length` being exactly the size
     * its version gives; unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);
    /* Checks what needs the device's capset: major, sub-API,
     * ring order, and a doorbell index below its context count. */
    [[nodiscard]] Status validate_against(const CapsetInfo &capset) const;

private:
    static constexpr uint32_t magic         = 0x434B5652u;  /* "RVKC" */
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

}  // namespace av

#endif  // AV_CONTEXT_SETUP_HPP
