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

/* ---- mappings, and the invariant that keeps them apart ------------------- */

namespace av::clicd {

/* The states a context is never in. Two live mappings answering one pointer
 * would leave an unmap choosing between them; a made context missing any of
 * the four things a make holds is one whose teardown would not know what to
 * give back; and a context that has made nothing holding any of them is a
 * make that failed halfway and said it had not. */
struct GuestClContextProbe {
    static MappedRegion &mapping(GuestClContext &context, uint32_t slot)
    { return context.mappings_[slot]; }
    /* Answers the block it took away, so a test can put it back: the
     * destructor is what frees it, and a context that has forgotten it leaks. */
    static uint8_t      *forget_staging(GuestClContext &context)
    { uint8_t *const held = context.staging_; context.staging_ = nullptr; return held; }
    static void          remember_staging(GuestClContext &context, uint8_t *staging)
    { context.staging_ = staging; }
    static void          forget_link(GuestClContext &context)
    { context.link_ = icd::IcdConnection::no_link; }
    static void          say_it_was_made(GuestClContext &context)
    { context.state_ = GuestClContext::State::made; }
    static void          say_it_was_never_made(GuestClContext &context)
    { context.state_ = GuestClContext::State::created; }
    /* The lock around the transport, taken away and put back: it owns
     * nothing, so a context without it leaks nothing - it is simply a
     * teardown that stopped halfway. */
    static void          drop_the_lock(GuestClContext &context) { context.locked_.destroy(); }
    static void          restore_the_lock(GuestClContext &context)
    { static_cast<void>(context.locked_.construct(context.transport_.object(),
                                                  context.send_lock_)); }
    /* A staging block and a link on a context that says it has made nothing.
     * Neither points at anything, so nothing is freed or released. */
    static void          stray_staging(GuestClContext &context, uint8_t *block)
    { context.staging_ = block; }
    static void          stray_link(GuestClContext &context) { context.link_ = 0u; }
    static SvmTable     &svm_of(GuestClContext &context) { return context.svm_; }
};

}  // namespace av::clicd

namespace av::clicd {

/* The shared allocations the context holds: a count that disagrees with the
 * rows is a slot leaked or handed out twice. */
struct SvmTableProbe {
    static void miscount(SvmTable &table) { table.live_ += 1u; }
};

}  // namespace av::clicd

using av::clicd::GuestClContextProbe;
using av::clicd::MappedRegion;

TEST(guest_cl_context, a_mapping_of_nothing_is_not_taken_by_either_kind)
{ ClContextRig   rig;
  arena_answers(rig);
  GuestClContext context = made_context(rig);
  CHECK_STATUS(context.make(), Status::ok);
  std::uint8_t at[16] = {};
  /* No buffer, no host address, no length: each is a row that could never be
   * unmapped, and neither kind of mapping takes one. */
  CHECK(context.take_mapping(0u, 0x7000u, 0u, sizeof at, CL_MAP_READ) == nullptr);
  CHECK(context.take_mapping(0x6600u, 0u, 0u, sizeof at, CL_MAP_READ) == nullptr);
  CHECK(context.take_mapping(0x6600u, 0x7000u, 0u, 0u, CL_MAP_READ) == nullptr);
  CHECK(context.take_shared_mapping(0u, 0x7000u, 0u, sizeof at, at) == nullptr);
  CHECK(context.take_shared_mapping(0x6600u, 0u, 0u, sizeof at, at) == nullptr);
  CHECK(context.take_shared_mapping(0x6600u, 0x7000u, 0u, 0u, at) == nullptr);
  /* And a shared mapping with no memory behind it. */
  CHECK(context.take_shared_mapping(0x6600u, 0x7000u, 0u, sizeof at, nullptr) == nullptr);
  CHECK_EQ(context.mappings(), 0u);
  CHECK(context.invariants_hold()); }

TEST(guest_cl_context, the_mapping_table_fills_and_then_refuses_both_kinds)
{ ClContextRig   rig;
  arena_answers(rig);
  GuestClContext context = made_context(rig);
  CHECK_STATUS(context.make(), Status::ok);
  std::uint8_t at[GuestClContext::max_mappings][16] = {};
  for (std::uint32_t index = 0u; index < GuestClContext::max_mappings; ++index) {
      CHECK(context.take_shared_mapping(0x6600u + index, 0x7000u + index, 0u, 16u, at[index]) !=
            nullptr);
  }
  CHECK_EQ(context.mappings(), GuestClContext::max_mappings);
  /* Full: the application is told the map failed rather than being handed a
   * mapping the context cannot remember. */
  std::uint8_t one_more[16] = {};
  CHECK(context.take_shared_mapping(0x7700u, 0x8800u, 0u, 16u, one_more) == nullptr);
  CHECK(context.take_mapping(0x7700u, 0x8800u, 0u, 16u, CL_MAP_READ) == nullptr);
  CHECK_EQ(context.mappings(), GuestClContext::max_mappings);
  CHECK(context.invariants_hold()); }

TEST(guest_cl_context, a_staged_mapping_with_no_memory_for_its_block_is_not_taken)
{ ClContextRig   rig;
  arena_answers(rig);
  GuestClContext context = made_context(rig);
  CHECK_STATUS(context.make(), Status::ok);
  /* The slot was free and the block was not: nothing is kept, so the slot is
   * still free for the next map. */
  /* The next allocation, whatever the make already did: the counter is
   * reset when the refusal is armed. */
  rig.platform().fail_allocation_at(1u);
  CHECK(context.take_mapping(0x6600u, 0x7000u, 0u, 16u, CL_MAP_READ) == nullptr);
  CHECK_EQ(context.mappings(), 0u);
  CHECK(context.invariants_hold()); }

TEST(guest_cl_context, the_invariant_catches_two_mappings_answering_one_pointer)
{ ClContextRig   rig;
  arena_answers(rig);
  GuestClContext context = made_context(rig);
  CHECK_STATUS(context.make(), Status::ok);
  std::uint8_t at[16] = {};
  std::uint8_t other[16] = {};
  CHECK(context.take_shared_mapping(0x6600u, 0x7000u, 0u, sizeof at, at) != nullptr);
  CHECK(context.take_shared_mapping(0x6601u, 0x7001u, 0u, sizeof other, other) != nullptr);
  CHECK(context.invariants_hold());
  /* Both answering one address: an unmap would have two to choose between,
   * and choosing is what it must never do. */
  GuestClContextProbe::mapping(context, 1u).take(0x6601u, 0x7001u, 0u, sizeof at, 0u, at, false);
  CHECK(!context.invariants_hold());
  /* Put it back so teardown gives back what it really holds. */
  GuestClContextProbe::mapping(context, 1u).take(0x6601u, 0x7001u, 0u, sizeof other, 0u, other,
                                                 false);
  CHECK(context.invariants_hold()); }

TEST(guest_cl_context, the_invariant_catches_a_made_context_that_is_missing_what_a_make_holds)
{ { ClContextRig   rig;
    arena_answers(rig);
    GuestClContext context = made_context(rig);
    CHECK_STATUS(context.make(), Status::ok);
    CHECK(context.invariants_hold());
    /* Made, but with no staging block: the transport has nowhere to put a
     * reply, and teardown would give back nothing. */
    std::uint8_t *const staging = GuestClContextProbe::forget_staging(context);
    CHECK(!context.invariants_hold());
    /* Put it back: the destructor is what frees it. */
    GuestClContextProbe::remember_staging(context, staging);
    CHECK(context.invariants_hold()); }
  { ClContextRig   rig;
    arena_answers(rig);
    GuestClContext context = made_context(rig);
    CHECK_STATUS(context.make(), Status::ok);
    /* Made, but holding no link: the adapter it opened is one nothing names. */
    GuestClContextProbe::forget_link(context);
    CHECK(!context.invariants_hold()); }
  { ClContextRig   rig;
    GuestClContext context = made_context(rig);
    /* And the mirror: it says it was made and has none of it. */
    CHECK(context.invariants_hold());
    GuestClContextProbe::say_it_was_made(context);
    CHECK(!context.invariants_hold()); } }

/* Whether the context got an arena at all is what says which path its buffers
 * and its bulk data take, so both answers have to be reachable and both have
 * to be right. */
TEST(guest_cl_context, a_context_says_whether_it_got_an_arena)
{ { ClContextRig rig;
    GuestClContext context = made_context(rig);
    /* Before a make there is no arena, because there is no transport to
     * import one through. */
    CHECK(!context.has_arena()); }
  { ClContextRig rig;
    arena_answers(rig);
    GuestClContext context = made_context(rig);
    CHECK_STATUS(context.make(), Status::ok);
    CHECK(context.has_arena()); }
  { ClContextRig rig;
    /* A device that will not import leaves the context perfectly usable and
     * without one: bulk data crosses inline and buffers are staged. */
    arena_answers(rig, 1u);
    GuestClContext context = made_context(rig);
    CHECK_STATUS(context.make(), Status::ok);
    CHECK(!context.has_arena()); } }

/* The invariant reaches what the context owns, not only its own fields: a
 * broken allocation or a broken mapping is a broken context. */
TEST(guest_cl_context, the_invariant_reaches_the_allocations_and_the_mappings_it_holds)
{ ClContextRig rig;
  arena_answers(rig);
  GuestClContext context = made_context(rig);
  CHECK_STATUS(context.make(), Status::ok);
  std::uint8_t at[16] = {};
  CHECK(context.take_shared_mapping(0x6600u, 0x7000u, 0u, sizeof at, at) != nullptr);
  CHECK(context.invariants_hold());
  /* A mapping that is held but names no buffer could never be unmapped. */
  GuestClContextProbe::mapping(context, 0u).take(0u, 0x7000u, 0u, sizeof at, 0u, at, false);
  CHECK(!context.invariants_hold());
  GuestClContextProbe::mapping(context, 0u).take(0x6600u, 0x7000u, 0u, sizeof at, 0u, at, false);
  CHECK(context.invariants_hold()); }

/* A context that says it was never made must hold none of what a make holds:
 * each of the four is checked on its own, because one left behind is a make
 * that failed halfway and reported that it had not started. */
TEST(guest_cl_context, the_invariant_catches_an_unmade_context_still_holding_something)
{ ClContextRig rig;
  arena_answers(rig);
  GuestClContext context = made_context(rig);
  CHECK_STATUS(context.make(), Status::ok);
  /* Everything is held, and the state is put back to created: every one of
   * the four terms is false, which is what the created branch demands. */
  GuestClContextProbe::say_it_was_never_made(context);
  CHECK(!context.invariants_hold());
  GuestClContextProbe::say_it_was_made(context);
  CHECK(context.invariants_hold()); }

/* The transport and the lock around it are constructed together, so a context
 * holding one without the other is a make or a teardown that stopped halfway
 * - in either direction. */
TEST(guest_cl_context, the_invariant_catches_a_transport_without_the_lock_around_it)
{ ClContextRig rig;
  arena_answers(rig);
  GuestClContext context = made_context(rig);
  CHECK_STATUS(context.make(), Status::ok);
  CHECK(context.invariants_hold());
  GuestClContextProbe::drop_the_lock(context);
  CHECK(!context.invariants_hold());
  GuestClContextProbe::restore_the_lock(context);
  CHECK(context.invariants_hold()); }

/* A context that says it has made nothing must hold none of what a make
 * holds: each is checked on its own, because the first term to fail hides
 * the rest. */
TEST(guest_cl_context, the_invariant_catches_each_thing_an_unmade_context_should_not_hold)
{ { ClContextRig rig;
    GuestClContext context = made_context(rig);
    CHECK(context.invariants_hold());
    std::uint8_t block = 0u;
    GuestClContextProbe::stray_staging(context, &block);
    CHECK(!context.invariants_hold());
    GuestClContextProbe::stray_staging(context, nullptr);
    CHECK(context.invariants_hold()); }
  { ClContextRig rig;
    GuestClContext context = made_context(rig);
    /* A link it never acquired: the adapter would be held open by a context
     * that believes it opened nothing. */
    GuestClContextProbe::stray_link(context);
    CHECK(!context.invariants_hold()); } }

/* The shared allocations are the context's, so a table that is unsound makes
 * the context unsound: an address translating two ways is not something the
 * context may believe it is fine with. */
TEST(guest_cl_context, the_invariant_reaches_the_shared_allocations_it_holds)
{ ClContextRig rig;
  arena_answers(rig);
  GuestClContext context = made_context(rig);
  CHECK_STATUS(context.make(), Status::ok);
  CHECK(context.invariants_hold());
  av::clicd::SvmTableProbe::miscount(GuestClContextProbe::svm_of(context));
  CHECK(!context.invariants_hold()); }

/* A driver that will not make the context's own transport.
 *
 * The transport is what every call of that cl_context goes over, so a context
 * without one is a context whose every call would fail - and the application
 * is told the make failed rather than handed it. Whatever was taken before
 * the refusal goes back, which the rig's object count is what proves. */
TEST(guest_cl_context, a_driver_that_refuses_the_contexts_transport_makes_no_context)
{ ClContextRig rig;
  GuestClContext context = made_context(rig);
  /* The driver refuses to make the context the transport needs. */
  rig.kmt().fail_at(av::MockKmt::Call::create_context_virtual, 1u, 0xC0000001u);
  CHECK(context.make() != Status::ok);
  CHECK(context.state() == GuestClContext::State::created);
  CHECK(context.wire() == nullptr);
  CHECK_EQ(rig.log().holding("the driver refused the context"), 1u);
  /* Nothing is held: the adapter and the device it opened are given back. */
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK(context.invariants_hold()); }
