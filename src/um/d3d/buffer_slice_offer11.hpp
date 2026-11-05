#ifndef AV_UM_BUFFER_SLICE_OFFER11_HPP
#define AV_UM_BUFFER_SLICE_OFFER11_HPP

#include "um/d3d/slice_offer.hpp"

namespace av::d3d11 {

class BufferSliceOffer11 final : public d3d::SliceOffer {
public:
    BufferSliceOffer11();

    bool granted(ClientTransport &host, DriverLog &lines) const override;
};

}

#endif
