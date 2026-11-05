#ifndef AV_UM_BUFFER_SLICE_OFFER9_HPP
#define AV_UM_BUFFER_SLICE_OFFER9_HPP

#include "um/d3d/slice_offer.hpp"

namespace av::d3d9 {

class BufferSliceOffer9 final : public d3d::SliceOffer {
public:
    BufferSliceOffer9();

    bool granted(ClientTransport &host, DriverLog &lines) const override;
};

}

#endif
