#ifndef AV_PRESENT_AREA_HPP
#define AV_PRESENT_AREA_HPP

#include <stdint.h>

namespace av {

class Rectangle;

class PresentArea {
public:
    PresentArea(int64_t left, int64_t top, int64_t right, int64_t bottom);
    explicit PresentArea(const Rectangle &rectangle);

    int64_t left() const;
    int64_t top() const;
    int64_t right() const;
    int64_t bottom() const;
    int64_t width() const;
    int64_t height() const;
    PresentArea clipped_to(uint32_t width, uint32_t height) const;
    PresentArea intersected_with(const PresentArea &other) const;

private:
    int64_t left_;
    int64_t top_;
    int64_t right_;
    int64_t bottom_;
};

}

#endif
