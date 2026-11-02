#ifndef AV_GUARDED_BUFFER_HPP
#define AV_GUARDED_BUFFER_HPP

#include <cstddef>
#include <cstdint>

namespace av {

class GuardedBuffer {
public:
    explicit GuardedBuffer(std::size_t size);
    ~GuardedBuffer();
    GuardedBuffer(const GuardedBuffer &) = delete;
    GuardedBuffer &operator=(const GuardedBuffer &) = delete;

    std::uint8_t *data();
    std::size_t   size() const;

    bool guards_intact() const;

private:
    static constexpr std::size_t guard_bytes = 64u;
    static constexpr std::uint8_t poison = 0x5Au;

    std::uint8_t *raw_;
    std::size_t   size_;
};

}

#endif
