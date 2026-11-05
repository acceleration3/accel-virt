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

void ContextSlices::trim()
{
    AV_SCOPE("ContextSlices::trim");
    if (slices_ != nullptr) slices_->trim();
}

uint8_t *ContextSlices::view(const BufferSlices::Slice &slice) const
{
    AV_SCOPE("ContextSlices::view");
    return slices_->view(slice);
}

uint64_t ContextSlices::memory(const BufferSlices::Slice &slice) const
{
    AV_SCOPE("ContextSlices::memory");
    return slices_->memory(slice);
}

uint64_t ContextSlices::offset_in_block(const BufferSlices::Slice &slice) const
{
    AV_SCOPE("ContextSlices::offset_in_block");
    return slices_->offset_in_block(slice);
}

}
