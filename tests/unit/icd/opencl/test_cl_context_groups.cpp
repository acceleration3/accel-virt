#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/kmt/pci_location.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/cl_icd_rig.hpp"
#include "icd/opencl/cl_context_group.hpp"
#include "icd/opencl/cl_icd_root.hpp"
#include "icd/opencl/cl_replicas.hpp"
#include "icd/opencl/guest_cl_context.hpp"
#include "proto/generated/opencl/cl_protocol_ids.h"
#include "proto/submission/command_header.hpp"

#include <cstdint>
#include <cstring>

using av::AdapterScenario;
using av::AvgpuAdapters;
using av::ClIcdRig;
using av::FakeClLoader;
using av::MockKmt;
using av::PciIdentity;
using av::PciLocation;
using av::clicd::ClContextGroup;
using av::clicd::ClIcdRoot;
using av::clicd::ClObject;
using av::clicd::GuestClContext;

namespace av::clicd {
struct ClIcdRootProbe {
    static GuestClContext *session_of(ClIcdRoot &root, const void *handle, ClObject::Kind kind)
    {
        ClObject *const held = root.judged(handle, kind);
        return held != nullptr ? held->context() : nullptr;
    }
    static GuestClContext *context(ClIcdRoot &root, const void *handle) { return session_of(root, handle, ClObject::Kind::context); }
    static std::uint32_t live_of(ClIcdRoot &root, ClObject::Kind kind) { return root.objects_.live_of(kind); }
    static std::uint64_t wire_of(ClIcdRoot &root, const void *handle, ClObject::Kind kind)
    {
        ClObject *const held = root.judged(handle, kind);
        return held != nullptr ? held->wire() : 0u;
    }
};
}

using av::clicd::ClIcdRootProbe;

namespace {

constexpr std::uint64_t host_platform = 0x6200u;
constexpr std::uint64_t host_device = 0x6300u;
constexpr std::uint64_t host_context = 0x6400u;
constexpr std::uint64_t host_queue = 0x6500u;
constexpr std::uint64_t host_buffer = 0x6600u;
constexpr std::uint64_t host_event = 0x6900u;
constexpr std::uint64_t second_luid = 0x0000000300005678ull;

const PciIdentity first_gpu(0x1002u, 0x7550u, PciIdentity::virtio_vendor, AvgpuAdapters::av_subsystem, 0xC0u);
const PciIdentity second_gpu(0x1002u, 0x13C0u, PciIdentity::virtio_vendor, AvgpuAdapters::av_subsystem, 0xC9u);

void two_gpus(ClIcdRig &rig)
{
    MockKmt &kmt = rig.kmt();
    kmt.set_identity(0u, first_gpu);
    kmt.describe_adapter(0u, PciLocation(3u, 0u, 0u), "AMD Radeon RX 9070 XT", 0x0023000000000000ull);
    kmt.add_adapter(second_gpu, AdapterScenario::wddm_3_2);
    kmt.set_adapter_luid(1u, second_luid);
    kmt.describe_adapter(1u, PciLocation(0x7Bu, 0u, 0u), "AMD Radeon(TM) Graphics", 0x0023000000000000ull);
}

void device_opens(ClIcdRig &rig)
{
    rig.host().arena_import(0x77u, 3u);
    rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
    rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
    rig.host().cl_info(CL_SUCCESS, "OpenCL 3.0 host", 16u);
    rig.host().cl_info(CL_SUCCESS, "OpenCL C 1.2", 13u);
    rig.host().cl_info(CL_SUCCESS, "", 1u);
}

void session_opens(ClIcdRig &rig, std::uint32_t slot, std::uint64_t context)
{
    rig.host().arena_import(slot, 3u);
    rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
    rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
    rig.host().cl_handle(context, CL_SUCCESS);
}

class Group {
public:
    explicit Group(ClIcdRig &rig) : rig_(rig), devices_{}, context_(nullptr), queues_{}
    {
        two_gpus(rig_);
        if (rig_.loader().load() != FakeClLoader::Load::kept) return;
        device_opens(rig_);
        device_opens(rig_);
        cl_uint count = 0u;
        if (rig_.loader().device_ids(CL_DEVICE_TYPE_GPU, 2u, devices_, &count) != CL_SUCCESS || count != 2u) return;
        session_opens(rig_, 0x78u, host_context);
        session_opens(rig_, 0x79u, host_context + 1u);
        cl_int error = CL_INVALID_VALUE;
        context_ = rig_.loader().dispatch_of(devices_[0])->clCreateContext(nullptr, 2u, devices_, nullptr, nullptr, &error);
        if (context_ == nullptr) return;
        for (std::uint32_t index = 0u; index < 2u; ++index) {
            rig_.host().cl_handle(host_queue + index, CL_SUCCESS);
            queues_[index] = rig_.loader().create_queue(context_, devices_[index], nullptr);
        }
    }

    bool             made() const { return queues_[0] != nullptr && queues_[1] != nullptr; }
    cl_device_id     device(std::uint32_t index) const { return devices_[index]; }
    cl_context       context() const { return context_; }
    cl_command_queue queue(std::uint32_t index) const { return queues_[index]; }
    const cl_device_id *devices() const { return devices_; }

private:
    ClIcdRig        &rig_;
    cl_device_id     devices_[2];
    cl_context       context_;
    cl_command_queue queues_[2];
};

std::uint32_t count_commands(const MockKmt &kmt, std::uint32_t from, std::uint32_t command)
{
    std::uint32_t counted = 0u;
    for (std::uint32_t index = from; index < kmt.commands(); ++index)
        if (kmt.command_id(index) == command) ++counted;
    return counted;
}

}

TEST(cl_context_groups, a_context_over_two_devices_is_one_session_per_device_and_a_queue_lives_in_its_devices_session)
{ ClIcdRig rig;
  Group    group(rig);
  CHECK(group.made());
  if (!group.made()) return;
  GuestClContext *const first = ClIcdRootProbe::session_of(rig.root(), group.queue(0u), ClObject::Kind::command_queue);
  GuestClContext *const second = ClIcdRootProbe::session_of(rig.root(), group.queue(1u), ClObject::Kind::command_queue);
  GuestClContext *const owner = ClIcdRootProbe::session_of(rig.root(), group.context(), ClObject::Kind::context);
  CHECK(first != nullptr && second != nullptr && first != second);
  CHECK(owner == first);
  CHECK(first->group() != nullptr && first->group() == second->group());
  CHECK_EQ(first->group()->count(), 2u);
  CHECK_EQ(first->group()->of(second)->host_context, host_context + 1u);

  cl_uint count = 0u;
  CHECK_EQ(rig.root().context_info(group.context(), CL_CONTEXT_NUM_DEVICES, sizeof count, &count, nullptr), CL_SUCCESS);
  CHECK_EQ(count, 2u);
  cl_device_id listed[2] = {};
  std::size_t  bytes = 0u;
  CHECK_EQ(rig.root().context_info(group.context(), CL_CONTEXT_DEVICES, sizeof listed, listed, &bytes), CL_SUCCESS);
  CHECK_EQ(bytes, sizeof listed);
  CHECK(listed[0] == group.device(0u) && listed[1] == group.device(1u));
  cl_device_id on = nullptr;
  CHECK_EQ(rig.root().queue_info(group.queue(1u), CL_QUEUE_DEVICE, sizeof on, &on, nullptr), CL_SUCCESS);
  CHECK(on == group.device(1u));
  cl_context owner_handle = nullptr;
  CHECK_EQ(rig.root().queue_info(group.queue(1u), CL_QUEUE_CONTEXT, sizeof owner_handle, &owner_handle, nullptr), CL_SUCCESS);
  CHECK(owner_handle == group.context());
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_context_groups, a_device_named_twice_or_none_is_refused_and_a_queue_needs_a_device_of_the_group)
{ ClIcdRig rig;
  Group    group(rig);
  CHECK(group.made());
  if (!group.made()) return;
  const cl_device_id twice[2] = {group.device(0u), group.device(0u)};
  cl_int             error = CL_SUCCESS;
  const std::uint32_t sent = rig.kmt().commands();
  CHECK(rig.root().create_context(nullptr, twice, 2u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  const cl_device_id stranger[2] = {group.device(0u), reinterpret_cast<cl_device_id>(&error)};
  CHECK(rig.root().create_context(nullptr, stranger, 2u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_DEVICE);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().create_queue(group.context(), reinterpret_cast<cl_device_id>(&error), nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_DEVICE);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_context_groups, a_second_session_that_cannot_open_gives_the_first_back_and_makes_no_context)
{ ClIcdRig rig;
  two_gpus(rig);
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  device_opens(rig);
  device_opens(rig);
  cl_device_id devices[2] = {};
  cl_uint      count = 0u;
  CHECK_EQ(rig.loader().device_ids(CL_DEVICE_TYPE_GPU, 2u, devices, &count), CL_SUCCESS);
  const std::uint32_t before = rig.kmt().open_objects();
  session_opens(rig, 0x78u, host_context);
  rig.host().arena_import(0x79u, 3u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(0u, CL_OUT_OF_RESOURCES);
  cl_int error = CL_SUCCESS;
  CHECK(rig.root().create_context(nullptr, devices, 2u, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.kmt().open_objects(), before);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_context_groups, releasing_the_context_and_its_queues_gives_every_session_back)
{ ClIcdRig rig;
  const std::uint32_t before = rig.kmt().open_objects();
  { Group group(rig);
    CHECK(group.made());
    if (!group.made()) return;
    CHECK(rig.kmt().open_objects() > before);
    CHECK_EQ(rig.loader().release_queue(group.queue(0u)), CL_SUCCESS);
    CHECK_EQ(rig.loader().release_queue(group.queue(1u)), CL_SUCCESS);
    CHECK_EQ(rig.loader().release_context(group.context()), CL_SUCCESS);
    CHECK_EQ(ClIcdRootProbe::live_of(rig.root(), ClObject::Kind::context), 0u); }
  CHECK(rig.root().invariants_hold()); }

namespace {

bool carries_handle(const MockKmt &kmt, std::uint32_t index, std::uint64_t handle)
{
    const std::uint8_t *const frame = kmt.command_bytes(index);
    if (frame == nullptr) return false;
    for (std::size_t at = av::CommandHeader::encoded_size; at + sizeof handle <= MockKmt::journal_bytes; ++at)
        if (std::memcmp(frame + at, &handle, sizeof handle) == 0) return true;
    return false;
}

bool carries_bytes(const MockKmt &kmt, std::uint32_t index, const std::uint8_t *bytes, std::size_t count)
{
    const std::uint8_t *const frame = kmt.command_bytes(index);
    if (frame == nullptr) return false;
    for (std::size_t at = av::CommandHeader::encoded_size; at + count <= MockKmt::journal_bytes; ++at)
        if (std::memcmp(frame + at, bytes, count) == 0) return true;
    return false;
}

std::uint32_t last_command(const MockKmt &kmt, std::uint32_t command)
{
    for (std::uint32_t index = kmt.commands(); index > 0u; --index)
        if (kmt.command_id(index - 1u) == command) return index - 1u;
    return ~std::uint32_t{0};
}

}

TEST(cl_context_groups, a_user_event_exists_in_every_session_and_a_wait_names_the_one_of_the_queues_session)
{ ClIcdRig rig;
  Group    group(rig);
  CHECK(group.made());
  if (!group.made()) return;
  const std::uint64_t gate = 0x6950u;
  rig.host().cl_handle(gate, CL_SUCCESS);
  rig.host().cl_handle(gate + 1u, CL_SUCCESS);
  cl_int         error = CL_INVALID_VALUE;
  const cl_event user = rig.loader().create_user_event(group.context(), &error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(user != nullptr);
  if (user == nullptr) return;
  CHECK_EQ(ClIcdRootProbe::wire_of(rig.root(), user, ClObject::Kind::event), gate);

  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  cl_event on_second = nullptr;
  CHECK_EQ(rig.root().enqueue_marker(group.queue(1u), 1u, &user, &on_second), CL_SUCCESS);
  const std::uint32_t marker = last_command(rig.kmt(), av::clproto::kCmd_clEnqueueMarkerWithWaitList);
  CHECK(carries_handle(rig.kmt(), marker, gate + 1u));
  CHECK(!carries_handle(rig.kmt(), marker, gate));

  rig.host().cl_enqueued(CL_SUCCESS, host_event + 1u);
  cl_event on_first = nullptr;
  CHECK_EQ(rig.root().enqueue_marker(group.queue(0u), 1u, &user, &on_first), CL_SUCCESS);
  const std::uint32_t other = last_command(rig.kmt(), av::clproto::kCmd_clEnqueueMarkerWithWaitList);
  CHECK(carries_handle(rig.kmt(), other, gate));
  CHECK(!carries_handle(rig.kmt(), other, gate + 1u));

  const std::uint32_t before = rig.kmt().commands();
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().set_user_event_status(user, CL_COMPLETE), CL_SUCCESS);
  CHECK_EQ(count_commands(rig.kmt(), before, av::clproto::kCmd_clSetUserEventStatus), 2u);
  CHECK_EQ(rig.root().set_user_event_status(user, CL_COMPLETE), CL_INVALID_OPERATION);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_context_groups, an_event_of_another_session_is_waited_for_in_the_guest_and_stood_in_for_by_a_completed_one)
{ ClIcdRig rig;
  Group    group(rig);
  CHECK(group.made());
  if (!group.made()) return;
  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  cl_event produced = nullptr;
  CHECK_EQ(rig.root().enqueue_marker(group.queue(0u), 0u, nullptr, &produced), CL_SUCCESS);
  CHECK(produced != nullptr);
  if (produced == nullptr) return;

  const std::uint64_t standing = 0x6960u;
  const std::int32_t  complete = CL_COMPLETE;
  rig.host().cl_handle(standing, CL_SUCCESS);
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 1u);
  cl_event after = nullptr;
  CHECK_EQ(rig.root().enqueue_marker(group.queue(1u), 1u, &produced, &after), CL_SUCCESS);
  const std::uint32_t marker = last_command(rig.kmt(), av::clproto::kCmd_clEnqueueMarkerWithWaitList);
  CHECK(carries_handle(rig.kmt(), marker, standing));
  CHECK(!carries_handle(rig.kmt(), marker, host_event));
  CHECK_EQ(rig.kmt().queued_answers(), 0u);

  const std::int32_t failed = -5;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&failed), sizeof failed);
  CHECK_EQ(rig.root().enqueue_marker(group.queue(1u), 1u, &produced, nullptr),
           CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_context_groups, a_buffer_nobody_has_used_moves_to_the_session_of_the_queue_that_uses_it_without_a_copy)
{ ClIcdRig rig;
  Group    group(rig);
  CHECK(group.made());
  if (!group.made()) return;
  GuestClContext *const first = ClIcdRootProbe::session_of(rig.root(), group.queue(0u), ClObject::Kind::command_queue);
  GuestClContext *const second = ClIcdRootProbe::session_of(rig.root(), group.queue(1u), ClObject::Kind::command_queue);
  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  cl_int       error = CL_INVALID_VALUE;
  const cl_mem buffer = rig.loader().create_buffer(group.context(), CL_MEM_READ_WRITE, 64u, &error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;
  CHECK(ClIcdRootProbe::session_of(rig.root(), buffer, ClObject::Kind::mem) == first);

  const std::uint32_t before = rig.kmt().commands();
  rig.host().cl_handle(host_buffer + 1u, CL_SUCCESS);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().enqueue_migrate(group.queue(1u), 1u, &buffer, 0u, nullptr), CL_SUCCESS);
  CHECK(ClIcdRootProbe::session_of(rig.root(), buffer, ClObject::Kind::mem) == second);
  CHECK_EQ(ClIcdRootProbe::wire_of(rig.root(), buffer, ClObject::Kind::mem), host_buffer + 1u);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK_EQ(count_commands(rig.kmt(), before, av::clproto::kCmd_clEnqueueReadBuffer), 0u);
  CHECK_EQ(count_commands(rig.kmt(), before, av::clproto::kCmd_clEnqueueWriteBuffer), 0u);

  const std::uint32_t after_move = rig.kmt().commands();
  CHECK_EQ(rig.root().enqueue_migrate(group.queue(1u), 1u, &buffer, 0u, nullptr), CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), after_move);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_context_groups, a_buffer_a_queue_has_used_is_read_out_of_its_session_and_written_into_the_next_before_it_moves)
{ ClIcdRig rig;
  Group    group(rig);
  CHECK(group.made());
  if (!group.made()) return;
  GuestClContext *const first = ClIcdRootProbe::session_of(rig.root(), group.queue(0u), ClObject::Kind::command_queue);
  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  cl_int       error = CL_INVALID_VALUE;
  const cl_mem buffer = rig.loader().create_buffer(group.context(), CL_MEM_READ_WRITE, 64u, &error);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;
  CHECK_EQ(rig.root().enqueue_migrate(group.queue(0u), 1u, &buffer, 0u, nullptr), CL_SUCCESS);

  const std::uint32_t before = rig.kmt().commands();
  const std::int32_t  complete = CL_COMPLETE;
  std::uint8_t        content[64] = {};
  for (std::size_t index = 0u; index < sizeof content; ++index) content[index] = static_cast<std::uint8_t>(0x30u + index);
  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_handle(host_buffer + 1u, CL_SUCCESS);
  rig.host().cl_handle(host_queue + 2u, CL_SUCCESS);
  rig.host().cl_bytes(CL_SUCCESS, content, sizeof content);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().enqueue_migrate(group.queue(1u), 1u, &buffer, 0u, nullptr), CL_SUCCESS);
  GuestClContext *const second = ClIcdRootProbe::session_of(rig.root(), group.queue(1u), ClObject::Kind::command_queue);
  CHECK(second->flush() == av::TransportResult::ok);
  CHECK(ClIcdRootProbe::session_of(rig.root(), buffer, ClObject::Kind::mem) !=
        first);
  CHECK_EQ(count_commands(rig.kmt(), before, av::clproto::kCmd_clEnqueueReadBuffer), 1u);
  CHECK_EQ(count_commands(rig.kmt(), before, av::clproto::kCmd_clEnqueueWriteBuffer), 1u);
  const std::uint32_t read = last_command(rig.kmt(), av::clproto::kCmd_clEnqueueReadBuffer);
  const std::uint32_t write = last_command(rig.kmt(), av::clproto::kCmd_clEnqueueWriteBuffer);
  CHECK(read < write);
  CHECK(carries_handle(rig.kmt(), read, host_buffer));
  CHECK(carries_handle(rig.kmt(), write, host_buffer + 1u));
  CHECK(carries_bytes(rig.kmt(), write, content, 32u));
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }


TEST(cl_context_groups, a_program_is_made_and_built_in_every_session_and_a_kernel_follows_its_queue_with_its_arguments)
{ ClIcdRig rig;
  Group    group(rig);
  CHECK(group.made());
  if (!group.made()) return;
  const std::uint64_t host_program = 0x6700u;
  const std::uint64_t host_kernel = 0x6800u;
  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  cl_int       error = CL_INVALID_VALUE;
  const cl_mem buffer = rig.loader().create_buffer(group.context(), CL_MEM_READ_WRITE, 64u, &error);
  CHECK(buffer != nullptr);
  const std::uint32_t before_program = rig.kmt().commands();
  rig.host().cl_handle(host_program, CL_SUCCESS);
  rig.host().cl_handle(host_program + 1u, CL_SUCCESS);
  const cl_program program = rig.loader().create_program(group.context(), "__kernel void k(__global int *d, int n){}", &error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(program != nullptr);
  if (buffer == nullptr || program == nullptr) return;
  CHECK_EQ(count_commands(rig.kmt(), before_program, av::clproto::kCmd_clCreateProgramWithSource), 2u);

  const std::uint32_t before_build = rig.kmt().commands();
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.loader().build_program(program, nullptr), CL_SUCCESS);
  CHECK_EQ(count_commands(rig.kmt(), before_build, av::clproto::kCmd_clBuildProgram), 2u);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);

  rig.host().cl_handle(host_kernel, CL_SUCCESS);
  rig.host().cl_kernel_undescribed();
  const cl_kernel kernel = rig.loader().create_kernel(program, "k", &error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(kernel != nullptr);
  if (kernel == nullptr) return;
  const std::uint32_t before_args = rig.kmt().commands();
  const cl_int        count = 9;
  CHECK_EQ(rig.root().set_kernel_argument(kernel, 0u, sizeof buffer, &buffer), CL_SUCCESS);
  CHECK_EQ(rig.root().set_kernel_argument(kernel, 1u, sizeof count, &count), CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), before_args);

  GuestClContext *const first = ClIcdRootProbe::session_of(rig.root(), group.queue(0u), ClObject::Kind::command_queue);
  GuestClContext *const second = ClIcdRootProbe::session_of(rig.root(), group.queue(1u), ClObject::Kind::command_queue);
  CHECK(ClIcdRootProbe::session_of(rig.root(), kernel, ClObject::Kind::kernel) == first);
  const std::size_t one = 1u;
  rig.host().cl_info(CL_SUCCESS, "k", 2u);
  rig.host().cl_handle(host_kernel + 1u, CL_SUCCESS);
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_handle(host_buffer + 1u, CL_SUCCESS);
  rig.host().cl_result(CL_SUCCESS);
  const std::uint32_t before_run = rig.kmt().commands();
  CHECK_EQ(rig.root().enqueue_kernel(group.queue(1u), kernel, 1u, nullptr, &one, nullptr, nullptr), CL_SUCCESS);
  CHECK(ClIcdRootProbe::session_of(rig.root(), kernel, ClObject::Kind::kernel) == second);
  CHECK(ClIcdRootProbe::session_of(rig.root(), buffer, ClObject::Kind::mem) == second);
  CHECK_EQ(ClIcdRootProbe::wire_of(rig.root(), kernel, ClObject::Kind::kernel), host_kernel + 1u);
  CHECK(second->flush() == av::TransportResult::ok);
  CHECK_EQ(count_commands(rig.kmt(), before_run, av::clproto::kCmd_clCreateKernel), 1u);
  CHECK_EQ(count_commands(rig.kmt(), before_run, av::clproto::kCmd_clSetKernelArg), 2u);
  CHECK_EQ(count_commands(rig.kmt(), before_run, av::clproto::kCmd_clEnqueueNDRangeKernel), 1u);
  const std::uint32_t set_buffer = last_command(rig.kmt(), av::clproto::kCmd_clSetKernelArg) - 1u;
  CHECK(carries_handle(rig.kmt(), set_buffer, host_buffer + 1u));
  CHECK_EQ(rig.kmt().queued_answers(), 0u);

  const std::uint32_t before_again = rig.kmt().commands();
  CHECK_EQ(rig.root().enqueue_kernel(group.queue(1u), kernel, 1u, nullptr, &one, nullptr, nullptr), CL_SUCCESS);
  CHECK(second->flush() == av::TransportResult::ok);
  CHECK_EQ(count_commands(rig.kmt(), before_again, av::clproto::kCmd_clCreateKernel), 0u);
  CHECK_EQ(count_commands(rig.kmt(), before_again, av::clproto::kCmd_clSetKernelArg), 2u);
  CHECK(rig.root().invariants_hold()); }
