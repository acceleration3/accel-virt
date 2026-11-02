#ifndef AV_GUEST_BYTES_HPP
#define AV_GUEST_BYTES_HPP

#include <stdint.h>

namespace av {

class GuestBytes {
public:
    GuestBytes();
    GuestBytes(uint8_t *base, uint64_t length);
    GuestBytes(const GuestBytes &) = delete;
    GuestBytes &operator=(const GuestBytes &) = delete;

    [[nodiscard]] bool bind(uint8_t *base, uint64_t length);
    void               unbind();
    bool               over(const uint8_t *base) const;

    uint8_t *at(uint64_t offset, uint64_t bytes) const;
    bool     holds(const void *bytes) const;
    uint64_t length() const;

    bool invariants_hold() const;

private:
    uint8_t *base_;
    uint64_t length_;
};

}

#endif
