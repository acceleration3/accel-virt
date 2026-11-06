#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/cl_icd_rig.hpp"
#include "harness/icd/fake_cl_loader.hpp"
#include "icd/opencl/cl_arena_blocks.hpp"
#include "icd/opencl/cl_buffer_rect.hpp"
#include "icd/opencl/cl_host_numbers.hpp"
#include "icd/opencl/cl_icd_root.hpp"
#include "icd/opencl/cl_object.hpp"
#include "icd/opencl/guest_cl_context.hpp"
#include "icd/opencl/cl_transfer_chunks.hpp"
#include "proto/generated/opencl/cl_protocol_ids.h"
#include "proto/client/transfer_room.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ClIcdRig;
using av::FakeClLoader;
using av::clicd::ClBufferRect;

namespace {
constexpr std::size_t write_room = av::clicd::GuestClContext::staging_bytes - av::TransferRoom::command_bytes;


constexpr std::uint64_t host_platform = 0x6200u;
constexpr std::uint64_t host_device = 0x6300u;
constexpr std::uint64_t host_context = 0x6400u;
constexpr std::uint64_t host_queue = 0x6500u;
constexpr std::uint64_t host_buffer = 0x6600u;
constexpr std::uint64_t host_program = 0x6700u;
constexpr std::uint64_t host_kernel = 0x6800u;
constexpr std::uint64_t host_event = 0x6900u;
constexpr std::uint64_t host_image = 0x6A00u;
constexpr std::uint64_t host_sampler = 0x6B00u;
constexpr std::uint64_t own_queue = 0x6C00u;

const cl_icd_dispatch *table_of(const void *object)
{
    return object != nullptr ? *reinterpret_cast<const cl_icd_dispatch *const *>(object) : nullptr;
}

class Running {
public:
    explicit Running(ClIcdRig &rig) : rig_(rig)
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
        buffer_ = rig_.loader().create_buffer(context_, CL_MEM_READ_WRITE, 256u, nullptr);
        rig_.host().cl_handle(host_program, CL_SUCCESS);
        program_ = rig_.loader().create_program(context_, "__kernel void k(void){}", nullptr);
        if (program_ == nullptr) return;
        rig_.host().cl_handle(host_kernel, CL_SUCCESS);
        kernel_ = rig_.loader().create_kernel(program_, "k", nullptr);
    }

    bool made() const { return kernel_ != nullptr && buffer_ != nullptr && queue_ != nullptr; }
    cl_device_id     device() const { return device_; }
    cl_context       context() const { return context_; }
    cl_command_queue queue() const { return queue_; }
    cl_mem           buffer() const { return buffer_; }
    cl_program       program() const { return program_; }
    cl_kernel        kernel() const { return kernel_; }

    cl_mem image()
    {
        const cl_image_format format = {CL_RGBA, CL_UNORM_INT8};
        cl_image_desc         desc = {};
        desc.image_type = CL_MEM_OBJECT_IMAGE2D;
        desc.image_width = 16u;
        desc.image_height = 8u;
        rig_.host().cl_handle(host_image, CL_SUCCESS);
        return rig_.root().create_image(context_, CL_MEM_READ_WRITE, &format, &desc, nullptr, nullptr);
    }

    void leaves_work()
    {
        rig_.host().cl_enqueued(CL_SUCCESS, host_event);
        cl_event marker = nullptr;
        static_cast<void>(table_of(queue_)->clEnqueueMarkerWithWaitList(queue_, 0u, nullptr, &marker));
    }

    void drains()
    {
        rig_.host().cl_enqueued(CL_SUCCESS, host_event);
        rig_.host().cl_result(CL_SUCCESS);
        const cl_int complete = CL_COMPLETE;
        rig_.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
        rig_.host().cl_result(CL_SUCCESS);
    }

    void element_is(std::size_t bytes)
    {
        rig_.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&bytes), sizeof bytes);
    }

private:
    ClIcdRig        &rig_;
    cl_device_id     device_ = nullptr;
    cl_context       context_ = nullptr;
    cl_command_queue queue_ = nullptr;
    cl_mem           buffer_ = nullptr;
    cl_program       program_ = nullptr;
    cl_kernel        kernel_ = nullptr;
};

av::WireReader payload_of(const av::MockKmt &kmt, std::uint32_t index)
{
    return av::WireReader(kmt.command_bytes(index) + av::CommandHeader::encoded_size,
                          av::MockKmt::journal_bytes - av::CommandHeader::encoded_size);
}

bool carries(const av::MockKmt &kmt, std::uint32_t index, std::uint64_t handle)
{
    const std::uint8_t *const frame = kmt.command_bytes(index);
    if (frame == nullptr) return false;
    for (std::size_t at = av::CommandHeader::encoded_size; at + sizeof handle <= av::MockKmt::journal_bytes; ++at)
        if (std::memcmp(frame + at, &handle, sizeof handle) == 0) return true;
    return false;
}

}

TEST(cl_image_paths, a_programs_build_log_is_the_hosts_for_its_own_device)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const std::uint32_t sent = rig.kmt().commands();
  char                text[16] = {};
  size_t              size = 0u;
  CHECK_EQ(rig.root().program_build_info(reinterpret_cast<cl_program>(run.kernel()), run.device(),
                                         CL_PROGRAM_BUILD_LOG, sizeof text, text, &size),
           CL_INVALID_PROGRAM);
  CHECK_EQ(rig.root().program_build_info(run.program(), reinterpret_cast<cl_device_id>(run.context()),
                                         CL_PROGRAM_BUILD_LOG, sizeof text, text, &size),
           CL_INVALID_DEVICE);
  CHECK_EQ(rig.kmt().commands(), sent);
  rig.host().cl_info(CL_SUCCESS, "no errors", 10u);
  CHECK_EQ(table_of(run.program())->clGetProgramBuildInfo(run.program(), run.device(),
                                                                        CL_PROGRAM_BUILD_LOG, sizeof text, text,
                                                                        &size),
           CL_SUCCESS);
  CHECK(std::strcmp(text, "no errors") == 0);
  CHECK_EQ(size, 10u);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clGetProgramBuildInfo);
  CHECK(carries(rig.kmt(), sent, host_program));
  CHECK(carries(rig.kmt(), sent, host_device)); }

TEST(cl_image_paths, a_kernels_work_group_info_may_name_its_device_or_none)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  size_t largest = 0u;
  CHECK_EQ(rig.root().kernel_work_group_info(reinterpret_cast<cl_kernel>(run.program()), run.device(),
                                             CL_KERNEL_WORK_GROUP_SIZE, sizeof largest, &largest, nullptr),
           CL_INVALID_KERNEL);
  CHECK_EQ(rig.root().kernel_work_group_info(run.kernel(), reinterpret_cast<cl_device_id>(run.queue()),
                                             CL_KERNEL_WORK_GROUP_SIZE, sizeof largest, &largest, nullptr),
           CL_INVALID_DEVICE);
  const size_t answer = 256u;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&answer), sizeof answer);
  CHECK_EQ(table_of(run.kernel())->clGetKernelWorkGroupInfo(
               run.kernel(), nullptr, CL_KERNEL_WORK_GROUP_SIZE, sizeof largest, &largest, nullptr),
           CL_SUCCESS);
  CHECK_EQ(largest, 256u);
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&answer), sizeof answer);
  largest = 0u;
  CHECK_EQ(rig.root().kernel_work_group_info(run.kernel(), run.device(), CL_KERNEL_WORK_GROUP_SIZE,
                                             sizeof largest, &largest, nullptr),
           CL_SUCCESS);
  CHECK_EQ(largest, 256u);
  CHECK(carries(rig.kmt(), rig.kmt().commands() - 1u, host_device)); }

TEST(cl_image_paths, a_copy_and_a_fill_are_enqueued_with_the_hosts_numbers_and_its_event)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  rig.host().cl_handle(host_buffer + 1u, CL_SUCCESS);
  cl_mem other = rig.root().create_buffer(run.context(), CL_MEM_READ_WRITE, 256u, nullptr, nullptr, nullptr);
  CHECK(other != nullptr);

  std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(table_of(run.queue())->clEnqueueCopyBuffer(run.queue(), run.buffer(), other, 4u, 8u, 64u, 0u,
                                                                    nullptr, nullptr),
           CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent);

  cl_event event = nullptr;
  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  CHECK_EQ(rig.root().enqueue_copy_buffer(run.queue(), run.buffer(), other, 0u, 0u, 64u, &event), CL_SUCCESS);
  CHECK(event != nullptr);
  CHECK(table_of(event) != nullptr);
  CHECK_EQ(rig.kmt().commands(), sent + 2u);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueCopyBuffer);
  CHECK(carries(rig.kmt(), sent, host_buffer));
  CHECK(carries(rig.kmt(), sent, host_buffer + 1u));
  CHECK_EQ(rig.kmt().command_id(sent + 1u), av::clproto::kCmd_clEnqueueCopyBuffer);

  const std::uint32_t pattern = 0xDEADBEEFu;
  sent = rig.kmt().commands();
  CHECK_EQ(table_of(run.queue())->clEnqueueFillBuffer(run.queue(), run.buffer(), &pattern,
                                                                    sizeof pattern, 0u, 256u, 0u, nullptr, nullptr),
           CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent);
  cl_event filled = nullptr;
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 1u);
  CHECK_EQ(rig.root().enqueue_fill_buffer(run.queue(), run.buffer(), &pattern, sizeof pattern, 0u, 256u, &filled),
           CL_SUCCESS);
  CHECK(filled != nullptr);
  CHECK_EQ(rig.kmt().commands(), sent + 2u);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueFillBuffer);
  CHECK(carries(rig.kmt(), sent, host_buffer));
  CHECK_EQ(rig.kmt().command_id(sent + 1u), av::clproto::kCmd_clEnqueueFillBuffer);

  cl_event refused = nullptr;
  rig.host().cl_enqueued(CL_INVALID_VALUE, 0u);
  CHECK_EQ(rig.root().enqueue_copy_buffer(run.queue(), run.buffer(), other, 250u, 0u, 64u, &refused),
           CL_INVALID_VALUE);
  CHECK(refused == nullptr);
  rig.host().cl_enqueued(CL_INVALID_VALUE, 0u);
  CHECK_EQ(rig.root().enqueue_fill_buffer(run.queue(), run.buffer(), &pattern, 3u, 0u, 256u, &refused),
           CL_INVALID_VALUE);
  CHECK(refused == nullptr);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_image_paths, a_copy_or_a_fill_of_something_that_is_not_its_object_asks_the_host_nothing)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const std::uint32_t sent = rig.kmt().commands();
  const std::uint32_t pattern = 1u;
  const cl_mem        not_a_buffer = reinterpret_cast<cl_mem>(run.kernel());
  const cl_command_queue not_a_queue = reinterpret_cast<cl_command_queue>(run.context());
  CHECK_EQ(rig.root().enqueue_copy_buffer(not_a_queue, run.buffer(), run.buffer(), 0u, 0u, 4u, nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().enqueue_copy_buffer(run.queue(), not_a_buffer, run.buffer(), 0u, 0u, 4u, nullptr),
           CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.root().enqueue_copy_buffer(run.queue(), run.buffer(), not_a_buffer, 0u, 0u, 4u, nullptr),
           CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.root().enqueue_fill_buffer(not_a_queue, run.buffer(), &pattern, 4u, 0u, 4u, nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().enqueue_fill_buffer(run.queue(), not_a_buffer, &pattern, 4u, 0u, 4u, nullptr),
           CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.root().enqueue_fill_buffer(run.queue(), run.buffer(), nullptr, 4u, 0u, 4u, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_fill_buffer(run.queue(), run.buffer(), &pattern, 0u, 0u, 4u, nullptr), CL_INVALID_VALUE);
  const size_t origin[3] = {0u, 0u, 0u}, region[3] = {1u, 1u, 1u};
  CHECK_EQ(rig.root().enqueue_copy_image(not_a_queue, run.buffer(), run.buffer(), origin, origin, region, nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().enqueue_copy_image(run.queue(), not_a_buffer, run.buffer(), origin, origin, region, nullptr),
           CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.root().enqueue_copy_image(run.queue(), run.buffer(), not_a_buffer, origin, origin, region, nullptr),
           CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.root().enqueue_copy_image(run.queue(), run.buffer(), run.buffer(), nullptr, origin, region, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_copy_image(run.queue(), run.buffer(), run.buffer(), origin, nullptr, region, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_copy_image(run.queue(), run.buffer(), run.buffer(), origin, origin, nullptr, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().commands(), sent); }

TEST(cl_image_paths, an_image_is_the_hosts_and_a_buffer_it_is_made_over_crosses_as_the_hosts_number)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const std::uint32_t sent = rig.kmt().commands();
  cl_mem              image = run.image();
  CHECK(image != nullptr);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clCreateImage);
  CHECK(table_of(image) != nullptr);

  const cl_image_format format = {CL_RGBA, CL_UNORM_INT8};
  cl_image_desc         desc = {};
  desc.image_type = CL_MEM_OBJECT_IMAGE1D_BUFFER;
  desc.image_width = 64u;
  desc.buffer = run.buffer();
  cl_int error = CL_SUCCESS;
  rig.host().cl_handle(host_image + 1u, CL_SUCCESS);
  const std::uint32_t at = rig.kmt().commands();
  CHECK(table_of(run.context())->clCreateImage(run.context(), CL_MEM_READ_ONLY, &format, &desc,
                                                              nullptr, &error) != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  av::WireReader payload = payload_of(rig.kmt(), at);
  std::uint64_t  context = 0u, flags = 0u;
  std::uint8_t   present = 0u;
  cl_image_format crossed_format = {};
  cl_image_desc   crossed = {};
  CHECK(payload.read_pod(context) && context == host_context);
  CHECK(payload.read_varint(flags) && payload.read_pod(present) && present == 1u);
  CHECK(payload.read(&crossed_format, sizeof crossed_format));
  CHECK(payload.read_pod(present) && present == 1u && payload.read(&crossed, sizeof crossed));
  CHECK_EQ(reinterpret_cast<std::uintptr_t>(crossed.buffer), host_buffer);
  CHECK_EQ(crossed.image_width, 64u);
  CHECK(desc.buffer == run.buffer());
  CHECK(rig.root().invariants_hold()); }

TEST(cl_image_paths, an_image_the_api_or_this_driver_refuses_asks_the_host_nothing)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const std::uint32_t   sent = rig.kmt().commands();
  const cl_image_format format = {CL_RGBA, CL_UNORM_INT8};
  cl_image_desc         desc = {};
  desc.image_type = CL_MEM_OBJECT_IMAGE2D;
  desc.image_width = 4u;
  desc.image_height = 4u;
  std::uint32_t texels[16] = {};
  cl_int        error = CL_SUCCESS;
  CHECK(rig.root().create_image(reinterpret_cast<cl_context>(run.queue()), 0u, &format, &desc, nullptr, &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  CHECK(rig.root().create_image(run.context(), 0u, nullptr, &desc, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_IMAGE_FORMAT_DESCRIPTOR);
  CHECK(rig.root().create_image(run.context(), 0u, &format, nullptr, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_IMAGE_DESCRIPTOR);
  CHECK(rig.root().create_image(run.context(), CL_MEM_USE_HOST_PTR, &format, &desc, texels, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK_EQ(rig.log().holding("not carried"), 1u);
  CHECK(rig.root().create_image(run.context(), CL_MEM_COPY_HOST_PTR, &format, &desc, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_HOST_PTR);
  CHECK(rig.root().create_image(run.context(), 0u, &format, &desc, texels, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_HOST_PTR);
  desc.buffer = reinterpret_cast<cl_mem>(run.kernel());
  CHECK(rig.root().create_image(run.context(), 0u, &format, &desc, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_IMAGE_DESCRIPTOR);
  CHECK_EQ(rig.kmt().commands(), sent);

  desc.buffer = nullptr;
  rig.host().cl_handle(0u, CL_IMAGE_FORMAT_NOT_SUPPORTED);
  CHECK(rig.root().create_image(run.context(), 0u, &format, &desc, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_IMAGE_FORMAT_NOT_SUPPORTED);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_image_paths, an_image_copied_from_memory_is_written_on_a_queue_of_the_icds_own)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const cl_image_format format = {CL_RGBA, CL_UNORM_INT8};
  cl_image_desc         desc = {};
  desc.image_type = CL_MEM_OBJECT_IMAGE2D;
  desc.image_width = 4u;
  desc.image_height = 2u;
  std::uint32_t texels[8] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
  const std::uint32_t sent = rig.kmt().commands();
  rig.host().cl_handle(host_image, CL_SUCCESS);
  run.element_is(4u);
  rig.host().cl_handle(own_queue, CL_SUCCESS);
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_result(CL_SUCCESS);
  cl_int error = CL_INVALID_VALUE;
  cl_mem image =
      rig.root().create_image(run.context(), CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, &format, &desc, texels, &error);
  CHECK(image != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent + 5u);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clCreateImage);
  CHECK_EQ(rig.kmt().command_id(sent + 1u), av::clproto::kCmd_clGetImageInfo);
  CHECK_EQ(rig.kmt().command_id(sent + 2u), av::clproto::kCmd_clCreateCommandQueueWithProperties);
  CHECK_EQ(rig.kmt().command_id(sent + 3u), av::clproto::kCmd_clEnqueueWriteImage);
  CHECK_EQ(rig.kmt().command_id(sent + 4u), av::clproto::kCmd_clReleaseCommandQueue);
  CHECK(carries(rig.kmt(), sent + 3u, own_queue));
  CHECK(carries(rig.kmt(), sent + 3u, host_image));
  CHECK(carries(rig.kmt(), sent + 3u, 0x0000000200000001ull));
  av::WireReader payload = payload_of(rig.kmt(), sent);
  std::uint64_t  context = 0u, flags = 0u;
  CHECK(payload.read_pod(context) && payload.read_varint(flags));
  CHECK_EQ(flags & CL_MEM_COPY_HOST_PTR, 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_image_paths, an_image_whose_bytes_could_not_be_written_is_given_back_to_the_host)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const cl_image_format format = {CL_RGBA, CL_UNORM_INT8};
  cl_image_desc         desc = {};
  desc.image_type = CL_MEM_OBJECT_IMAGE2D;
  desc.image_width = 4u;
  desc.image_height = 2u;
  std::uint32_t texels[8] = {};
  cl_int        error = CL_SUCCESS;

  std::uint32_t sent = rig.kmt().commands();
  rig.host().cl_handle(host_image, CL_SUCCESS);
  rig.host().cl_info(CL_INVALID_MEM_OBJECT, "", 0u);
  rig.host().cl_result(CL_SUCCESS);
  CHECK(rig.root().create_image(run.context(), CL_MEM_COPY_HOST_PTR, &format, &desc, texels, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_IMAGE_DESCRIPTOR);
  CHECK_EQ(rig.kmt().command_id(rig.kmt().commands() - 1u), av::clproto::kCmd_clReleaseMemObject);
  CHECK(carries(rig.kmt(), rig.kmt().commands() - 1u, host_image));

  rig.host().cl_handle(host_image, CL_SUCCESS);
  run.element_is(4u);
  rig.host().cl_handle(0u, CL_OUT_OF_HOST_MEMORY);
  rig.host().cl_result(CL_SUCCESS);
  CHECK(rig.root().create_image(run.context(), CL_MEM_COPY_HOST_PTR, &format, &desc, texels, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_HOST_MEMORY);
  CHECK_EQ(rig.kmt().command_id(rig.kmt().commands() - 1u), av::clproto::kCmd_clReleaseMemObject);

  sent = rig.kmt().commands();
  rig.host().cl_handle(host_image, CL_SUCCESS);
  run.element_is(4u);
  rig.host().cl_handle(own_queue, CL_SUCCESS);
  rig.host().cl_result(CL_INVALID_OPERATION);
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_result(CL_SUCCESS);
  CHECK(rig.root().create_image(run.context(), CL_MEM_COPY_HOST_PTR, &format, &desc, texels, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_OPERATION);
  CHECK_EQ(rig.kmt().command_id(sent + 4u), av::clproto::kCmd_clReleaseCommandQueue);
  CHECK_EQ(rig.kmt().command_id(sent + 5u), av::clproto::kCmd_clReleaseMemObject);

  desc.image_type = CL_MEM_OBJECT_IMAGE1D_ARRAY;
  desc.image_array_size = 2u;
  rig.host().cl_handle(host_image, CL_SUCCESS);
  rig.host().cl_result(CL_SUCCESS);
  CHECK(rig.root().create_image(run.context(), CL_MEM_COPY_HOST_PTR, &format, &desc, texels, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_OPERATION);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_image_paths, every_image_layout_made_of_rows_and_slices_can_be_copied_from_memory)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const cl_image_format      format = {CL_R, CL_UNSIGNED_INT8};
  const cl_mem_object_type   types[4] = {CL_MEM_OBJECT_IMAGE1D, CL_MEM_OBJECT_IMAGE2D, CL_MEM_OBJECT_IMAGE2D_ARRAY,
                                         CL_MEM_OBJECT_IMAGE3D};
  const std::uint64_t rows[4] = {1u, 3u, 3u, 3u};
  const std::uint64_t slices[4] = {1u, 1u, 2u, 2u};
  std::uint8_t        texels[64] = {};
  for (std::uint32_t index = 0u; index < 4u; ++index) {
      cl_image_desc desc = {};
      desc.image_type = types[index];
      desc.image_width = 5u;
      desc.image_height = 3u;
      desc.image_depth = 2u;
      desc.image_array_size = 2u;
      rig.host().cl_handle(host_image + index, CL_SUCCESS);
      run.element_is(1u);
      rig.host().cl_handle(own_queue, CL_SUCCESS);
      for (std::uint64_t slice = 0u; slice < slices[index]; ++slice) rig.host().cl_result(CL_SUCCESS);
      rig.host().cl_result(CL_SUCCESS);
      const std::uint32_t sent = rig.kmt().commands();
      cl_int              error = CL_INVALID_VALUE;
      CHECK(rig.root().create_image(run.context(), CL_MEM_COPY_HOST_PTR, &format, &desc, texels, &error) != nullptr);
      CHECK_EQ(error, CL_SUCCESS);
      CHECK_EQ(rig.kmt().commands(), sent + 4u + static_cast<std::uint32_t>(slices[index]));
      av::WireReader payload = payload_of(rig.kmt(), sent + 3u);
      std::uint64_t  queue = 0u, image = 0u, blocking = 0u, count = 0u, origin[3] = {}, region[3] = {};
      std::uint8_t   present = 0u;
      CHECK(payload.read_pod(queue) && payload.read_pod(image) && payload.read_varint(blocking));
      CHECK(payload.read_pod(present) && payload.read_varint(count) && payload.read_varint(origin[0]) &&
            payload.read_varint(origin[1]) && payload.read_varint(origin[2]));
      CHECK(payload.read_pod(present) && payload.read_varint(count) && payload.read_varint(region[0]) &&
            payload.read_varint(region[1]) && payload.read_varint(region[2]));
      CHECK_EQ(region[0], 5u);
      CHECK_EQ(region[1], rows[index]);
      CHECK_EQ(region[2], 1u);
  }
  CHECK(rig.root().invariants_hold()); }

TEST(cl_image_paths, an_image_read_waits_for_its_queue_and_hands_back_the_rows_it_asked_for)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  cl_mem image = run.image();
  CHECK(image != nullptr);
  const size_t        origin[3] = {2u, 1u, 0u}, region[3] = {4u, 2u, 1u};
  const unsigned char answered[32] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u,
                                      17u, 18u, 19u, 20u, 21u, 22u, 23u, 24u, 25u, 26u, 27u, 28u, 29u, 30u, 31u, 32u};
  unsigned char       into[32] = {};
  run.leaves_work();
  run.element_is(4u);
  run.drains();
  rig.host().cl_bytes(CL_SUCCESS, answered, sizeof answered);
  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  cl_event            event = nullptr;
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(table_of(run.queue())->clEnqueueReadImage(run.queue(), image, CL_FALSE, origin, region, 0u, 0u,
                                                                   into, 0u, nullptr, &event),
           CL_SUCCESS);
  for (std::size_t index = 0u; index < sizeof into; ++index) CHECK_EQ(into[index], answered[index]);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clGetImageInfo);
  CHECK_EQ(rig.kmt().command_id(sent + 1u), av::clproto::kCmd_clEnqueueMarkerWithWaitList);
  CHECK_EQ(rig.kmt().command_id(sent + 5u), av::clproto::kCmd_clEnqueueReadImage);
  CHECK_EQ(rig.kmt().command_id(sent + 6u), av::clproto::kCmd_clEnqueueMarkerWithWaitList);
  CHECK(event != nullptr);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_image_paths, an_image_write_waits_for_its_queue_and_asks_for_an_event_only_when_wanted)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  cl_mem image = run.image();
  CHECK(image != nullptr);
  const size_t        origin[3] = {0u, 0u, 0u}, region[3] = {16u, 8u, 1u};
  std::uint32_t       texels[128] = {};
  run.leaves_work();
  run.element_is(4u);
  run.drains();
  rig.host().cl_result(CL_SUCCESS);
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(table_of(run.queue())->clEnqueueWriteImage(run.queue(), image, CL_TRUE, origin, region, 0u, 0u,
                                                                    texels, 0u, nullptr, nullptr),
           CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent + 6u);
  CHECK_EQ(rig.kmt().command_id(sent + 5u), av::clproto::kCmd_clEnqueueWriteImage);
  CHECK(carries(rig.kmt(), sent + 5u, host_image));

  run.leaves_work();
  run.element_is(4u);
  run.drains();
  rig.host().cl_result(CL_INVALID_OPERATION);
  cl_event event = nullptr;
  CHECK_EQ(rig.root().enqueue_write_image(run.queue(), image, origin, region, 0u, 0u, texels, &event),
           CL_INVALID_OPERATION);
  CHECK(event == nullptr);

  run.leaves_work();
  run.element_is(4u);
  rig.host().cl_enqueued(CL_OUT_OF_RESOURCES, 0u);
  CHECK_EQ(rig.root().enqueue_write_image(run.queue(), image, origin, region, 0u, 0u, texels, nullptr),
           CL_OUT_OF_RESOURCES);
  run.leaves_work();
  run.element_is(4u);
  rig.host().cl_enqueued(CL_OUT_OF_RESOURCES, 0u);
  CHECK_EQ(rig.root().enqueue_read_image(run.queue(), image, origin, region, 0u, 0u, texels, nullptr),
           CL_OUT_OF_RESOURCES);
  run.leaves_work();
  run.element_is(4u);
  run.drains();
  rig.host().cl_bytes(CL_INVALID_OPERATION, texels, 0u);
  CHECK_EQ(rig.root().enqueue_read_image(run.queue(), image, origin, region, 0u, 0u, texels, &event),
           CL_INVALID_OPERATION);
  CHECK(event == nullptr);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_image_paths, an_image_transfer_the_api_would_refuse_asks_the_host_at_most_its_texel_size)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  cl_mem image = run.image();
  CHECK(image != nullptr);
  const size_t           origin[3] = {0u, 0u, 0u}, region[3] = {4u, 4u, 1u}, empty[3] = {4u, 0u, 1u};
  std::uint32_t          texels[16] = {};
  const cl_command_queue not_a_queue = reinterpret_cast<cl_command_queue>(run.kernel());
  const cl_mem           not_an_image = reinterpret_cast<cl_mem>(run.queue());
  std::uint32_t          sent = rig.kmt().commands();
  CHECK_EQ(rig.root().enqueue_read_image(not_a_queue, image, origin, region, 0u, 0u, texels, nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().enqueue_read_image(run.queue(), not_an_image, origin, region, 0u, 0u, texels, nullptr),
           CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.root().enqueue_read_image(run.queue(), image, nullptr, region, 0u, 0u, texels, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_read_image(run.queue(), image, origin, nullptr, 0u, 0u, texels, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_read_image(run.queue(), image, origin, region, 0u, 0u, nullptr, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_write_image(not_a_queue, image, origin, region, 0u, 0u, texels, nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().enqueue_write_image(run.queue(), not_an_image, origin, region, 0u, 0u, texels, nullptr),
           CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.root().enqueue_write_image(run.queue(), image, nullptr, region, 0u, 0u, texels, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_write_image(run.queue(), image, origin, nullptr, 0u, 0u, texels, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_write_image(run.queue(), image, origin, region, 0u, 0u, nullptr, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().commands(), sent);
  run.element_is(4u);
  CHECK_EQ(rig.root().enqueue_read_image(run.queue(), image, origin, empty, 0u, 0u, texels, nullptr),
           CL_INVALID_VALUE);
  run.element_is(4u);
  CHECK_EQ(rig.root().enqueue_write_image(run.queue(), image, origin, region, 8u, 0u, texels, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().commands(), sent + 2u); }

TEST(cl_image_paths, a_sampler_is_made_from_the_three_properties_and_nothing_else)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const std::uint32_t   sent = rig.kmt().commands();
  const cl_sampler_properties unknown[] = {CL_SAMPLER_NORMALIZED_COORDS, CL_TRUE, CL_QUEUE_SIZE, 1u, 0u};
  cl_int                error = CL_SUCCESS;
  CHECK(table_of(run.context())->clCreateSamplerWithProperties(run.context(), unknown, &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_PROPERTY);
  CHECK(rig.root().create_sampler(reinterpret_cast<cl_context>(run.queue()), nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  CHECK_EQ(rig.kmt().commands(), sent);

  rig.host().cl_handle(0u, CL_INVALID_VALUE);
  CHECK(table_of(run.context())->clCreateSampler(run.context(), CL_TRUE, 0x1234u, CL_FILTER_LINEAR,
                                                                &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);

  rig.host().cl_handle(host_sampler, CL_SUCCESS);
  const std::uint32_t at = rig.kmt().commands();
  cl_sampler          sampler = table_of(run.context())->clCreateSampler(
      run.context(), CL_TRUE, CL_ADDRESS_REPEAT, CL_FILTER_LINEAR, &error);
  CHECK(sampler != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK_EQ(rig.kmt().command_id(at), av::clproto::kCmd_clCreateSamplerWithProperties);
  av::WireReader payload = payload_of(rig.kmt(), at);
  std::uint64_t  context = 0u, count = 0u, name = 0u, value = 0u;
  std::uint8_t   present = 0u;
  CHECK(payload.read_pod(context) && payload.read_pod(present) && present == 1u && payload.read_varint(count));
  CHECK_EQ(count, 6u);
  CHECK(payload.read_pod(name) && payload.read_pod(value));
  CHECK_EQ(name, static_cast<std::uint64_t>(CL_SAMPLER_NORMALIZED_COORDS));
  CHECK_EQ(value, static_cast<std::uint64_t>(CL_TRUE));

  const std::uint32_t set = rig.kmt().commands();
  CHECK_EQ(rig.root().set_kernel_argument(run.kernel(), 0u, sizeof sampler, &sampler), CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), set);

  CHECK_EQ(table_of(sampler)->clRetainSampler(sampler), CL_SUCCESS);
  CHECK_EQ(table_of(sampler)->clReleaseSampler(sampler), CL_SUCCESS);
  const std::uint32_t before = rig.kmt().commands();
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(table_of(sampler)->clReleaseSampler(sampler), CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), before + 2u);
  CHECK_EQ(rig.kmt().command_id(before), av::clproto::kCmd_clSetKernelArg);
  CHECK(carries(rig.kmt(), before, host_sampler));
  CHECK_EQ(rig.kmt().command_id(before + 1u), av::clproto::kCmd_clReleaseSampler);
  CHECK(carries(rig.kmt(), before + 1u, host_sampler));
  CHECK_EQ(rig.root().release(sampler, av::clicd::ClObject::Kind::sampler), CL_INVALID_SAMPLER);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_image_paths, a_buffer_copied_from_more_memory_than_one_command_carries_is_written_after_it)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const std::size_t bytes = 2u * write_room + 1u;
  static std::uint8_t from[2u * write_room + 1u];
  for (std::size_t index = 0u; index < bytes; ++index) from[index] = static_cast<std::uint8_t>(index);

  std::uint32_t sent = rig.kmt().commands();
  rig.host().cl_handle(host_buffer + 2u, CL_SUCCESS);
  rig.host().cl_handle(own_queue, CL_SUCCESS);
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_result(CL_SUCCESS);
  cl_int error = CL_INVALID_VALUE;
  cl_mem buffer = rig.root().create_buffer(run.context(), CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, bytes, from,
                                           nullptr, &error);
  CHECK(buffer != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent + 4u);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clCreateBuffer);
  av::WireReader payload = payload_of(rig.kmt(), sent);
  std::uint64_t  context = 0u, flags = 0u, size = 0u;
  std::uint8_t   present = 1u;
  CHECK(payload.read_pod(context) && payload.read_varint(flags) && payload.read_varint(size) &&
        payload.read_pod(present));
  CHECK_EQ(flags & CL_MEM_COPY_HOST_PTR, 0u);
  CHECK_EQ(size, bytes);
  CHECK_EQ(present, 0u);
  CHECK_EQ(rig.kmt().command_id(sent + 1u), av::clproto::kCmd_clCreateCommandQueueWithProperties);
  CHECK_EQ(rig.kmt().command_id(sent + 2u), av::clproto::kCmd_clEnqueueWriteBuffer);
  CHECK(carries(rig.kmt(), sent + 2u, own_queue));
  CHECK_EQ(rig.kmt().command_id(sent + 3u), av::clproto::kCmd_clReleaseCommandQueue);

  sent = rig.kmt().commands();
  rig.host().cl_handle(host_buffer + 3u, CL_SUCCESS);
  CHECK(rig.root().create_buffer(run.context(), CL_MEM_COPY_HOST_PTR, 64u, from, nullptr, &error) != nullptr);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);

  sent = rig.kmt().commands();
  rig.host().cl_handle(host_buffer + 4u, CL_SUCCESS);
  rig.host().cl_handle(own_queue, CL_SUCCESS);
  rig.host().cl_result(CL_MEM_OBJECT_ALLOCATION_FAILURE);
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_result(CL_SUCCESS);
  CHECK(rig.root().create_buffer(run.context(), CL_MEM_COPY_HOST_PTR, bytes, from, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_MEM_OBJECT_ALLOCATION_FAILURE);
  CHECK_EQ(rig.kmt().commands(), sent + 5u);
  CHECK_EQ(rig.kmt().command_id(sent + 4u), av::clproto::kCmd_clReleaseMemObject);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);

  rig.host().cl_handle(host_buffer + 5u, CL_SUCCESS);
  rig.host().cl_handle(0u, CL_OUT_OF_RESOURCES);
  rig.host().cl_result(CL_SUCCESS);
  CHECK(rig.root().create_buffer(run.context(), CL_MEM_COPY_HOST_PTR, bytes, from, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.kmt().command_id(rig.kmt().commands() - 1u), av::clproto::kCmd_clReleaseMemObject);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_image_paths, transfers_are_staged_while_buffers_fill_their_part_of_the_arena)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  av::clicd::GuestClContext *const on = static_cast<av::clicd::ClObject *>(static_cast<void *>(run.context()))->context();
  constexpr std::uint64_t page = av::clicd::ClArenaBlocks::alignment;
  std::uint64_t           low = 0u;
  std::uint64_t           high = av::clicd::GuestClContext::blocks_reserve / page + 1u;
  std::uint64_t           full = 0u;
  while (high - low > 1u) {
      const std::uint64_t middle = (low + high) / 2u;
      if (on->blocks().take(middle * page, full)) {
          on->blocks().give_back(full);
          low = middle;
      } else {
          high = middle;
      }
  }
  CHECK(low * page > av::clicd::GuestClContext::blocks_reserve / 2u);
  CHECK(on->blocks().take(low * page, full));
  std::uint64_t more = 0u;
  CHECK(!on->blocks().take(page, more));
  const std::size_t   bytes = 2u * write_room + 1u;
  static std::uint8_t from[2u * write_room + 1u];
  const std::uint32_t sent = rig.kmt().commands();
  rig.host().cl_handle(host_buffer + 2u, CL_SUCCESS);
  rig.host().cl_handle(own_queue, CL_SUCCESS);
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_result(CL_SUCCESS);
  cl_int error = CL_INVALID_VALUE;
  CHECK(rig.root().create_buffer(run.context(), CL_MEM_COPY_HOST_PTR, bytes, from, nullptr, &error) != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent + 4u);
  CHECK_EQ(rig.kmt().command_id(sent + 2u), av::clproto::kCmd_clEnqueueWriteBuffer);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  on->blocks().give_back(full); }

TEST(cl_image_paths, the_supported_image_formats_are_the_hosts_as_they_stand)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  cl_image_format formats[4] = {};
  cl_uint         listed = 0u;
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.root().supported_image_formats(reinterpret_cast<cl_context>(run.queue()), CL_MEM_READ_ONLY,
                                              CL_MEM_OBJECT_IMAGE2D, 4u, formats, &listed),
           CL_INVALID_CONTEXT);
  CHECK_EQ(rig.root().supported_image_formats(run.context(), CL_MEM_READ_ONLY, CL_MEM_OBJECT_IMAGE2D, 0u, formats,
                                              &listed),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().commands(), sent);

  const cl_image_format answered[2] = {{CL_RGBA, CL_UNORM_INT8}, {CL_R, CL_FLOAT}};
  rig.host().cl_formats(CL_SUCCESS, answered, 2u, 30u);
  CHECK_EQ(table_of(run.context())->clGetSupportedImageFormats(run.context(), CL_MEM_READ_ONLY,
                                                               CL_MEM_OBJECT_IMAGE2D, 2u, formats, &listed),
           CL_SUCCESS);
  CHECK_EQ(listed, 30u);
  CHECK_EQ(formats[0].image_channel_order, static_cast<cl_channel_order>(CL_RGBA));
  CHECK_EQ(formats[1].image_channel_data_type, static_cast<cl_channel_type>(CL_FLOAT));
  CHECK_EQ(formats[2].image_channel_order, 0u);

  rig.host().cl_formats(CL_SUCCESS, nullptr, 0u, 30u);
  listed = 0u;
  CHECK_EQ(rig.root().supported_image_formats(run.context(), CL_MEM_READ_ONLY, CL_MEM_OBJECT_IMAGE2D, 0u, nullptr,
                                              &listed),
           CL_SUCCESS);
  CHECK_EQ(listed, 30u); }

TEST(cl_image_paths, a_marker_or_a_barrier_names_the_hosts_events_and_hands_back_its_own)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  cl_event copied = nullptr;
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 7u);
  CHECK_EQ(rig.root().enqueue_copy_buffer(run.queue(), run.buffer(), run.buffer(), 0u, 128u, 64u, &copied),
           CL_SUCCESS);
  CHECK(copied != nullptr);

  std::uint32_t sent = rig.kmt().commands();
  cl_event      marker = nullptr;
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 8u);
  CHECK_EQ(table_of(run.queue())->clEnqueueMarkerWithWaitList(run.queue(), 1u, &copied, &marker), CL_SUCCESS);
  CHECK(marker != nullptr);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueMarkerWithWaitList);
  CHECK(carries(rig.kmt(), sent, host_event + 7u));

  sent = rig.kmt().commands();
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(table_of(run.queue())->clEnqueueBarrierWithWaitList(run.queue(), 0u, nullptr, nullptr), CL_SUCCESS);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueBarrierWithWaitList);

  cl_event refused = nullptr;
  rig.host().cl_enqueued(CL_INVALID_EVENT_WAIT_LIST, 0u);
  CHECK_EQ(rig.root().enqueue_barrier(run.queue(), 1u, &copied, &refused), CL_INVALID_EVENT_WAIT_LIST);
  CHECK(refused == nullptr);

  sent = rig.kmt().commands();
  CHECK_EQ(rig.root().enqueue_marker(reinterpret_cast<cl_command_queue>(run.context()), 0u, nullptr, nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().enqueue_marker(run.queue(), 1u, nullptr, nullptr), CL_INVALID_EVENT_WAIT_LIST);
  CHECK_EQ(rig.root().enqueue_marker(run.queue(), 0u, &copied, nullptr), CL_INVALID_EVENT_WAIT_LIST);
  const cl_event not_an_event = reinterpret_cast<cl_event>(run.kernel());
  CHECK_EQ(rig.root().enqueue_marker(run.queue(), 1u, &not_an_event, nullptr), CL_INVALID_EVENT_WAIT_LIST);
  cl_event many[av::clicd::ClHostNumbers::kept_inline + 1u] = {};
  for (cl_event &one : many) one = copied;
  rig.platform().fail_allocation_at(1u);
  CHECK_EQ(rig.root().enqueue_barrier(run.queue(), av::clicd::ClHostNumbers::kept_inline + 1u, many, nullptr),
           CL_OUT_OF_HOST_MEMORY);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_image_paths, an_event_of_another_context_is_not_one_a_queue_can_wait_on)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  rig.host().arena_import(0x79u, 3u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(host_context + 1u, CL_SUCCESS);
  cl_context other = rig.loader().create_context(run.device(), nullptr);
  CHECK(other != nullptr);
  if (other == nullptr) return;
  rig.host().cl_handle(host_event + 9u, CL_SUCCESS);
  cl_event foreign = rig.root().create_user_event(other, nullptr);
  CHECK(foreign != nullptr);
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.root().enqueue_marker(run.queue(), 1u, &foreign, nullptr), CL_INVALID_EVENT_WAIT_LIST);
  CHECK_EQ(rig.kmt().commands(), sent); }

namespace {
bool crossed_description(const av::MockKmt &kmt, std::uint32_t index, std::uint64_t &flags, cl_image_desc &crossed)
{
    av::WireReader  payload = payload_of(kmt, index);
    std::uint64_t   context = 0u;
    std::uint8_t    present = 0u;
    cl_image_format format = {};
    return kmt.command_id(index) == av::clproto::kCmd_clCreateImage && payload.read_pod(context) &&
           payload.read_varint(flags) && payload.read_pod(present) && present == 1u &&
           payload.read(&format, sizeof format) && payload.read_pod(present) && present == 1u &&
           payload.read(&crossed, sizeof crossed);
}

}

TEST(cl_image_paths, the_1_1_image_calls_make_the_image_clcreateimage_would_be_described)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const cl_icd_dispatch *const table = table_of(run.context());
  const cl_image_format        format = {CL_RGBA, CL_UNORM_INT8};
  std::uint64_t                flags = 0u;
  cl_image_desc                crossed = {};

  rig.host().cl_handle(host_image, CL_SUCCESS);
  std::uint32_t at = rig.kmt().commands();
  cl_int        error = CL_INVALID_VALUE;
  cl_mem        flat = table->clCreateImage2D(run.context(), CL_MEM_READ_ONLY, &format, 640u, 480u, 0u, nullptr, &error);
  CHECK(flat != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(table_of(flat) == table);
  CHECK(crossed_description(rig.kmt(), at, flags, crossed));
  CHECK_EQ(flags, static_cast<std::uint64_t>(CL_MEM_READ_ONLY));
  CHECK_EQ(crossed.image_type, static_cast<cl_mem_object_type>(CL_MEM_OBJECT_IMAGE2D));
  CHECK_EQ(crossed.image_width, 640u);
  CHECK_EQ(crossed.image_height, 480u);
  CHECK_EQ(crossed.image_depth, 0u);
  CHECK_EQ(crossed.image_row_pitch, 0u);

  rig.host().cl_handle(host_image + 1u, CL_SUCCESS);
  at = rig.kmt().commands();
  cl_mem deep = table->clCreateImage3D(run.context(), CL_MEM_READ_WRITE, &format, 32u, 16u, 8u, 0u, 0u, nullptr, &error);
  CHECK(deep != nullptr);
  CHECK(crossed_description(rig.kmt(), at, flags, crossed));
  CHECK_EQ(crossed.image_type, static_cast<cl_mem_object_type>(CL_MEM_OBJECT_IMAGE3D));
  CHECK_EQ(crossed.image_width, 32u);
  CHECK_EQ(crossed.image_height, 16u);
  CHECK_EQ(crossed.image_depth, 8u);

  at = rig.kmt().commands();
  std::uint8_t pixels[16] = {};
  CHECK(table->clCreateImage2D(run.context(), CL_MEM_READ_ONLY, &format, 2u, 2u, 0u, pixels, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_HOST_PTR);
  CHECK_EQ(rig.kmt().commands(), at);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_image_paths, a_buffer_rectangle_read_waits_for_its_queue_and_hands_back_its_rows)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const size_t        corner[3] = {4u, 2u, 0u}, at[3] = {0u, 0u, 0u}, region[3] = {8u, 4u, 1u};
  unsigned char       answered[32] = {};
  for (std::size_t index = 0u; index < sizeof answered; ++index) answered[index] = static_cast<unsigned char>(index + 1u);
  unsigned char       into[32] = {};
  run.leaves_work();
  run.drains();
  rig.host().cl_bytes(CL_SUCCESS, answered, sizeof answered);
  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  cl_event            event = nullptr;
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(table_of(run.queue())->clEnqueueReadBufferRect(run.queue(), run.buffer(), CL_FALSE, corner, at, region, 16u,
                                                          0u, 0u, 0u, into, 0u, nullptr, &event),
           CL_SUCCESS);
  for (std::size_t index = 0u; index < sizeof into; ++index) CHECK_EQ(into[index], answered[index]);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueMarkerWithWaitList);
  CHECK_EQ(rig.kmt().command_id(sent + 4u), av::clproto::kCmd_clEnqueueReadBufferRect);
  CHECK(carries(rig.kmt(), sent + 4u, host_buffer));
  CHECK_EQ(rig.kmt().command_id(sent + 5u), av::clproto::kCmd_clEnqueueMarkerWithWaitList);
  CHECK_EQ(rig.kmt().commands(), sent + 6u);
  CHECK(event != nullptr);

  run.leaves_work();
  run.drains();
  rig.host().cl_bytes(CL_INVALID_VALUE, answered, 0u);
  cl_event refused = nullptr;
  CHECK_EQ(rig.root().enqueue_read_buffer_rect(run.queue(), run.buffer(),
                                               ClBufferRect(corner, at, region, 16u, 0u, 0u, 0u), into, &refused),
           CL_INVALID_VALUE);
  CHECK(refused == nullptr);
  run.leaves_work();
  rig.host().cl_enqueued(CL_OUT_OF_RESOURCES, 0u);
  CHECK_EQ(rig.root().enqueue_read_buffer_rect(run.queue(), run.buffer(),
                                               ClBufferRect(corner, at, region, 16u, 0u, 0u, 0u), into, nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_image_paths, a_transfer_to_a_queue_known_idle_drains_nothing)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const size_t  corner[3] = {0u, 0u, 0u}, at[3] = {0u, 0u, 0u}, region[3] = {8u, 2u, 1u};
  unsigned char answered[16] = {};
  for (std::size_t index = 0u; index < sizeof answered; ++index) answered[index] = static_cast<unsigned char>(index + 7u);
  unsigned char into[16] = {};
  rig.host().cl_bytes(CL_SUCCESS, answered, sizeof answered);
  std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.root().enqueue_read_buffer_rect(run.queue(), run.buffer(),
                                               ClBufferRect(corner, at, region, 16u, 0u, 0u, 0u), into, nullptr),
           CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueReadBufferRect);
  for (std::size_t index = 0u; index < sizeof into; ++index) CHECK_EQ(into[index], answered[index]);
  rig.host().cl_result(CL_SUCCESS);
  sent = rig.kmt().commands();
  CHECK_EQ(rig.root().enqueue_write_buffer_rect(run.queue(), run.buffer(),
                                                ClBufferRect(corner, at, region, 16u, 0u, 0u, 0u), into, nullptr),
           CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueWriteBufferRect);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_image_paths, work_left_on_a_queue_is_drained_before_the_next_transfer_and_only_once)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const size_t  corner[3] = {0u, 0u, 0u}, at[3] = {0u, 0u, 0u}, region[3] = {8u, 2u, 1u};
  unsigned char answered[16] = {};
  unsigned char into[16] = {};
  run.leaves_work();
  rig.host().cl_enqueued(CL_OUT_OF_RESOURCES, 0u);
  CHECK_EQ(rig.root().enqueue_read_buffer_rect(run.queue(), run.buffer(),
                                               ClBufferRect(corner, at, region, 16u, 0u, 0u, 0u), into, nullptr),
           CL_OUT_OF_RESOURCES);
  run.drains();
  rig.host().cl_bytes(CL_SUCCESS, answered, sizeof answered);
  std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.root().enqueue_read_buffer_rect(run.queue(), run.buffer(),
                                               ClBufferRect(corner, at, region, 16u, 0u, 0u, 0u), into, nullptr),
           CL_SUCCESS);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueMarkerWithWaitList);
  CHECK_EQ(rig.kmt().command_id(rig.kmt().commands() - 1u), av::clproto::kCmd_clEnqueueReadBufferRect);
  rig.host().cl_bytes(CL_SUCCESS, answered, sizeof answered);
  sent = rig.kmt().commands();
  CHECK_EQ(rig.root().enqueue_read_buffer_rect(run.queue(), run.buffer(),
                                               ClBufferRect(corner, at, region, 16u, 0u, 0u, 0u), into, nullptr),
           CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_image_paths, a_buffer_rectangle_write_waits_for_its_queue_and_asks_for_an_event_only_when_wanted)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const size_t        corner[3] = {0u, 1u, 0u}, at[3] = {2u, 0u, 0u}, region[3] = {8u, 4u, 1u};
  unsigned char       from[64] = {};
  run.leaves_work();
  run.drains();
  rig.host().cl_result(CL_SUCCESS);
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(table_of(run.queue())->clEnqueueWriteBufferRect(run.queue(), run.buffer(), CL_TRUE, corner, at, region, 16u,
                                                           0u, 16u, 0u, from, 0u, nullptr, nullptr),
           CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent + 5u);
  CHECK_EQ(rig.kmt().command_id(sent + 4u), av::clproto::kCmd_clEnqueueWriteBufferRect);
  CHECK(carries(rig.kmt(), sent + 4u, host_buffer));

  run.leaves_work();
  run.drains();
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  cl_event event = nullptr;
  CHECK_EQ(rig.root().enqueue_write_buffer_rect(run.queue(), run.buffer(),
                                                ClBufferRect(corner, at, region, 16u, 0u, 16u, 0u), from, &event),
           CL_SUCCESS);
  CHECK(event != nullptr);

  run.leaves_work();
  run.drains();
  rig.host().cl_result(CL_INVALID_OPERATION);
  cl_event refused = nullptr;
  CHECK_EQ(rig.root().enqueue_write_buffer_rect(run.queue(), run.buffer(),
                                                ClBufferRect(corner, at, region, 16u, 0u, 16u, 0u), from, &refused),
           CL_INVALID_OPERATION);
  CHECK(refused == nullptr);
  run.leaves_work();
  rig.host().cl_enqueued(CL_OUT_OF_RESOURCES, 0u);
  CHECK_EQ(rig.root().enqueue_write_buffer_rect(run.queue(), run.buffer(),
                                                ClBufferRect(corner, at, region, 16u, 0u, 16u, 0u), from, nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_image_paths, a_buffer_rectangle_copy_is_enqueued_with_the_hosts_numbers_and_its_event)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  rig.host().cl_handle(host_buffer + 1u, CL_SUCCESS);
  cl_mem other = rig.root().create_buffer(run.context(), CL_MEM_READ_WRITE, 256u, nullptr, nullptr, nullptr);
  CHECK(other != nullptr);
  const size_t from_corner[3] = {1u, 1u, 0u}, to_corner[3] = {0u, 2u, 0u}, region[3] = {8u, 3u, 1u};

  std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(table_of(run.queue())->clEnqueueCopyBufferRect(run.queue(), run.buffer(), other, from_corner, to_corner,
                                                          region, 16u, 0u, 32u, 0u, 0u, nullptr, nullptr),
           CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent);

  cl_event event = nullptr;
  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  CHECK_EQ(rig.root().enqueue_copy_buffer_rect(run.queue(), run.buffer(), other, from_corner, to_corner, region, 16u,
                                               0u, 32u, 0u, &event),
           CL_SUCCESS);
  CHECK(event != nullptr);
  CHECK_EQ(rig.kmt().commands(), sent + 2u);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueCopyBufferRect);
  CHECK(carries(rig.kmt(), sent, host_buffer));
  CHECK(carries(rig.kmt(), sent, host_buffer + 1u));

  cl_event refused = nullptr;
  rig.host().cl_enqueued(CL_INVALID_VALUE, 0u);
  CHECK_EQ(rig.root().enqueue_copy_buffer_rect(run.queue(), run.buffer(), other, from_corner, to_corner, region, 4u,
                                               0u, 32u, 0u, &refused),
           CL_INVALID_VALUE);
  CHECK(refused == nullptr);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_image_paths, a_buffer_rectangle_the_api_would_refuse_asks_the_host_nothing)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const std::uint32_t    sent = rig.kmt().commands();
  const cl_mem           not_a_buffer = reinterpret_cast<cl_mem>(run.kernel());
  const cl_command_queue not_a_queue = reinterpret_cast<cl_command_queue>(run.context());
  const size_t           corner[3] = {0u, 0u, 0u}, region[3] = {8u, 2u, 1u};
  const ClBufferRect     rect(corner, corner, region, 0u, 0u, 0u, 0u);
  const ClBufferRect     too_narrow(corner, corner, region, 4u, 0u, 0u, 0u);
  unsigned char          memory[64] = {};
  CHECK_EQ(rig.root().enqueue_read_buffer_rect(not_a_queue, run.buffer(), rect, memory, nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().enqueue_read_buffer_rect(run.queue(), not_a_buffer, rect, memory, nullptr),
           CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.root().enqueue_read_buffer_rect(run.queue(), run.buffer(), rect, nullptr, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_read_buffer_rect(run.queue(), run.buffer(), too_narrow, memory, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_write_buffer_rect(not_a_queue, run.buffer(), rect, memory, nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().enqueue_write_buffer_rect(run.queue(), not_a_buffer, rect, memory, nullptr),
           CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.root().enqueue_write_buffer_rect(run.queue(), run.buffer(), rect, nullptr, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_write_buffer_rect(run.queue(), run.buffer(), too_narrow, memory, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_copy_buffer_rect(not_a_queue, run.buffer(), run.buffer(), corner, corner, region, 0u,
                                               0u, 0u, 0u, nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().enqueue_copy_buffer_rect(run.queue(), not_a_buffer, run.buffer(), corner, corner, region, 0u,
                                               0u, 0u, 0u, nullptr),
           CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.root().enqueue_copy_buffer_rect(run.queue(), run.buffer(), not_a_buffer, corner, corner, region, 0u,
                                               0u, 0u, 0u, nullptr),
           CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.root().enqueue_copy_buffer_rect(run.queue(), run.buffer(), run.buffer(), nullptr, corner, region, 0u,
                                               0u, 0u, 0u, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_copy_buffer_rect(run.queue(), run.buffer(), run.buffer(), corner, nullptr, region, 0u,
                                               0u, 0u, 0u, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_copy_buffer_rect(run.queue(), run.buffer(), run.buffer(), corner, corner, nullptr, 0u,
                                               0u, 0u, 0u, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().commands(), sent); }

TEST(cl_image_paths, an_image_copied_from_memory_whose_queue_the_host_does_not_make_is_given_back)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const cl_image_format format = {CL_RGBA, CL_UNORM_INT8};
  cl_image_desc         desc = {};
  desc.image_type = CL_MEM_OBJECT_IMAGE2D;
  desc.image_width = 4u;
  desc.image_height = 2u;
  std::uint32_t texels[8] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
  rig.host().cl_handle(host_image, CL_SUCCESS);
  run.element_is(4u);
  rig.host().cl_handle(0u, CL_SUCCESS);
  rig.host().cl_result(CL_SUCCESS);
  cl_int error = CL_SUCCESS;
  CHECK(rig.root().create_image(run.context(), CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, &format, &desc, texels,
                                &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }
