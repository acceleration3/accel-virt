#ifndef AV_GUEST_PATTERN_HPP
#define AV_GUEST_PATTERN_HPP

#include <stdint.h>

namespace av {

class GuestPattern {
public:
    explicit GuestPattern(uint32_t bytes);

    uint32_t bytes() const;
    void     write(volatile unsigned char *view) const;
    void     clear(volatile unsigned char *view) const;
    uint32_t matched(const volatile unsigned char *view) const;
    uint32_t filled(const volatile unsigned char *view) const;

private:
    unsigned char at(uint32_t offset) const;

    uint32_t bytes_;
};

}

#endif
