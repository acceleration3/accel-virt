#ifndef AV_SURFACE_POINT_HPP
#define AV_SURFACE_POINT_HPP

#include <stdint.h>

namespace av {

class SurfacePoint {
public:
    SurfacePoint();
    SurfacePoint(uint64_t surface, uint32_t point);

    uint64_t surface() const;
    uint32_t point() const;
    bool     names() const;
    bool     operator==(const SurfacePoint &other) const;

private:
    uint64_t surface_;
    uint32_t point_;
};

}

#endif
