#ifndef AV_UM_HEAP_SLICE_OFFER12_HPP
#define AV_UM_HEAP_SLICE_OFFER12_HPP

#include "um/d3d/slice_offer.hpp"

namespace av::d3d12 {

class HeapSliceOffer12 final : public d3d::SliceOffer {
public:
    HeapSliceOffer12();

    bool granted(ClientTransport &host, DriverLog &lines) const override;
};

}

#endif
