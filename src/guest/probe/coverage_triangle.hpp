#ifndef AV_COVERAGE_TRIANGLE_HPP
#define AV_COVERAGE_TRIANGLE_HPP

#include <stdint.h>

namespace av {

class CoverageTriangle {
public:
    static constexpr int64_t subpixels = 256;

    CoverageTriangle(const int32_t (&x)[3], const int32_t (&y)[3]);

    uint32_t coverage(uint32_t samples, uint32_t px, uint32_t py) const;

private:
    bool covers(int64_t x, int64_t y) const;

    int64_t x_[3];
    int64_t y_[3];
    bool    area_;
};

}

#endif
