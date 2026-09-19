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

  fault_next(rig);
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
