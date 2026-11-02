#ifndef AV_OVERLAY_PLANE_HPP
#define AV_OVERLAY_PLANE_HPP

#include "miniport/ddi/rect_record.hpp"

#include <stdint.h>

namespace av {

struct DxgkMultiplaneOverlayAttributes3Record;

class OverlayPlane {
public:
    static constexpr uint32_t max_planes = 4u;

    OverlayPlane();
    OverlayPlane(uint32_t layer, uint64_t present_id);
    OverlayPlane(uint32_t layer, uint64_t present_id, void *allocation, int64_t address,
                 const DxgkMultiplaneOverlayAttributes3Record &attributes);

    uint32_t          layer() const;
    uint64_t          present_id() const;
    bool              flipped() const;
    bool              shown() const;
    void             *allocation() const;
    int64_t           address() const;
    const RectRecord &source() const;
    const RectRecord &destination() const;
    const RectRecord &clip() const;
    uint32_t          flips() const;
    bool              blended() const;
    bool              filtered() const;
    uint32_t          colour_space() const;

private:
    uint32_t   layer_;
    uint64_t   present_id_;
    void      *allocation_;
    int64_t    address_;
    RectRecord source_;
    RectRecord destination_;
    RectRecord clip_;
    uint32_t   flips_;
    bool       blended_;
    bool       filtered_;
    uint32_t   colour_space_;
};

}

#endif
