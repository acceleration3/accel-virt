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

}
