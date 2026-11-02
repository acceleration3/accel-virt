#ifndef AV_RECTANGLE_HPP
#define AV_RECTANGLE_HPP

#include <stdint.h>

namespace av {

class WireWriter;

class Rectangle {
public:
    Rectangle(int32_t left, int32_t top, int32_t right, int32_t bottom);

    bool is_valid() const;
    void write(WireWriter &writer) const;

    int32_t left() const;
    int32_t top() const;
    int32_t right() const;
    int32_t bottom() const;

private:
    int32_t left_;
    int32_t top_;
    int32_t right_;
    int32_t bottom_;
};

}

#endif
