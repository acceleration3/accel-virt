/* Images, samplers, the device-side copies and fills, and what a program and a
 * kernel say about themselves: the entries the OpenCL samples reach beyond a
 * buffer and a kernel.
 *
 * What is asserted is what an application would see - the object it is handed,
 * the error it is told, the bytes it reads - and, where the ICD decides it,
 * what crossed: the host's number for an object rather than the guest's
 * pointer, a transfer that waited for its queue before asking the host to
 * block, a queue of the ICD's own that went again. */
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/cl_icd_rig.hpp"
#include "harness/icd/fake_cl_loader.hpp"
#include "icd/opencl/cl_icd_root.hpp"
#include "proto/generated/opencl/cl_protocol_ids.h"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ClIcdRig;
using av::FakeClLoader;

namespace {

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

/* A context, a queue, a buffer, a program and a kernel: everything the entries
 * below act on, so each test is about the entry and not the setup. */
class Running {
public:
    explicit Running(ClIcdRig &rig) : rig_(rig)
    {
        if (rig_.loader().load() != FakeClLoader::Load::kept) return;
        rig_.host().blob_import(0u, 0x77u);
        rig_.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
        rig_.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
        rig_.host().cl_info(CL_SUCCESS, "OpenCL 3.0 host", 16u);
        rig_.host().cl_info(CL_SUCCESS, "OpenCL C 1.2", 13u);
        rig_.host().cl_info(CL_SUCCESS, "", 1u);
        cl_uint devices = 0u;
        if (rig_.loader().device_ids(CL_DEVICE_TYPE_GPU, 1u, &device_, &devices) != CL_SUCCESS) return;
        rig_.host().blob_import(0u, 0x78u);
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

    /* An image the host made as `host_image`. */
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

    /* What a drain of the queue answers: the marker, the flush, a complete
     * status, and the marker given back. */
    void drains()
    {
        rig_.host().cl_enqueued(CL_SUCCESS, host_event);
        rig_.host().cl_result(CL_SUCCESS);
        const cl_int complete = CL_COMPLETE;
        rig_.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
        rig_.host().cl_result(CL_SUCCESS);
    }

    /* CL_IMAGE_ELEMENT_SIZE's answer. */
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

/* What the loader dispatches through: the table in the object's first word. */
const cl_icd_dispatch *table_of(const void *object)
{
    return object != nullptr ? *reinterpret_cast<const cl_icd_dispatch *const *>(object) : nullptr;
}

/* The payload of the command at `index` of the journal, past its header. */
av::WireReader payload_of(const av::MockKmt &kmt, std::uint32_t index)
{
    return av::WireReader(kmt.command_bytes(index) + av::CommandHeader::encoded_size,
                          av::MockKmt::journal_bytes - av::CommandHeader::encoded_size);
}

/* Whether `handle` appears, as eight little-endian bytes, anywhere in the
 * command at `index`: how a test sees which number crossed for an object. */
bool carries(const av::MockKmt &kmt, std::uint32_t index, std::uint64_t handle)
{
    const std::uint8_t *const frame = kmt.command_bytes(index);
    if (frame == nullptr) return false;
    for (std::size_t at = av::CommandHeader::encoded_size; at + sizeof handle <= av::MockKmt::journal_bytes; ++at)
        if (std::memcmp(frame + at, &handle, sizeof handle) == 0) return true;
    return false;
}

}  // namespace

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
  /* The host's program and the host's device, not the guest's pointers. */
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

  /* Asked for no event: the host is asked for none. */
  std::uint32_t sent = rig.kmt().commands();
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(table_of(run.queue())->clEnqueueCopyBuffer(run.queue(), run.buffer(), other, 4u, 8u, 64u, 0u,
                                                                    nullptr, nullptr),
           CL_SUCCESS);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueCopyBuffer);
  CHECK(carries(rig.kmt(), sent, host_buffer));
  CHECK(carries(rig.kmt(), sent, host_buffer + 1u));

  /* Asked for one: the application is handed an object of its own. */
  cl_event event = nullptr;
  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  CHECK_EQ(rig.root().enqueue_copy_buffer(run.queue(), run.buffer(), other, 0u, 0u, 64u, &event), CL_SUCCESS);
  CHECK(event != nullptr);
  CHECK(table_of(event) != nullptr);

  const std::uint32_t pattern = 0xDEADBEEFu;
  sent = rig.kmt().commands();
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(table_of(run.queue())->clEnqueueFillBuffer(run.queue(), run.buffer(), &pattern,
                                                                    sizeof pattern, 0u, 256u, 0u, nullptr, nullptr),
           CL_SUCCESS);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueFillBuffer);
  cl_event filled = nullptr;
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 1u);
  CHECK_EQ(rig.root().enqueue_fill_buffer(run.queue(), run.buffer(), &pattern, sizeof pattern, 0u, 256u, &filled),
           CL_SUCCESS);
  CHECK(filled != nullptr);

  /* The host's refusal is the application's, and no event is made of it. */
  cl_event refused = nullptr;
  rig.host().cl_enqueued(CL_INVALID_VALUE, 0u);
  CHECK_EQ(rig.root().enqueue_copy_buffer(run.queue(), run.buffer(), other, 250u, 0u, 64u, &refused),
           CL_INVALID_VALUE);
  CHECK(refused == nullptr);
  rig.host().cl_enqueued(CL_INVALID_VALUE, 0u);
  CHECK_EQ(rig.root().enqueue_fill_buffer(run.queue(), run.buffer(), &pattern, 3u, 0u, 256u, &refused),
           CL_INVALID_VALUE);
  CHECK(refused == nullptr);
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

  /* An image over a buffer: the description the host decodes names the
   * host's buffer, in the field the application wrote its pointer in. */
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
  /* The application's own description is untouched. */
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
  /* Memory the application keeps is refused, and the log says why. */
  CHECK(rig.root().create_image(run.context(), CL_MEM_USE_HOST_PTR, &format, &desc, texels, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK_EQ(rig.log().holding("not carried"), 1u);
  /* A pointer exactly when the flags say to copy from one. */
  CHECK(rig.root().create_image(run.context(), CL_MEM_COPY_HOST_PTR, &format, &desc, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_HOST_PTR);
  CHECK(rig.root().create_image(run.context(), 0u, &format, &desc, texels, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_HOST_PTR);
  /* A buffer the description names has to be one. */
  desc.buffer = reinterpret_cast<cl_mem>(run.kernel());
  CHECK(rig.root().create_image(run.context(), 0u, &format, &desc, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_IMAGE_DESCRIPTOR);
  CHECK_EQ(rig.kmt().commands(), sent);

  /* And the host's own refusal is the application's. */
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
  /* The write is on the ICD's queue and carries the image's bytes. */
  CHECK(carries(rig.kmt(), sent + 3u, own_queue));
  CHECK(carries(rig.kmt(), sent + 3u, host_image));
  CHECK(carries(rig.kmt(), sent + 3u, 0x0000000200000001ull));
  /* The create asked the host to copy nothing: the bytes follow it. */
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

  /* The host would not say how large a texel is. */
  std::uint32_t sent = rig.kmt().commands();
  rig.host().cl_handle(host_image, CL_SUCCESS);
  rig.host().cl_info(CL_INVALID_MEM_OBJECT, "", 0u);
  rig.host().cl_result(CL_SUCCESS);
  CHECK(rig.root().create_image(run.context(), CL_MEM_COPY_HOST_PTR, &format, &desc, texels, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_IMAGE_DESCRIPTOR);
  CHECK_EQ(rig.kmt().command_id(rig.kmt().commands() - 1u), av::clproto::kCmd_clReleaseMemObject);
  CHECK(carries(rig.kmt(), rig.kmt().commands() - 1u, host_image));

  /* The host would not make the queue. */
  rig.host().cl_handle(host_image, CL_SUCCESS);
  run.element_is(4u);
  rig.host().cl_handle(0u, CL_OUT_OF_HOST_MEMORY);
  rig.host().cl_result(CL_SUCCESS);
  CHECK(rig.root().create_image(run.context(), CL_MEM_COPY_HOST_PTR, &format, &desc, texels, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_HOST_MEMORY);
  CHECK_EQ(rig.kmt().command_id(rig.kmt().commands() - 1u), av::clproto::kCmd_clReleaseMemObject);

  /* The host refused the write: the queue goes, then the image. */
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

  /* A 1D array's layout is not rows and slices, so its bytes are not carried. */
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
  /* The rows each layout's bytes are written as: one, a height, and a height
   * of a slice or a layer. */
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
      /* A write per slice, each of the layout's rows. */
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
  /* The size, the drain, the one read, and the marker the event is. */
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

  /* The host's refusal of the write is the application's, and makes no event. */
  run.element_is(4u);
  run.drains();
  rig.host().cl_result(CL_INVALID_OPERATION);
  cl_event event = nullptr;
  CHECK_EQ(rig.root().enqueue_write_image(run.queue(), image, origin, region, 0u, 0u, texels, &event),
           CL_INVALID_OPERATION);
  CHECK(event == nullptr);

  /* A drain the host refuses ends the transfer before it starts. */
  run.element_is(4u);
  rig.host().cl_enqueued(CL_OUT_OF_RESOURCES, 0u);
  CHECK_EQ(rig.root().enqueue_write_image(run.queue(), image, origin, region, 0u, 0u, texels, nullptr),
           CL_OUT_OF_RESOURCES);
  run.element_is(4u);
  rig.host().cl_enqueued(CL_OUT_OF_RESOURCES, 0u);
  CHECK_EQ(rig.root().enqueue_read_image(run.queue(), image, origin, region, 0u, 0u, texels, nullptr),
           CL_OUT_OF_RESOURCES);
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
  /* A region of no rows, and a pitch too small for its row: the size is asked
   * and nothing after it. */
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

  /* The 1.2 call is the same list, written out. */
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
  /* Three pairs; the terminator is the wire's to add back, not to carry. */
  CHECK_EQ(count, 6u);
  CHECK(payload.read_pod(name) && payload.read_pod(value));
  CHECK_EQ(name, static_cast<std::uint64_t>(CL_SAMPLER_NORMALIZED_COORDS));
  CHECK_EQ(value, static_cast<std::uint64_t>(CL_TRUE));

  /* An argument the size of a sampler that is one crosses as the host's
   * number for it. */
  rig.host().cl_result(CL_SUCCESS);
  const std::uint32_t set = rig.kmt().commands();
  CHECK_EQ(rig.root().set_kernel_argument(run.kernel(), 0u, sizeof sampler, &sampler), CL_SUCCESS);
  CHECK(carries(rig.kmt(), set, host_sampler));

  /* Retained once, released twice: the host is told at the second. */
  CHECK_EQ(table_of(sampler)->clRetainSampler(sampler), CL_SUCCESS);
  CHECK_EQ(table_of(sampler)->clReleaseSampler(sampler), CL_SUCCESS);
  const std::uint32_t before = rig.kmt().commands();
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(table_of(sampler)->clReleaseSampler(sampler), CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), before + 1u);
  CHECK_EQ(rig.kmt().command_id(before), av::clproto::kCmd_clReleaseSampler);
  CHECK(carries(rig.kmt(), before, host_sampler));
  CHECK_EQ(rig.root().release(sampler, av::clicd::ClObject::Kind::sampler), CL_INVALID_SAMPLER);
  CHECK(rig.root().invariants_hold()); }
