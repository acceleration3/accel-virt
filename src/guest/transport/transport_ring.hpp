/* One context's ring, written from the guest (docs/protocol.md). The process
 * maps the ring blob - a 32-byte header then 2^order bytes of data addressed by
 * byte counters - and produces batches into it: each is a BatchHeader and its
 * payload, occupying `span` bytes rounded up to 8, wrapping the data area's end
 * as the device's reader does. The process owns the tail counter and the
 * miniport publishes it in the header once it has checked the submission
 * (docs/protocol.md, Submission); the device owns head, which the ring reads
 * to know how much room is free. This writes the ring bytes only; framing a batch's
 * payload into commands is the transport's above it. */
#ifndef AV_TRANSPORT_RING_HPP
#define AV_TRANSPORT_RING_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct TransportRingProbe;

class TransportRing {
public:
    TransportRing();

    /* Binds the mapped ring blob and writes its fresh header - magic, `order`,
     * tail and head 0 - as setup requires. invalid_parameter for no memory or
     * an order the protocol does not allow; invalid_state once bound. */
    [[nodiscard]] Status initialize(uint8_t *ring, uint8_t order);
    /* Writes one batch of `length` payload bytes with fence `fence_id` at the
     * tail, wrapping the data area, and moves the tail it keeps; `begin` and
     * `end` are the tail counters before and after, for the submission that
     * carries them. invalid_state unbound; invalid_parameter for fence 0 or a
     * batch too large for the ring; no_space until the device consumes enough. */
    [[nodiscard]] Status write_batch(const uint8_t *payload, uint32_t length, uint64_t fence_id,
                                     uint64_t &begin, uint64_t &end);

    uint64_t tail() const;
    /* The device's head, read from the ring header each time. */
    uint64_t head() const;
    uint64_t capacity() const;
    /* Data-area bytes not yet consumed by the device. */
    uint64_t free_space() const;

    bool invariants_hold() const;

private:
    static constexpr size_t   header_size = 32u;
    static constexpr uint64_t head_offset = 16u;

    void     write_wrapped(uint64_t counter, const uint8_t *source, uint32_t count);
    uint64_t read_head() const;

    uint8_t *ring_;
    uint8_t  order_;
    uint64_t tail_;

    friend struct TransportRingProbe;
};

}  // namespace av

#endif  // AV_TRANSPORT_RING_HPP
