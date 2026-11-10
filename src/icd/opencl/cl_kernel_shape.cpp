#include "icd/opencl/cl_kernel_shape.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

ClKernelShape::ClKernelShape(BlockSource &memory) : args_(memory, Pool::paged), limits_{}
{
    AV_SCOPE("ClKernelShape::ClKernelShape");
}

Status ClKernelShape::size_to(uint32_t count)
{
    AV_SCOPE("ClKernelShape::size_to");
    return args_.resize(count, Arg{ArgKind::unknown, false, 0u, 0u});
}

void ClKernelShape::describe(uint32_t index, ArgKind kind, uint32_t bytes)
{
    AV_SCOPE("ClKernelShape::describe");
    if (index >= args_.size()) return;
    args_.at(index).kind_ = kind;
    args_.at(index).bytes_ = bytes;
}

void ClKernelShape::limit_by(const Limits &limits)
{
    AV_SCOPE("ClKernelShape::limit_by");
    limits_ = limits;
}

Status ClKernelShape::copy_from(const ClKernelShape &other)
{
    AV_SCOPE("ClKernelShape::copy_from");
    const Status sized = size_to(other.args_.size());
    if (sized != Status::ok) return sized;
    for (uint32_t index = 0u; index < other.args_.size(); ++index) args_.at(index) = other.args_.at(index);
    limits_ = other.limits_;
    return Status::ok;
}

uint32_t ClKernelShape::count() const
{
    AV_SCOPE("ClKernelShape::count");
    return args_.size();
}

ClKernelShape::ArgKind ClKernelShape::kind(uint32_t index) const
{
    AV_SCOPE("ClKernelShape::kind");
    return index < args_.size() ? args_.at(index).kind_ : ArgKind::unknown;
}

cl_int ClKernelShape::judge_argument(uint32_t index, size_t bytes, const void *value) const
{
    AV_SCOPE("ClKernelShape::judge_argument");
    if (index >= args_.size()) return CL_INVALID_ARG_INDEX;
    const Arg &arg = args_.at(index);
    switch (arg.kind_) {
    case ArgKind::local:
        if (bytes == 0u) return CL_INVALID_ARG_SIZE;
        return value != nullptr ? CL_INVALID_ARG_VALUE : CL_SUCCESS;
    case ArgKind::buffer:
        return bytes != sizeof(cl_mem) ? CL_INVALID_ARG_SIZE : CL_SUCCESS;
    case ArgKind::image:
        if (bytes != sizeof(cl_mem)) return CL_INVALID_ARG_SIZE;
        return value == nullptr ? CL_INVALID_ARG_VALUE : CL_SUCCESS;
    case ArgKind::sampler:
        if (bytes != sizeof(cl_sampler)) return CL_INVALID_ARG_SIZE;
        return value == nullptr ? CL_INVALID_ARG_VALUE : CL_SUCCESS;
    case ArgKind::value:
        if (arg.bytes_ != 0u && bytes != arg.bytes_) return CL_INVALID_ARG_SIZE;
        return value == nullptr ? CL_INVALID_ARG_VALUE : CL_SUCCESS;
    case ArgKind::unknown:
        return CL_SUCCESS;
    }
    return CL_SUCCESS;
}

void ClKernelShape::note_set(uint32_t index, size_t bytes, const void *value)
{
    AV_SCOPE("ClKernelShape::note_set");
    static_cast<void>(value);
    if (index >= args_.size()) return;
    Arg &arg = args_.at(index);
    arg.set_ = true;
    arg.local_bytes_ = arg.kind_ == ArgKind::local ? bytes : 0u;
}

void ClKernelShape::note_set_pointer(uint32_t index)
{
    AV_SCOPE("ClKernelShape::note_set_pointer");
    if (index < args_.size()) args_.at(index).set_ = true;
}

bool ClKernelShape::all_set() const
{
    AV_SCOPE("ClKernelShape::all_set");
    for (uint32_t index = 0u; index < args_.size(); ++index)
        if (!args_.at(index).set_) return false;
    return true;
}

uint64_t ClKernelShape::local_bytes() const
{
    AV_SCOPE("ClKernelShape::local_bytes");
    uint64_t total = limits_.static_local_bytes_;
    for (uint32_t index = 0u; index < args_.size(); ++index) total += args_.at(index).local_bytes_;
    return total;
}

cl_int ClKernelShape::judge_launch(cl_uint dimensions, const size_t *offset, const size_t *global,
                                   const size_t *local, bool non_uniform) const
{
    AV_SCOPE("ClKernelShape::judge_launch");
    if (dimensions == 0u || dimensions > 3u || (limits_.item_dimensions_ != 0u && dimensions > limits_.item_dimensions_))
        return CL_INVALID_WORK_DIMENSION;
    if (!all_set()) return CL_INVALID_KERNEL_ARGS;
    if (global == nullptr) return CL_INVALID_GLOBAL_WORK_SIZE;
    for (cl_uint axis = 0u; axis < dimensions; ++axis) {
        if (global[axis] == 0u) return CL_INVALID_GLOBAL_WORK_SIZE;
        if (offset != nullptr && offset[axis] > ~size_t{0} - global[axis]) return CL_INVALID_GLOBAL_OFFSET;
    }
    const bool compiled = limits_.compiled_group_[0] != 0u && limits_.compiled_group_[1] != 0u &&
                          limits_.compiled_group_[2] != 0u;
    if (local != nullptr) {
        size_t items = 1u;
        for (cl_uint axis = 0u; axis < dimensions; ++axis) {
            if (local[axis] == 0u) return CL_INVALID_WORK_GROUP_SIZE;
            if (local[axis] > ~size_t{0} / items) return CL_INVALID_WORK_GROUP_SIZE;
            items *= local[axis];
        }
        if (limits_.group_size_ != 0u && items > limits_.group_size_) return CL_INVALID_WORK_GROUP_SIZE;
        if (limits_.device_group_size_ != 0u && items > limits_.device_group_size_) return CL_INVALID_WORK_GROUP_SIZE;
        for (cl_uint axis = 0u; axis < dimensions; ++axis) {
            if (limits_.item_sizes_[axis] != 0u && local[axis] > limits_.item_sizes_[axis])
                return CL_INVALID_WORK_ITEM_SIZE;
            if (!non_uniform && global[axis] % local[axis] != 0u) return CL_INVALID_WORK_GROUP_SIZE;
            if (compiled && local[axis] != limits_.compiled_group_[axis]) return CL_INVALID_WORK_GROUP_SIZE;
        }
        if (compiled) {
            for (cl_uint axis = dimensions; axis < 3u; ++axis)
                if (limits_.compiled_group_[axis] != 1u) return CL_INVALID_WORK_GROUP_SIZE;
        }
    }
    if (limits_.device_local_bytes_ != 0u && local_bytes() > limits_.device_local_bytes_) return CL_OUT_OF_RESOURCES;
    return CL_SUCCESS;
}

bool ClKernelShape::invariants_hold() const
{
    AV_SCOPE("ClKernelShape::invariants_hold");
    return args_.invariants_hold();
}

}
