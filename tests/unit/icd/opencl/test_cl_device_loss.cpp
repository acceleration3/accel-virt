/* Device loss: a transport that faults loses its cl_context, and with it every
 * other context on the same device (feature 050's policy, which IcdConnection
 * carries for both ICDs).
 *
 * What an OpenCL application is owed is different from what a GL one is: there
 * is no reset status and no sticky error, so every entry on a lost context
 * answers CL_OUT_OF_RESOURCES, immediately, without reaching the Kmt at all.
 * The application is still holding real objects - the spec has no "lost
 * context" state - so releasing them has to work, and has to cost no round
 * trip on a transport that is gone.
 *
 * A context made after the fault opens a new device beside the lost one, which
 * is what lets a process recover without restarting. */
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/cl_icd_rig.hpp"

#include <CL/cl_ext.h>

#include <cstdint>
#include <cstring>

using av::ClIcdRig;
using av::FakeClLoader;
using av::MockKmt;
using av::clicd::ClObject;

namespace {

constexpr std::uint64_t host_platform = 0x6200u;
constexpr std::uint64_t host_device = 0x6300u;
constexpr std::uint64_t host_context = 0x6400u;
constexpr std::uint64_t host_queue = 0x6500u;
constexpr std::uint64_t host_buffer = 0x6600u;

/* What the device answers while a cl_context is being made: the arena's
 * import, the platform and the device the context asks for again, then the
 * host's own context. */
void context_opens(ClIcdRig &rig)
{
    rig.host().blob_import(0u, 0x77u);
    rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
    rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
    rig.host().cl_handle(host_context, CL_SUCCESS);
}

cl_device_id opened_device(ClIcdRig &rig)
{
    if (rig.loader().load() != FakeClLoader::Load::kept) return nullptr;
    rig.host().blob_import(0u, 0x77u);
    rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
    rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
    rig.host().cl_info(CL_SUCCESS, "OpenCL 3.0 host", 16u);
    rig.host().cl_info(CL_SUCCESS, "OpenCL C 1.2", 13u);
    rig.host().cl_info(CL_SUCCESS, "", 1u);
    cl_device_id device = nullptr;
    cl_uint      devices = 0u;
    return rig.loader().device_ids(CL_DEVICE_TYPE_GPU, 1u, &device, &devices) == CL_SUCCESS
               ? device
               : nullptr;
}

cl_context made_context(ClIcdRig &rig, cl_device_id device)
{
    context_opens(rig);
    return rig.loader().create_context(device, nullptr);
}

/* The next wait this context makes finds nothing, which is how a context the
 * host faulted looks to its process. */
void fault_next(ClIcdRig &rig) { rig.kmt().withhold_answer_at(1u); }

}  // namespace

TEST(cl_device_loss, a_fault_loses_every_context_on_the_device_and_none_of_them_sends_again)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  CHECK(device != nullptr);
  cl_context first = made_context(rig, device);
  cl_context second = made_context(rig, device);
  CHECK(first != nullptr);
  CHECK(second != nullptr);

  /* The first context's next call faults its transport. */
  fault_next(rig);
  cl_int error = CL_SUCCESS;
  CHECK(rig.loader().create_queue(first, device, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);

  /* From here neither context reaches the Kmt: the one that faulted, and the
   * one that merely shared its device. */
  const std::uint32_t commands = rig.kmt().commands();
  CHECK(rig.loader().create_queue(first, device, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  CHECK(rig.loader().create_buffer(second, CL_MEM_READ_WRITE, 64u, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  CHECK(rig.loader().create_program(second, "__kernel void k(void) {}", &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.kmt().commands(), commands);
  CHECK(rig.root().invariants_hold()); }

namespace {

struct Told {
    std::uint32_t times = 0u;
    bool          named_the_loss = false;
    bool          private_info = false;
};

void told(const char *text, const void *info, size_t bytes, void *user_data)
{
    Told *const record = static_cast<Told *>(user_data);
    record->times++;
    record->named_the_loss = text != nullptr && std::strstr(text, "lost") != nullptr;
    record->private_info = info != nullptr || bytes != 0u;
}

}  // namespace

/* The callback clCreateContext was given is told once, by the call that finds
 * the context lost - a host GPU fault, the device the context ran on lost - and
 * not again; a context that registered none is told nothing. */
TEST(cl_device_loss, a_lost_context_tells_its_callback_once)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  CHECK(device != nullptr);
  Told       record;
  context_opens(rig);
  cl_context context = rig.loader().create_context(device, &told, &record, nullptr);
  CHECK(context != nullptr);
  CHECK_EQ(record.times, 0u);
  /* The call the fault met fails; the next finds the context lost, and
   * tells. */
  fault_next(rig);
  cl_int error = CL_SUCCESS;
  CHECK(rig.loader().create_user_event(context, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  CHECK(rig.loader().create_user_event(context, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  CHECK_EQ(record.times, 1u);
  CHECK(record.named_the_loss);
  CHECK(!record.private_info);
  CHECK(rig.loader().create_user_event(context, &error) == nullptr);
  CHECK_EQ(record.times, 1u);
  CHECK_EQ(rig.loader().release_context(context), CL_SUCCESS);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_device_loss, releasing_a_lost_context_costs_no_round_trip_and_gives_everything_back)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  CHECK(device != nullptr);
  cl_context context = made_context(rig, device);
  CHECK(context != nullptr);
  /* An object made before the fault is still the application's, and it still
   * has to be releasable afterwards - OpenCL has no lost-context state to
   * excuse a refusal. */
  rig.host().cl_handle(host_queue, CL_SUCCESS);
  cl_command_queue queue = rig.loader().create_queue(context, device, nullptr);
  CHECK(queue != nullptr);

  /* A call that always crosses: a finish on a queue that has run nothing
   * sends nothing, and would never meet the fault. */
  fault_next(rig);
  cl_int error = CL_SUCCESS;
  CHECK(rig.loader().create_user_event(context, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.loader().finish(queue), CL_OUT_OF_RESOURCES);

  const std::uint32_t commands = rig.kmt().commands();
  CHECK_EQ(rig.loader().release_queue(queue), CL_SUCCESS);
  CHECK_EQ(rig.loader().release_context(context), CL_SUCCESS);
  /* Nothing was told: there is nothing to tell it over. */
  CHECK_EQ(rig.kmt().commands(), commands);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_device_loss, a_context_made_after_the_fault_opens_a_new_device_beside_the_lost_one)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  CHECK(device != nullptr);
  cl_context lost = made_context(rig, device);
  CHECK(lost != nullptr);
  fault_next(rig);
  cl_int error = CL_SUCCESS;
  CHECK(rig.loader().create_queue(lost, device, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  /* The call that faulted is not the call that notices: the transport failed
   * it, and the next one through finds the transport faulted and loses the
   * device. Until then the link is still one a new context could join. */
  CHECK(rig.loader().create_queue(lost, device, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);

  /* A second D3DKMT device, opened because the first is no longer one new
   * contexts may join. */
  const std::uint32_t devices = rig.kmt().calls(MockKmt::Call::create_device);
  cl_context fresh = made_context(rig, device);
  CHECK(fresh != nullptr);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_device), devices + 1u);

  /* And it works: the lost one's neighbours are gone, this one's are not. */
  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  CHECK(rig.loader().create_buffer(fresh, CL_MEM_READ_WRITE, 64u, &error) != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_device_loss, the_device_and_the_platform_answer_after_a_fault_because_neither_is_a_context)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  CHECK(device != nullptr);
  cl_context context = made_context(rig, device);
  CHECK(context != nullptr);
  fault_next(rig);
  cl_int error = CL_SUCCESS;
  CHECK(rig.loader().create_queue(context, device, &error) == nullptr);

  /* The four the ICD decides are the adapter's, not the host's, so a fault
   * does not take them: an application enumerating after a loss still finds
   * this driver. */
  cl_uint platforms = 0u;
  CHECK_EQ(rig.root().platform_ids(0u, nullptr, &platforms), CL_SUCCESS);
  CHECK_EQ(platforms, 1u);
  std::uint64_t luid = 0u;
  std::size_t   answered = 0u;
  CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_LUID_KHR, sizeof luid, &luid, &answered),
           CL_SUCCESS);
  CHECK_EQ(luid, ClIcdRig::adapter_luid);
  CHECK(rig.root().invariants_hold()); }

/* Every entry that needs a transport, on a context that has none.
 *
 * The spec has no "lost context", so there is no status an application can
 * ask for and no sticky error to find. What it gets instead is the same
 * refusal from every call, immediately, with nothing sent - which is the only
 * honest answer once the transport is gone, and the only one that cannot hang.
 *
 * Every one of these is a distinct path in the ICD: each entry reaches for its
 * context's transport separately, and an entry that forgot to would send into
 * a torn-down ring. */
TEST(cl_device_loss, every_entry_on_a_lost_context_refuses_at_once_and_sends_nothing)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  CHECK(device != nullptr);
  cl_context context = made_context(rig, device);
  CHECK(context != nullptr);
  if (context == nullptr) return;

  /* Real objects, made while the context still worked: the application is
   * still holding them after the fault, and every one of them is asked. */
  rig.host().cl_handle(host_queue, CL_SUCCESS);
  cl_command_queue queue = rig.loader().create_queue(context, device, nullptr);
  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  cl_mem buffer = rig.loader().create_buffer(context, CL_MEM_READ_WRITE, 256u, nullptr);
  rig.host().cl_handle(0x6700u, CL_SUCCESS);
  cl_program program = rig.loader().create_program(context, "__kernel void k(void) {}", nullptr);
  CHECK(queue != nullptr && buffer != nullptr && program != nullptr);
  if (queue == nullptr || buffer == nullptr || program == nullptr) return;
  rig.host().cl_handle(0x6800u, CL_SUCCESS);
  cl_kernel kernel = rig.loader().create_kernel(program, "k", nullptr);
  rig.host().cl_handle(0x6901u, CL_SUCCESS);
  cl_event user = rig.loader().create_user_event(context, nullptr);
  CHECK(kernel != nullptr && user != nullptr);
  if (kernel == nullptr || user == nullptr) return;

  /* The fault, on a call that always crosses - a finish on a queue that has
   * run nothing sends nothing. From here the transport is gone. */
  fault_next(rig);
  cl_int error = CL_SUCCESS;
  CHECK(rig.loader().create_user_event(context, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);

  const std::uint32_t sent = rig.kmt().commands();
  unsigned char       bytes[8] = {};
  const std::size_t   global = 1u;

  CHECK_EQ(rig.loader().finish(queue), CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().flush(queue), CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().enqueue_read(queue, buffer, CL_TRUE, 0u, sizeof bytes, bytes, nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().enqueue_read(queue, buffer, CL_FALSE, 0u, sizeof bytes, bytes, nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().enqueue_write(queue, buffer, CL_TRUE, 0u, sizeof bytes, bytes, nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().enqueue_kernel(queue, kernel, 1u, nullptr, &global, nullptr, nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().set_kernel_argument(kernel, 0u, sizeof buffer, &buffer),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().build_program(program, 0u, nullptr, nullptr), CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().wait_for_events(1u, &user), CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().event_info(user, CL_EVENT_COMMAND_EXECUTION_STATUS, 0u, nullptr, nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().set_user_event_status(user, CL_COMPLETE), CL_OUT_OF_RESOURCES);

  /* The map answers null with the same refusal, and the unmap of a pointer
   * no live mapping answered is still the caller's own mistake. */
  error = CL_SUCCESS;
  CHECK(rig.root().map_buffer(queue, buffer, CL_TRUE, CL_MAP_READ, 0u, sizeof bytes, nullptr,
                              &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().unmap(queue, buffer, bytes, nullptr), CL_INVALID_VALUE);

  /* The shared-memory entries too: an allocation cannot be made and an
   * address inside none is refused before anything is sent. */
  CHECK(rig.root().svm_alloc(context, CL_MEM_READ_WRITE, 256u, 0u) == nullptr);
  CHECK_EQ(rig.root().svm_map(queue, CL_TRUE, CL_MAP_READ, bytes, sizeof bytes, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.root().svm_unmap(queue, bytes, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().set_kernel_svm_argument(kernel, 0u, bytes), CL_INVALID_ARG_VALUE);

  /* A kernel made on a program of a lost context, and a user event made on
   * the context itself: both need the transport that is gone. */
  CHECK(rig.loader().create_kernel(program, "k", &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  error = CL_SUCCESS;
  CHECK(rig.loader().create_user_event(context, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  /* And registering a callback on an event of it. */
  CHECK_EQ(rig.root().watch_event(user, CL_COMPLETE, nullptr, nullptr), CL_OUT_OF_RESOURCES);

  /* Not one of them reached the driver. */
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }
