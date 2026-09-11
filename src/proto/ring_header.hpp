/* One snapshot of a shared ring's header (docs/protocol.md). Counters are
 * totals of bytes produced and consumed; a byte's position is counter &
 * (capacity - 1). */
#ifndef AV_RING_HEADER_HPP
#define AV_RING_HEADER_HPP

#include "../core/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class RingHeader {
public:
    static constexpr size_t  encoded_size  = 32u;
    static constexpr uint8_t lowest_order  = 12u;
    static constexpr uint8_t highest_order = 24u;

    RingHeader();
    RingHeader(uint8_t order, uint64_t tail, uint64_t head);

    uint8_t  order() const;
    uint64_t capacity() const;
    uint64_t tail() const;
    uint64_t head() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* Structure only; RingProgress judges the counters. Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t magic = 0x524B5652u;  /* "RVKR" */

    uint8_t  order_;
    uint64_t tail_;
    uint64_t head_;
};

}  // namespace av

#endif  // AV_RING_HEADER_HPP
