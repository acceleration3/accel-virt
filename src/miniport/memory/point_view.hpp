#ifndef AV_POINT_VIEW_HPP
#define AV_POINT_VIEW_HPP

#include <stdint.h>

namespace av {

class PointView {
public:
    PointView();
    PointView(bool pointed, bool queued, uint32_t point, bool first_starvation);

    bool     pointed() const;
    bool     queued() const;
    uint32_t point() const;
    bool     first_starvation() const;

private:
    bool     pointed_;
    bool     queued_;
    uint32_t point_;
    bool     first_starvation_;
};

}

#endif
