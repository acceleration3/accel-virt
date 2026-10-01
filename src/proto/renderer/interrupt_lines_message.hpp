/* QEMU -> renderer: the device's queues' guest-notifier eventfds, which the
 * renderer signals to raise a fence interrupt without a hop through QEMU. A
 * queue whose notifier QEMU has not set up is absent, and only present lines
 * have a descriptor beside the message, in line order. interrupts_off, which
 * has no body, takes them all back. */
#ifndef AV_INTERRUPT_LINES_MESSAGE_HPP
#define AV_INTERRUPT_LINES_MESSAGE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class InterruptLinesMessage {
public:
    static constexpr size_t   encoded_size = 16u;
    /* One fence line per doorbell page (AV_DEVICE_DOORBELLS). */
    static constexpr uint32_t max_lines = 64u;

    InterruptLinesMessage();
    /* Bit i of `present` is line i. */
    InterruptLinesMessage(uint32_t lines, uint64_t present);

    uint32_t lines() const;
    bool     present(uint32_t line) const;
    /* One eventfd per present line. */
    uint32_t descriptors() const;

    /* too_large for less room than encoded_size; invalid_parameter for a
     * message decode would refuse. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* malformed for any other length, no lines or more than max_lines, a line
     * present past the last, a reserved word that is not zero, or a
     * descriptor count other than descriptors().
     * Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count);

private:
    bool fields_valid() const;

    uint32_t lines_;
    uint64_t present_;
};

}  // namespace av

#endif  // AV_INTERRUPT_LINES_MESSAGE_HPP
