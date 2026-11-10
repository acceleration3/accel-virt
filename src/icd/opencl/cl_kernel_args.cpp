#include "icd/opencl/cl_kernel_args.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

ClKernelArgs::ClKernelArgs(BlockSource &memory) : args_(memory, Pool::paged), bytes_(memory, Pool::paged)
{
    AV_SCOPE("ClKernelArgs::ClKernelArgs");
}

Status ClKernelArgs::place(const Arg &arg)
{
    AV_SCOPE("ClKernelArgs::place");
    for (uint32_t position = 0u; position < args_.size(); ++position) {
        if (args_.at(position).index != arg.index) continue;
        args_.at(position) = arg;
        return Status::ok;
    }
    return args_.append(arg);
}

Status ClKernelArgs::set_value(uint32_t index, const void *value, size_t bytes)
{
    AV_SCOPE("ClKernelArgs::set_value");
    if (value == nullptr || bytes == 0u || bytes > 0x10000u) return Status::invalid_parameter;
    const uint32_t offset = bytes_.size();
    const uint8_t *from = static_cast<const uint8_t *>(value);
    for (size_t at = 0u; at < bytes; ++at) {
        const Status kept = bytes_.append(from[at]);
        if (kept != Status::ok) {
            bytes_.truncate(offset);
            return kept;
        }
    }
    const Status placed = place(Arg{index, Kind::value, offset, static_cast<uint32_t>(bytes), nullptr});
    if (placed != Status::ok) bytes_.truncate(offset);
    return placed;
}

Status ClKernelArgs::set_object(uint32_t index, ClObject *object)
{
    AV_SCOPE("ClKernelArgs::set_object");
    if (object == nullptr) return Status::invalid_parameter;
    return place(Arg{index, Kind::object, 0u, 0u, object});
}

Status ClKernelArgs::set_local(uint32_t index, size_t bytes)
{
    AV_SCOPE("ClKernelArgs::set_local");
    if (bytes == 0u || bytes > 0xffffffffu) return Status::invalid_parameter;
    return place(Arg{index, Kind::local, 0u, static_cast<uint32_t>(bytes), nullptr});
}

Status ClKernelArgs::copy_from(const ClKernelArgs &other)
{
    AV_SCOPE("ClKernelArgs::copy_from");
    args_.clear();
    bytes_.clear();
    for (uint32_t position = 0u; position < other.count(); ++position) {
        const Arg &arg = other.at(position);
        Status     copied = Status::ok;
        if (arg.kind == Kind::value) copied = set_value(arg.index, other.bytes_of(arg), arg.bytes);
        else if (arg.kind == Kind::object) copied = set_object(arg.index, arg.object);
        else copied = set_local(arg.index, arg.bytes);
        if (copied != Status::ok) return copied;
    }
    return Status::ok;
}

uint32_t ClKernelArgs::count() const
{
    AV_SCOPE("ClKernelArgs::count");
    return args_.size();
}

const ClKernelArgs::Arg &ClKernelArgs::at(uint32_t position) const
{
    AV_SCOPE("ClKernelArgs::at");
    return args_.at(position);
}

const uint8_t *ClKernelArgs::bytes_of(const Arg &arg) const
{
    AV_SCOPE("ClKernelArgs::bytes_of");
    return arg.kind == Kind::value ? &bytes_.at(0u) + arg.offset : nullptr;
}

bool ClKernelArgs::invariants_hold() const
{
    AV_SCOPE("ClKernelArgs::invariants_hold");
    for (uint32_t position = 0u; position < args_.size(); ++position) {
        const Arg &arg = args_.at(position);
        if (arg.kind == Kind::value && static_cast<uint64_t>(arg.offset) + arg.bytes > bytes_.size()) return false;
        if (arg.kind == Kind::object && arg.object == nullptr) return false;
        for (uint32_t other = 0u; other < position; ++other)
            if (args_.at(other).index == arg.index) return false;
    }
    return true;
}

}
