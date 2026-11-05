#ifndef AV_GLICD_HALF_FLOAT_HPP
#define AV_GLICD_HALF_FLOAT_HPP

#include <stdint.h>

namespace av::glicd {

class HalfFloat {
public:
    explicit HalfFloat(uint16_t bits);
    explicit HalfFloat(float value);

    uint16_t bits() const;
    float    value() const;

private:
    uint16_t bits_;
};

}

#endif
