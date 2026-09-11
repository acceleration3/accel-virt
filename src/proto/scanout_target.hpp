/* The VidPn source a primary surface is created for, and the refresh rate it
 * was created with. Zero for every other allocation. */
#ifndef AV_SCANOUT_TARGET_HPP
#define AV_SCANOUT_TARGET_HPP

#include <stdint.h>

namespace av {

class ScanoutTarget {
public:
    ScanoutTarget();
    ScanoutTarget(uint32_t source, uint32_t refresh_numerator, uint32_t refresh_denominator);

    uint32_t source() const;
    uint32_t refresh_numerator() const;
    uint32_t refresh_denominator() const;
    bool     is_empty() const;

private:
    uint32_t source_;
    uint32_t refresh_numerator_;
    uint32_t refresh_denominator_;
};

}  // namespace av

#endif  // AV_SCANOUT_TARGET_HPP
