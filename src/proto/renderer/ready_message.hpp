/* Renderer -> QEMU, the answer to hello once the executor library is loaded:
 * the protocol and executor ABI the renderer speaks, the sub-APIs it can run
 * on the GPU hello named, and for each the blob import kinds it offers. The
 * capset needs both, and QEMU no longer loads the library that knows them.
 * Lane i describes the sub-API whose SubApi value is bit i. */
#ifndef AV_READY_MESSAGE_HPP
#define AV_READY_MESSAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ReadyMessage {
public:
    static constexpr uint32_t lanes = 8u;
    static constexpr size_t   encoded_size = 12u + lanes * sizeof(uint32_t);
    static constexpr uint32_t descriptors = 0u;

    ReadyMessage();
    ReadyMessage(uint32_t version, uint32_t executor_abi, uint32_t sub_apis,
                 const uint32_t (&import_kinds)[lanes]);

    uint32_t version() const;
    uint32_t executor_abi() const;
    uint32_t sub_apis() const;
    /* Zero past the last lane. */
    uint32_t import_kinds(uint32_t lane) const;

    /* too_large for less room than encoded_size; invalid_parameter for a
     * message decode would refuse. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for any other length, any descriptor, a sub-API bit past the
     * last lane, or import kinds for a sub-API not offered. Unchanged unless
     * ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count);

private:
    bool fields_valid() const;

    uint32_t version_;
    uint32_t executor_abi_;
    uint32_t sub_apis_;
    uint32_t import_kinds_[lanes];
};

}  // namespace av

#endif  // AV_READY_MESSAGE_HPP
