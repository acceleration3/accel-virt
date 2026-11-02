#ifndef AV_D3D_PRESENT_COUNT_HPP
#define AV_D3D_PRESENT_COUNT_HPP

#include <stdint.h>

namespace av::d3d {

class PresentCount {
public:
    PresentCount();

    uint32_t next();
    uint32_t last() const;

private:
    uint32_t count_;
};

}

#endif
