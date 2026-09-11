/* Binds a new context to its ring and fence page (docs/protocol.md). */
#ifndef AV_CONTEXT_SETUP_HPP
#define AV_CONTEXT_SETUP_HPP

#include "../core/status.hpp"
#include "protocol_version.hpp"
#include "sub_api.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class CapsetInfo;

class ContextSetup {
public:
    static constexpr size_t encoded_size = 32u;

    ContextSetup();
    ContextSetup(const ProtocolVersion &version, SubApi sub_api, uint32_t ring_resource,
                 uint8_t ring_order, uint32_t fence_resource);

    const ProtocolVersion &version() const;
    SubApi                 sub_api() const;
    uint32_t               ring_resource() const;
    uint8_t                ring_order() const;
    uint32_t               fence_resource() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* Checks what the bytes alone can prove; unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);
    /* Checks what needs the device's capset: major, sub-API, ring order. */
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
};

}  // namespace av

#endif  // AV_CONTEXT_SETUP_HPP
