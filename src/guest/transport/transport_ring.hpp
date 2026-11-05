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

    [[nodiscard]] Status initialize(uint8_t *ring, uint8_t order);
    [[nodiscard]] Status write_batch(const uint8_t *payload, uint32_t length, const uint8_t *trailer,
                                     uint32_t trailer_length, uint64_t fence_id, uint64_t published_at,
                                     uint64_t &begin, uint64_t &end);

    [[nodiscard]] Status publish_tail();
    uint64_t tail() const;
    uint64_t head() const;
    uint64_t capacity() const;
    uint64_t free_space() const;
    bool     consumer_awake() const;

    bool invariants_hold() const;

private:
    static constexpr size_t   header_size = 32u;
    static constexpr uint64_t tail_offset = 8u;
    static constexpr uint64_t head_offset = 16u;
    static constexpr uint64_t awake_offset = 5u;

    void     write_wrapped(uint64_t counter, const uint8_t *source, uint32_t count);
    uint64_t read_head() const;

    uint8_t *ring_;
    uint8_t  order_;
    uint64_t tail_;

    friend struct TransportRingProbe;
};

}

#endif
