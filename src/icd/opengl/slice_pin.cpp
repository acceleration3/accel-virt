#include "icd/opengl/slice_pin.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/opengl/sliced_buffers.hpp"

namespace av::glicd {

SlicePin::SlicePin() : owner_(nullptr)
{
    AV_SCOPE("SlicePin::SlicePin");
}

SlicePin::~SlicePin()
{
    AV_SCOPE("SlicePin::~SlicePin");
    if (owner_ != nullptr) owner_->unpin();
}

void SlicePin::hold(SlicedBuffers *owner)
{
    AV_SCOPE("SlicePin::hold");
    if (owner_ != nullptr) owner_->unpin();
    owner_ = owner;
    if (owner_ != nullptr) owner_->pin();
}

bool SlicePin::invariants_hold() const
{
    AV_SCOPE("SlicePin::invariants_hold");
    return owner_ == nullptr || owner_->pinned() != 0u;
}

}
