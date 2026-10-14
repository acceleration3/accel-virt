/* Renderer -> QEMU, the answer to hello once the executor library is loaded:
 * the protocol and executor ABI the renderer speaks and the sub-APIs it can
 * run on the GPU hello named, which the capset needs and QEMU no longer loads
 * the library to know. */
#ifndef AV_READY_MESSAGE_HPP
#define AV_READY_MESSAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ReadyMessage {
public:
    /* Sub-API bits a renderer may offer: one per SubApi value. */
    static constexpr uint32_t lanes = 8u;
    static constexpr size_t   encoded_size = 12u;
    static constexpr uint32_t descriptors = 0u;

    ReadyMessage();
    ReadyMessage(uint32_t version, uint32_t executor_abi, uint32_t sub_apis);

    uint32_t version() const;
    uint32_t executor_abi() const;
    uint32_t sub_apis() const;

    /* too_large for less room than encoded_size; invalid_parameter for a
     * message decode would refuse. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for any other length, any descriptor, or a sub-API bit past
     * the last lane. Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count);

private:
    bool fields_valid() const;

    uint32_t version_;
    uint32_t executor_abi_;
    uint32_t sub_apis_;
};

}  // namespace av

#endif  // AV_READY_MESSAGE_HPP
