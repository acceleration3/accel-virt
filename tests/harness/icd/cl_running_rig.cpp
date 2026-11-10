#include "harness/icd/cl_running_rig.hpp"

namespace av {

ClRunningRig::ClRunningRig(ClIcdRig &rig) : rig_(rig)
{
    if (rig_.loader().load() != FakeClLoader::Load::kept) return;
    rig_.host().arena_import(0x77u, 3u);
    rig_.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
    rig_.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
    rig_.host().cl_info(CL_SUCCESS, "OpenCL 3.0 host", 16u);
    rig_.host().cl_info(CL_SUCCESS, "OpenCL C 1.2", 13u);
    rig_.host().cl_info(CL_SUCCESS, "", 1u);
    cl_uint devices = 0u;
    if (rig_.loader().device_ids(CL_DEVICE_TYPE_GPU, 1u, &device_, &devices) != CL_SUCCESS) return;
    rig_.host().arena_import(0x78u, 3u);
    rig_.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
    rig_.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
    rig_.host().cl_handle(host_context, CL_SUCCESS);
    context_ = rig_.loader().create_context(device_, nullptr);
    if (context_ == nullptr) return;
    rig_.host().cl_handle(host_queue, CL_SUCCESS);
    queue_ = rig_.loader().create_queue(context_, device_, nullptr);
    rig_.host().cl_handle(host_buffer, CL_SUCCESS);
    buffer_ = rig_.loader().create_buffer(context_, CL_MEM_READ_WRITE, 64u * 1024u, nullptr);
    rig_.host().cl_handle(host_program, CL_SUCCESS);
    program_ = rig_.loader().create_program(context_, "__kernel void k(void){}", nullptr);
    if (program_ == nullptr) return;
    rig_.host().cl_handle(host_kernel, CL_SUCCESS);
    rig_.host().cl_kernel_undescribed();
    kernel_ = rig_.loader().create_kernel(program_, "k", nullptr);
    rig_.host().cl_handle(host_gate, CL_SUCCESS);
    gate_ = rig_.loader().create_user_event(context_, nullptr);
}

bool ClRunningRig::made() const
{
    return queue_ != nullptr && buffer_ != nullptr && kernel_ != nullptr && gate_ != nullptr;
}

ClIcdRig &ClRunningRig::rig() { return rig_; }

cl_device_id ClRunningRig::device() const { return device_; }

cl_context ClRunningRig::context() const { return context_; }

cl_command_queue ClRunningRig::queue() const { return queue_; }

cl_mem ClRunningRig::buffer() const { return buffer_; }

cl_program ClRunningRig::program() const { return program_; }

cl_kernel ClRunningRig::kernel() const { return kernel_; }

cl_event ClRunningRig::gate() const { return gate_; }

cl_mem ClRunningRig::image(std::uint64_t handle)
{
    const cl_image_format format = {CL_RGBA, CL_UNORM_INT8};
    cl_image_desc         desc = {};
    desc.image_type = CL_MEM_OBJECT_IMAGE2D;
    desc.image_width = 16u;
    desc.image_height = 8u;
    rig_.host().cl_handle(handle, CL_SUCCESS);
    return rig_.root().create_image(context_, CL_MEM_READ_WRITE, &format, &desc, nullptr, nullptr);
}

cl_event ClRunningRig::queue_event()
{
    rig_.host().cl_enqueued(CL_SUCCESS, host_event);
    cl_event marker = nullptr;
    static_cast<void>(rig_.root().enqueue_marker(queue_, 0u, nullptr, &marker));
    return marker;
}

void ClRunningRig::lose()
{
    rig_.kmt().withhold_answer_at(1u);
    static_cast<void>(rig_.loader().create_user_event(context_, nullptr));
}

}
