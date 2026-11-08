#ifndef AV_SURFACE_PAIR_HPP
#define AV_SURFACE_PAIR_HPP

#include <stdint.h>

namespace av {

class SurfacePair {
public:
    SurfacePair();
    SurfacePair(uint64_t source, uint64_t destination);

    uint64_t source() const;
    uint64_t destination() const;
    bool     names() const;
    bool     operator==(const SurfacePair &other) const;

private:
    uint64_t source_;
    uint64_t destination_;
};

}

#endif
