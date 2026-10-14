#include "icd/d3d/context_slices.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d {

ContextSlices::ContextSlices() : slices_(nullptr)
{
    AV_SCOPE("ContextSlices::ContextSlices");
}

void ContextSlices::own(BufferSlices *slices)
{
    AV_SCOPE("ContextSlices::own");
    slices_ = slices;
}

bool ContextSlices::place(uint64_t bytes, BufferSlices::Slice &slice)
{
    AV_SCOPE("ContextSlices::place");
    BufferSlices::Slice taken = {};
    if (slices_ == nullptr || slices_->take_slice(bytes, taken) != Status::ok) return false;
    slice = taken;
    return true;
}

bool ContextSlices::rename(uint64_t bytes, BufferSlices::Slice &slice)
{
    AV_SCOPE("ContextSlices::rename");
    BufferSlices::Slice fresh = {};
    if (!place(bytes, fresh)) return false;
    slices_->retire(slice);
    slice = fresh;
    return true;
}

void ContextSlices::leave(BufferSlices::Slice &slice)
{
    AV_SCOPE("ContextSlices::leave");
    if (slices_ != nullptr) slices_->retire(slice);
    slice = BufferSlices::Slice{0u, 0u};
}

}  // namespace av::d3d
