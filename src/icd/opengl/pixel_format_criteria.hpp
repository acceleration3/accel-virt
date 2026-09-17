/* The criteria one wglChoosePixelFormatARB call asks for: the attribute names
 * and values of its integer and float lists together, in the order given, at
 * most two full lists' worth. */
#ifndef AV_GLICD_PIXEL_FORMAT_CRITERIA_HPP
#define AV_GLICD_PIXEL_FORMAT_CRITERIA_HPP

#include <stdint.h>

namespace av::glicd {

struct PixelFormatCriteriaProbe;

class PixelFormatCriteria {
public:
    static constexpr uint32_t capacity = 256u;

    PixelFormatCriteria();
    PixelFormatCriteria(const PixelFormatCriteria &) = delete;
    PixelFormatCriteria &operator=(const PixelFormatCriteria &) = delete;

    /* False, keeping nothing, once capacity criteria are held. */
    bool     add(uint32_t name, double value);
    uint32_t count() const;
    uint32_t name(uint32_t index) const;
    double   value(uint32_t index) const;

    bool invariants_hold() const;

private:
    uint32_t names_[capacity];
    double   values_[capacity];
    uint32_t count_;

    friend struct PixelFormatCriteriaProbe;
};

}  // namespace av::glicd

#endif  // AV_GLICD_PIXEL_FORMAT_CRITERIA_HPP
