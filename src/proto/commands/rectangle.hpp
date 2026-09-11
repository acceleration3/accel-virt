/* A rectangle on the wire: left, top, right, bottom as i32, edges exclusive at
 * right and bottom as in a RECT. */
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

private:
    int32_t left_;
    int32_t top_;
    int32_t right_;
    int32_t bottom_;
};

}  // namespace av

#endif  // AV_RECTANGLE_HPP
