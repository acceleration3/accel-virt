#ifndef AV_POINT_VIEW_HPP
#define AV_POINT_VIEW_HPP

#include <stdint.h>

namespace av {

struct PointView {
    bool     pointed = false;
    bool     queued = false;
    uint32_t point = 0u;
    bool     first_starvation = false;
};

}

#endif
