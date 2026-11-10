#include "icd/opencl/cl_icd_root.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/opencl/cl_argument_type.hpp"
#include "icd/opencl/cl_mem_flags.hpp"
#include "proto/generated/opencl/cl_dispatch_client.h"

namespace av::clicd {

namespace {

constexpr size_t type_name_bytes = 96u;

}

ClKernelShape *ClIcdRoot::kernel_shape(ClObject &kernel, ClientTransport &wire)
{
    AV_SCOPE("ClIcdRoot::kernel_shape");
    if (kernel.shape() != nullptr) return kernel.shape();
    cl_uint count = 0u;
    if (av::clproto::av_clGetKernelInfo(wire, kernel.wire(), CL_KERNEL_NUM_ARGS, sizeof count, &count, nullptr) !=
        CL_SUCCESS)
        return nullptr;
    ClKernelShape *made = nullptr;
    if (shapes_.create(made, memory_) != Status::ok) return nullptr;
    if (made->size_to(count) != Status::ok) {
        static_cast<void>(shapes_.destroy(made));
        return nullptr;
    }
    const ClObject *const owner = kernel.parent();
    if (owner != nullptr && (owner->traits() & ClObject::program_typed_arguments) != 0u) {
        for (cl_uint index = 0u; index < count; ++index) {
            cl_kernel_arg_address_qualifier qualifier = 0u;
            char                            type_name[type_name_bytes] = {};
            if (av::clproto::av_clGetKernelArgInfo(wire, kernel.wire(), index, CL_KERNEL_ARG_ADDRESS_QUALIFIER,
                                                    sizeof qualifier, &qualifier, nullptr) != CL_SUCCESS ||
                av::clproto::av_clGetKernelArgInfo(wire, kernel.wire(), index, CL_KERNEL_ARG_TYPE_NAME,
                                                    sizeof type_name - 1u, type_name, nullptr) != CL_SUCCESS)
                break;
            const ClArgumentType type(type_name, qualifier);
            made->describe(index, type.kind(), type.bytes());
        }
    }
    const uint64_t        device = host_device_of(kernel);
    ClKernelShape::Limits limits = {};
    size_t                group = 0u;
    cl_ulong              local = 0u;
    if (av::clproto::av_clGetKernelWorkGroupInfo(wire, kernel.wire(), device, CL_KERNEL_WORK_GROUP_SIZE,
                                                  sizeof group, &group, nullptr) == CL_SUCCESS)
        limits.group_size = group;
    if (av::clproto::av_clGetKernelWorkGroupInfo(wire, kernel.wire(), device, CL_KERNEL_COMPILE_WORK_GROUP_SIZE,
                                                  sizeof limits.compiled_group, limits.compiled_group, nullptr) !=
        CL_SUCCESS)
        limits.compiled_group[0] = limits.compiled_group[1] = limits.compiled_group[2] = 0u;
    if (av::clproto::av_clGetKernelWorkGroupInfo(wire, kernel.wire(), device, CL_KERNEL_LOCAL_MEM_SIZE, sizeof local,
                                                  &local, nullptr) == CL_SUCCESS)
        limits.static_local_bytes = local;
    size_t sizes[3] = {};
    size_t answered = 0u;
    if (av::clproto::av_clGetDeviceInfo(wire, device, CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof sizes, sizes,
                                         &answered) == CL_SUCCESS)
        for (uint32_t axis = 0u; axis < 3u; ++axis) limits.item_sizes[axis] = sizes[axis];
    if (av::clproto::av_clGetDeviceInfo(wire, device, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS,
                                         sizeof limits.item_dimensions, &limits.item_dimensions, nullptr) !=
        CL_SUCCESS)
        limits.item_dimensions = 0u;
    size_t device_group = 0u;
    if (av::clproto::av_clGetDeviceInfo(wire, device, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof device_group,
                                         &device_group, nullptr) == CL_SUCCESS)
        limits.device_group_size = device_group;
    cl_ulong device_local = 0u;
    if (av::clproto::av_clGetDeviceInfo(wire, device, CL_DEVICE_LOCAL_MEM_SIZE, sizeof device_local, &device_local,
                                         nullptr) == CL_SUCCESS)
        limits.device_local_bytes = device_local;
    made->limit_by(limits);
    kernel.keep_shape(made);
    return made;
}

cl_int ClIcdRoot::judge_kernel_argument(ClKernelShape &shape, uint32_t index, size_t bytes, const void *value)
{
    AV_SCOPE("ClIcdRoot::judge_kernel_argument");
    const cl_int judged_size = shape.judge_argument(index, bytes, value);
    if (judged_size != CL_SUCCESS) return judged_size;
    const ClKernelShape::ArgKind kind = shape.kind(index);
    if (value == nullptr || bytes != sizeof(void *)) return CL_SUCCESS;
    if (kind != ClKernelShape::ArgKind::buffer && kind != ClKernelShape::ArgKind::image &&
        kind != ClKernelShape::ArgKind::sampler)
        return CL_SUCCESS;
    void *named = nullptr;
    __builtin_memcpy(&named, value, sizeof named);
    if (named == nullptr) return CL_SUCCESS;
    if (kind == ClKernelShape::ArgKind::sampler)
        return judged(named, ClObject::Kind::sampler) != nullptr ? CL_SUCCESS : CL_INVALID_SAMPLER;
    return judged(named, ClObject::Kind::mem) != nullptr ? CL_SUCCESS : CL_INVALID_MEM_OBJECT;
}

cl_int ClIcdRoot::judge_host_transfer(const ClObject &queue, const ClObject &mem, bool host_reads)
{
    AV_SCOPE("ClIcdRoot::judge_host_transfer");
    if (mem.context() != queue.context()) return CL_INVALID_CONTEXT;
    const ClMemFlags flags(mem.flags());
    return (host_reads ? flags.lets_host_read() : flags.lets_host_write()) ? CL_SUCCESS : CL_INVALID_OPERATION;
}

cl_int ClIcdRoot::judge_host_map(const ClObject &queue, const ClObject &mem, cl_map_flags map_flags)
{
    AV_SCOPE("ClIcdRoot::judge_host_map");
    if (mem.context() != queue.context()) return CL_INVALID_CONTEXT;
    const ClMemFlags flags(mem.flags());
    if ((map_flags & CL_MAP_READ) != 0u && !flags.lets_host_read()) return CL_INVALID_OPERATION;
    if ((map_flags & (CL_MAP_WRITE | CL_MAP_WRITE_INVALIDATE_REGION)) != 0u && !flags.lets_host_write())
        return CL_INVALID_OPERATION;
    return CL_SUCCESS;
}

}
