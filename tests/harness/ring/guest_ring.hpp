/* The producer side of a ring, as a guest driver writes it: a fresh header,
 * batches at the tail, the tail published when the test says so, and any byte
 * scribbled on when the test wants a hostile ring. Over memory the test owns -
 * a buffer, or mock guest RAM behind a blob. */
#ifndef AV_GUEST_RING_HPP
#define AV_GUEST_RING_HPP

#include <cstdint>

namespace av {

class GuestRing {
public:
    static constexpr std::uint64_t tail_offset = 8u;
    static constexpr std::uint64_t head_offset = 16u;

    /* Writes a header of `order` with tail and head 0 over `32 + 2^order` bytes. */
    GuestRing(std::uint8_t *bytes, std::uint8_t order);

    /* Header, `length` payload bytes and padding at the unpublished tail. */
    void          write_batch(std::uint64_t fence, std::uint32_t length);
    void          publish();
    void          publish(std::uint64_t tail);
    void          store(std::uint64_t offset, std::uint64_t value);
    std::uint64_t head() const;
    std::uint64_t tail() const;
    std::uint64_t capacity() const;
    std::uint8_t &data(std::uint64_t counter);
    std::uint8_t *bytes();

private:
    std::uint8_t *bytes_;
    std::uint8_t  order_;
    std::uint64_t tail_;
};

}  // namespace av

#endif  // AV_GUEST_RING_HPP
