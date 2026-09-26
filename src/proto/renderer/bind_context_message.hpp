/* QEMU -> renderer: binds a created context to its ring, fence page and reply
 * slot. The setup is the guest's own ContextSetup bytes, which QEMU validated
 * against its capset and the renderer decodes again, so they cross opaque.
 * A context with a doorbell has its ioeventfd's eventfd beside the message and
 * the doorbell's index here; one without has neither, so the flag says which
 * rather than the descriptor count alone. */
#ifndef AV_BIND_CONTEXT_MESSAGE_HPP
#define AV_BIND_CONTEXT_MESSAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BindContextMessage {
public:
    static constexpr size_t   fixed_size = 16u;
    static constexpr uint32_t has_doorbell_flag = 1u;

    BindContextMessage();
    /* No doorbell: the renderer polls the ring. A null setup is an empty one. */
    BindContextMessage(uint32_t context, const uint8_t *setup, uint32_t setup_length);
    BindContextMessage(uint32_t context, const uint8_t *setup, uint32_t setup_length, uint32_t doorbell);

    uint32_t       context() const;
    bool           has_doorbell() const;
    /* 0 without a doorbell. */
    uint32_t       doorbell() const;
    /* A view into the bytes decoded or the storage given, which must outlive
     * the message. */
    const uint8_t *setup() const;
    uint32_t       setup_length() const;
    /* The doorbell's eventfd, or none. */
    uint32_t       descriptors() const;
    size_t         encoded_length() const;

    /* too_large for less room than encoded_length(); invalid_parameter for a
     * message decode would refuse. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for context 0, an unknown flag, a doorbell index without the
     * flag, a setup that is empty or longer than any ContextSetup, a length
     * other than the fields give, or a descriptor count other than
     * descriptors(). Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count);

private:
    bool fields_valid() const;

    uint32_t       context_;
    uint32_t       doorbell_;
    uint32_t       flags_;
    const uint8_t *setup_;
    uint32_t       setup_length_;
};

}  // namespace av

#endif  // AV_BIND_CONTEXT_MESSAGE_HPP
