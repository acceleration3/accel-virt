/* One cl_context and the rvk context that carries it.
 *
 * Two things matter here and the rest follows from them. Nothing reaches the
 * device before the context is made, and a make that fails holds nothing -
 * which is what stops a process that failed to open a device from leaking a
 * link, a staging block and a ring for every context an application tries.
 * And `wire()` is the one accessor a call goes through, so a faulted transport
 * becomes a lost context there rather than wherever somebody remembered to
 * look. */
#include "harness/framework/test_registry.hpp"
#include "harness/icd/cl_context_rig.hpp"
#include "icd/opencl/guest_cl_context.hpp"
#include "proto/generated/opencl/cl_dispatch_client.h"

#include <cstdint>

using av::ClContextRig;
using av::MockKmt;
using av::Status;
using av::clicd::GuestClContext;

namespace {

GuestClContext made_context(ClContextRig &rig)
{
    return GuestClContext(rig.connection(), rig.platform(), rig.send_lock(), rig.log());
}

/* What the device answers the context's blob arena. A make asks exactly once,
 * and a host that answers nothing faults the transport rather than declining -
 * which is a different outcome and has its own case below. */
void arena_answers(ClContextRig &rig, std::uint32_t status = 0u)
{
    rig.host().blob_import(status, 0x77u);
}

}  // namespace

TEST(guest_cl_context, a_new_context_has_touched_nothing)
{ ClContextRig rig;
  GuestClContext context = made_context(rig);
  CHECK(context.state() == GuestClContext::State::created);
  CHECK(context.wire() == nullptr);
  CHECK(!context.lost());
  /* Not one D3DKMT call: an application that makes a cl_context and then
   * fails to use it costs a process nothing. */
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::enumerate_adapters), 0u);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK(context.invariants_hold()); }

TEST(guest_cl_context, making_it_opens_the_adapter_the_device_and_the_transport)
{ ClContextRig rig;
  arena_answers(rig);
  GuestClContext context = made_context(rig);
  CHECK_STATUS(context.make(), Status::ok);
  CHECK(context.state() == GuestClContext::State::made);
  CHECK(context.wire() != nullptr);
  CHECK(rig.kmt().open_objects() > ClContextRig::link_objects);
  CHECK(context.invariants_hold()); }

TEST(guest_cl_context, a_second_make_is_refused_and_changes_nothing)
{ ClContextRig rig;
  arena_answers(rig);
  GuestClContext context = made_context(rig);
  CHECK_STATUS(context.make(), Status::ok);
  const std::uint32_t objects = rig.kmt().open_objects();
  CHECK_STATUS(context.make(), Status::invalid_state);
  CHECK_EQ(rig.kmt().open_objects(), objects);
  CHECK(context.invariants_hold()); }

TEST(guest_cl_context, with_no_rvkgpu_adapter_it_is_not_supported_and_holds_nothing)
{ av::MockPlatform platform;
  MockKmt kmt;
  av::ScriptedHost host(kmt);
  av::LockTrace trace;
  av::RecordingThreadLock send_lock(trace, 'S');
  av::RecordingLog log;
  av::icd::IcdConnection connection(kmt);
  /* No adapter was added: what a process on a machine with no rvkgpu sees. */
  GuestClContext context(connection, platform, send_lock, log);
  CHECK_STATUS(context.make(), Status::not_supported);
  CHECK(context.state() == GuestClContext::State::created);
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(platform.live_allocations(), 0u);
  CHECK(context.invariants_hold()); }

TEST(guest_cl_context, a_context_with_no_memory_for_its_staging_holds_nothing)
{ ClContextRig rig;
  GuestClContext context = made_context(rig);
  rig.platform().fail_allocation_at(1u);
  CHECK_STATUS(context.make(), Status::no_memory);
  CHECK(context.state() == GuestClContext::State::created);
  /* The link it took on the way in went back with everything else. */
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.platform().live_allocations(), 0u);
  CHECK(context.invariants_hold()); }

TEST(guest_cl_context, a_context_whose_transport_faulted_opening_its_arena_is_not_made)
{ ClContextRig rig;
  /* No answer is queued, so the arena's import waits on a host that says
   * nothing and the transport latches faulted. A context whose every call is
   * already lost is not a context to hand back. */
  GuestClContext context = made_context(rig);
  CHECK_STATUS(context.make(), Status::invalid_state);
  CHECK(context.state() == GuestClContext::State::created);
  CHECK(rig.log().holding("faulted") > 0u);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.platform().live_allocations(), 0u);
  CHECK(context.invariants_hold()); }

TEST(guest_cl_context, a_device_that_will_not_import_leaves_the_context_usable)
{ ClContextRig rig;
  /* The device answered, and said no. Bulk data crosses inline and every
   * other call is unaffected - which is the whole difference from the case
   * above. */
  arena_answers(rig, 1u);
  GuestClContext context = made_context(rig);
  CHECK_STATUS(context.make(), Status::ok);
  CHECK(context.wire() != nullptr);
  CHECK(!context.has_arena());
  CHECK(rig.log().holding("crosses inline") > 0u);
  CHECK(context.invariants_hold()); }

TEST(guest_cl_context, a_faulted_transport_makes_the_context_lost_where_a_call_asks)
{ ClContextRig rig;
  arena_answers(rig);
  GuestClContext context = made_context(rig);
  CHECK_STATUS(context.make(), Status::ok);
  CHECK(context.wire() != nullptr);
  CHECK(!context.lost());

  /* The host stops answering. The next round trip through the transport
   * faults it, and `wire()` is where the context finds out. */
  rig.kmt().withhold_answer_at(1u);
  /* Any round trip will do; a flush is the smallest one an application makes. */
  static_cast<void>(av::clproto::rvk_clFlush(*context.wire(), 0u));

  CHECK(context.wire() == nullptr);
  CHECK(context.lost());
  CHECK(context.state() == GuestClContext::State::lost);
  CHECK(rig.log().holding("lost") > 0u);
  CHECK(context.invariants_hold()); }

TEST(guest_cl_context, the_svm_table_belongs_to_the_context_and_goes_with_it)
{ ClContextRig rig;
  arena_answers(rig);
  { GuestClContext context = made_context(rig);
    CHECK_STATUS(context.make(), Status::ok);
    CHECK(context.svm().take(0x7F00'0000'1000ull, 128u) != nullptr);
    CHECK_EQ(context.svm().live(), 1u);
    CHECK(context.invariants_hold()); }
  /* The mirrors were the context's to free. */
  CHECK_EQ(rig.platform().live_allocations(), 0u); }

TEST(guest_cl_context, tearing_down_gives_back_every_object_it_opened)
{ ClContextRig rig;
  arena_answers(rig);
  { GuestClContext context = made_context(rig);
    CHECK_STATUS(context.make(), Status::ok);
    CHECK(rig.kmt().open_objects() > 0u); }
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.platform().live_allocations(), 0u); }
