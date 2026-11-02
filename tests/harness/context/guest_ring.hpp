#ifndef AV_GUEST_RING_HPP
#define AV_GUEST_RING_HPP

#include <cstdint>

namespace av {

class GuestRing {
public:
    static constexpr std::uint64_t tail_offset = 8u;
    static constexpr std::uint64_t head_offset = 16u;
    static constexpr std::uint32_t filler_command = 0x10001u;

    GuestRing(std::uint8_t *bytes, std::uint8_t order);

    void          write_batch(std::uint64_t fence, std::uint32_t length);
    void          write_batch(std::uint64_t fence, const std::uint8_t *payload, std::uint32_t length);
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

}

#endif
