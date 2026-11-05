#include "um/d3d/slice_offer.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d {

SliceOffer::SliceOffer(uint64_t alignment) : alignment_(alignment)
{
    AV_SCOPE("SliceOffer::SliceOffer");
}

uint64_t SliceOffer::alignment() const
{
    AV_SCOPE("SliceOffer::alignment");
    return alignment_;
}

bool SliceOffer::invariants_hold() const
{
    AV_SCOPE("SliceOffer::invariants_hold");
    return alignment_ != 0u && (alignment_ & (alignment_ - 1u)) == 0u;
}

}
