#include "harness/framework/test_registry.hpp"
#include "proto/generated/opencl/cl_protocol_ids.h"
#include "icd/opencl/unanswered_transport.hpp"
#include "icd/common/opened_objects.hpp"
#include "harness/icd/context_link_probe.hpp"
#include "harness/icd/host_channel_probe.hpp"
#include "core/log/keyed_log.hpp"
#include "harness/icd/cl_context_rig.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/client/client_transport.hpp"
#include "guest/transport/guest_connection.hpp"
#include "icd/common/device_link.hpp"
#include "icd/common/shared_handles.hpp"
#include "icd/opencl/cl_property_list.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "proto/resources/shared_object_record.hpp"
#include "proto/resources/surface_layout.hpp"
#include "proto/submission/command_header.hpp"
#include "icd/opencl/d3d11_resource.hpp"
#include "icd/opencl/guest_cl_context.hpp"
#include "proto/generated/opencl/cl_dispatch_client.h"

#include <cstddef>
#include <cstdint>

using av::ClContextRig;
using av::MockKmt;
using av::Status;
using av::clicd::GuestClContext;

namespace av::icd {
struct OpenedObjectsProbe {
    static void add_empty(OpenedObjects &opened)
    {
        std::uint32_t slot = 0u;
        CHECK_STATUS(opened.opens_[0].insert(0x5151u, OpenedObjectRow{}, slot), Status::ok);
    }
    static void clear(OpenedObjects &opened) { opened.opens_[0].clear(); }
};

}

namespace av::clicd {

struct ClExternalMemoryProbe {
    static void add_empty(ClExternalMemory &memory) { icd::OpenedObjectsProbe::add_empty(memory.opened_); }
    static void clear(ClExternalMemory &memory) { icd::OpenedObjectsProbe::clear(memory.opened_); }
};

}

namespace {

GuestClContext made_context(ClContextRig &rig)
{
    return GuestClContext(rig.connection(), rig.platform(), rig.send_lock(), rig.blocks_lock(), rig.lines(),
                          rig.handles());
}

void arena_answers(ClContextRig &rig, std::uint32_t status = 0u)
{
    rig.host().blob_import(status, 0x77u);
    if (status != 0u) rig.host().blob_import(status, 0x77u);
}

}

TEST(guest_cl_context, a_new_context_has_touched_nothing)
{ ClContextRig rig;
  GuestClContext context = made_context(rig);
  CHECK(context.state() == GuestClContext::State::created);
  CHECK(context.wire() == nullptr);
  CHECK(!context.lost());
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

TEST(guest_cl_context, a_context_going_releases_the_shared_objects_it_still_holds)
{ ClContextRig rig;
  arena_answers(rig);
  std::uint32_t adapter = 0u;
  std::uint32_t found = 0u;
  CHECK_STATUS(rig.kmt().enumerate_adapters(&adapter, 1u, found), Status::ok);
  std::uint32_t elsewhere = 0u;
  CHECK_STATUS(rig.kmt().create_device(adapter, elsewhere), Status::ok);
  const av::AllocationDescriptor carrier(av::AllocationKind::client, true, av::SurfaceLayout(), 4096u,
                                         av::ScanoutTarget());
  std::uint8_t bytes[av::AllocationDescriptor::encoded_size + av::SharedObjectRecord::encoded_size] = {};
  CHECK_STATUS(carrier.encode(bytes, av::AllocationDescriptor::encoded_size), Status::ok);
  const av::SharedObjectRecord asking(0u, av::SharedObjectKind(av::SharedObjectKind::memory), 0u, 4096u);
  CHECK_STATUS(asking.encode(bytes + av::AllocationDescriptor::encoded_size,
                             av::SharedObjectRecord::encoded_size),
               Status::ok);
  std::uint32_t allocation = 0u;
  CHECK_STATUS(rig.kmt().create_shared_allocation(elsewhere, bytes, sizeof bytes, allocation), Status::ok);
  std::uint64_t handle = 0u;
  CHECK_STATUS(rig.kmt().share_object(elsewhere, allocation, handle), Status::ok);
  const std::uint64_t properties[] = {0x2061u, handle, 0u};

  std::uint32_t sent = 0u;
  { GuestClContext context = made_context(rig);
    CHECK_STATUS(context.make(), Status::ok);
    av::clicd::ClPropertyList list;
    CHECK(list.read(properties));
    rig.host().shared_object(0u, 0x9001u);
    std::uint64_t made = 0u;
    CHECK_EQ(context.external().open(*context.wire(), context.guest_device(), list, 4096u, made),
             CL_SUCCESS);
    CHECK_EQ(made, 0x9001u);
    CHECK_EQ(context.external().live(), 1u);
    CHECK(context.invariants_hold());
    sent = rig.kmt().commands();
    rig.host().shared_object(0u, 0u);
  }
  bool released = false;
  for (std::uint32_t index = sent; index < rig.kmt().commands(); index++)
      released = released || rig.kmt().command_id(index) == av::CommandHeader::release_shared_object_id;
  CHECK(released); }

TEST(guest_cl_context, an_import_goes_behind_the_calls_the_context_holds)
{ ClContextRig rig;
  arena_answers(rig);
  std::uint32_t adapter = 0u;
  std::uint32_t found = 0u;
  CHECK_STATUS(rig.kmt().enumerate_adapters(&adapter, 1u, found), Status::ok);
  std::uint32_t elsewhere = 0u;
  CHECK_STATUS(rig.kmt().create_device(adapter, elsewhere), Status::ok);
  const av::AllocationDescriptor carrier(av::AllocationKind::client, true, av::SurfaceLayout(), 4096u,
                                         av::ScanoutTarget());
  std::uint8_t bytes[av::AllocationDescriptor::encoded_size + av::SharedObjectRecord::encoded_size] = {};
  CHECK_STATUS(carrier.encode(bytes, av::AllocationDescriptor::encoded_size), Status::ok);
  const av::SharedObjectRecord asking(0u, av::SharedObjectKind(av::SharedObjectKind::memory), 0u, 4096u);
  CHECK_STATUS(asking.encode(bytes + av::AllocationDescriptor::encoded_size,
                             av::SharedObjectRecord::encoded_size),
               Status::ok);
  std::uint32_t allocation = 0u;
  CHECK_STATUS(rig.kmt().create_shared_allocation(elsewhere, bytes, sizeof bytes, allocation), Status::ok);
  std::uint64_t handle = 0u;
  CHECK_STATUS(rig.kmt().share_object(elsewhere, allocation, handle), Status::ok);
  const std::uint64_t properties[] = {0x2061u, handle, 0u};
  GuestClContext      context = made_context(rig);
  CHECK_STATUS(context.make(), Status::ok);
  const std::uint32_t before = rig.kmt().commands();
  av::icd::UnansweredTransport unanswered(*context.wire());
  CHECK_EQ(av::clproto::av_clReleaseKernel(unanswered, 0x4242u), CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), before);
  av::clicd::ClPropertyList list;
  CHECK(list.read(properties));
  rig.host().shared_object(0u, 0x9001u);
  std::uint64_t made = 0u;
  CHECK_EQ(context.external().open(*context.wire(), context.guest_device(), list, 4096u, made), CL_SUCCESS);
  CHECK_EQ(rig.kmt().command_id(before), av::clproto::kCmd_clReleaseKernel);
  CHECK_EQ(rig.kmt().command_id(before + 1u), av::CommandHeader::open_shared_object_id);
  rig.host().shared_object(0u, 0u); }

TEST(guest_cl_context, a_shared_object_table_that_cannot_be_true_makes_the_context_untrue)
{ ClContextRig rig;
  GuestClContext context = made_context(rig);
  CHECK(context.invariants_hold());
  av::clicd::ClExternalMemoryProbe::add_empty(context.external());
  CHECK(!context.invariants_hold());
  av::clicd::ClExternalMemoryProbe::clear(context.external());
  CHECK(context.invariants_hold()); }

TEST(guest_cl_context, it_names_the_device_its_transport_runs_on_once_it_has_one)
{ ClContextRig rig;
  arena_answers(rig);
  GuestClContext context = made_context(rig);
  CHECK_EQ(context.guest_device(), 0u);
  CHECK_STATUS(context.make(), Status::ok);
  const std::uint32_t device = context.guest_device();
  CHECK(device != 0u);
  av::icd::DeviceLink *link = nullptr;
  CHECK_STATUS(rig.connection().acquire(link), Status::ok);
  CHECK_EQ(device, link->device().device());
  rig.connection().release(*link);
  CHECK(context.invariants_hold()); }

TEST(guest_cl_context, it_shares_gl_only_once_made_from_a_gl_context)
{ ClContextRig rig;
  arena_answers(rig);
  GuestClContext sharing = made_context(rig);
  CHECK(!sharing.shares_gl());
  CHECK_STATUS(sharing.make(0x40000C42u), Status::ok);
  CHECK(sharing.shares_gl());
  CHECK(sharing.invariants_hold());

  ClContextRig   plain_rig;
  arena_answers(plain_rig);
  GuestClContext plain = made_context(plain_rig);
  CHECK_STATUS(plain.make(), Status::ok);
  CHECK(!plain.shares_gl()); }

TEST(guest_cl_context, the_properties_it_keeps_are_the_list_it_was_made_with)
{ ClContextRig rig;
  GuestClContext context = made_context(rig);
  cl_context_properties out[8] = {};
  CHECK_EQ(context.properties(out, 8u), 0u);
  const cl_context_properties given[] = {CL_CONTEXT_PLATFORM, 0x7200, CL_CONTEXT_INTEROP_USER_SYNC, CL_TRUE, 0};
  context.keep_properties(given);
  CHECK_EQ(context.properties(out, 8u), 5u);
  for (std::uint32_t at = 0u; at < 5u; at++) CHECK_EQ(out[at], given[at]);
  CHECK_EQ(context.properties(out, 4u), 0u);
  const cl_context_properties shorter[] = {CL_CONTEXT_PLATFORM, 0x7300, 0};
  context.keep_properties(shorter);
  CHECK_EQ(context.properties(out, 8u), 3u);
  CHECK_EQ(out[1], 0x7300);
  context.keep_properties(nullptr);
  CHECK_EQ(context.properties(out, 8u), 0u);
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

TEST(guest_cl_context, with_no_avgpu_adapter_it_is_not_supported_and_holds_nothing)
{ av::MockPlatform platform;
  MockKmt kmt;
  av::ScriptedHost host(kmt);
  av::LockTrace trace;
  av::RecordingThreadLock send_lock(trace, 'S');
  av::RecordingLog log;
  av::KeyedLog lines(platform, log, "avcl: ");
  av::icd::IcdConnection connection(platform, kmt, 0u);
  av::RecordingThreadLock handles_lock(trace, 'H');
  av::icd::SharedHandles  handles(kmt, handles_lock, platform);
  av::RecordingThreadLock blocks_lock(trace, 'B');
  GuestClContext context(connection, platform, send_lock, blocks_lock, lines, handles);
  CHECK_STATUS(context.make(), Status::not_supported);
  CHECK(context.state() == GuestClContext::State::created);
  CHECK_EQ(kmt.open_objects(), 0u);
  CHECK_EQ(platform.live_allocations(), 0u);
  CHECK(context.invariants_hold()); }

TEST(guest_cl_context, a_context_that_is_refused_memory_at_any_step_of_making_holds_nothing)
{ bool ran_out = false;
  bool made = false;
  for (std::uint32_t nth = 1u; nth <= 64u && !made; ++nth) {
      ClContextRig rig;
      arena_answers(rig);
      GuestClContext context = made_context(rig);
      rig.platform().fail_allocation_at(nth);
      const Status status = context.make();
      if (status == Status::ok) {
          made = true;
          CHECK(context.state() == GuestClContext::State::made);
          CHECK(context.invariants_hold());
          continue;
      }
      ran_out = ran_out || status == Status::no_memory;
      CHECK(context.state() == GuestClContext::State::created);
      CHECK(context.wire() == nullptr);
      CHECK_EQ(rig.kmt().open_objects(), 0u);
      CHECK_EQ(rig.platform().live_allocations(), 0u);
      CHECK(context.invariants_hold());
  }
  CHECK(ran_out);
  CHECK(made); }

TEST(guest_cl_context, a_context_whose_transport_faulted_opening_its_arena_is_not_made)
{ ClContextRig rig;
  GuestClContext context = made_context(rig);
  CHECK_STATUS(context.make(), Status::invalid_state);
  CHECK(context.state() == GuestClContext::State::created);
  CHECK(rig.log().holding("faulted") > 0u);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.platform().live_allocations(), 0u);
  CHECK(context.invariants_hold()); }

TEST(guest_cl_context, a_device_that_will_not_import_leaves_the_context_usable)
{ ClContextRig rig;
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

  rig.kmt().withhold_answer_at(1u);
  static_cast<void>(av::clproto::av_clFlush(*context.wire(), 0u));

  CHECK(context.wire() == nullptr);
  CHECK(context.lost());
  CHECK(context.state() == GuestClContext::State::lost);
  CHECK(rig.log().holding("lost") > 0u);
  CHECK(context.invariants_hold()); }

namespace {

std::uint32_t told_lost = 0u;

void tell_lost(const char *, const void *, std::size_t, void *user_data)
{
    told_lost += user_data == &told_lost ? 1u : 100u;
}

}

TEST(guest_cl_context, the_call_that_finds_the_context_lost_tells_its_callback_once)
{ ClContextRig rig;
  arena_answers(rig);
  GuestClContext context = made_context(rig);
  CHECK_STATUS(context.make(), Status::ok);
  told_lost = 0u;
  context.notify_with(av::clicd::ClContextNotify(&tell_lost, &told_lost));
  CHECK(context.wire() != nullptr);
  CHECK_EQ(told_lost, 0u);
  rig.kmt().withhold_answer_at(1u);
  static_cast<void>(av::clproto::av_clFlush(*context.wire(), 0u));
  CHECK(context.wire() == nullptr);
  CHECK_EQ(told_lost, 1u);
  CHECK(context.wire() == nullptr);
  CHECK_EQ(told_lost, 1u);
  CHECK(context.invariants_hold()); }

TEST(guest_cl_context, the_svm_table_belongs_to_the_context_and_goes_with_it)
{ ClContextRig rig;
  arena_answers(rig);
  { GuestClContext context = made_context(rig);
    CHECK_STATUS(context.make(), Status::ok);
    CHECK(context.svm().take(0x7F00'0000'1000ull, 128u) != nullptr);
    CHECK_EQ(context.svm().live(), 1u);
    CHECK(context.invariants_hold()); }
  CHECK_EQ(rig.platform().live_allocations(), 0u); }

TEST(guest_cl_context, tearing_down_gives_back_every_object_it_opened)
{ ClContextRig rig;
  arena_answers(rig);
  { GuestClContext context = made_context(rig);
    CHECK_STATUS(context.make(), Status::ok);
    CHECK(rig.kmt().open_objects() > 0u); }
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.platform().live_allocations(), 0u); }

namespace av::clicd {
struct GuestClContextProbe {
    static void retarget(GuestClContext &context, uint64_t object, uint8_t *to)
    { context.mappings_.for_each([object, to](MappedRegion &mapping) {
          if (mapping.object() == object) mapping.hand(to, icd::StagedMapping::Memory::named);
      }); }
    static icd::HostChannel &channel(GuestClContext &context) { return context.channel_; }
    static void          forget_link(GuestClContext &context)
    { icd::ContextLinkProbe(context.link_).link() = nullptr; }
    static void          say_it_was_made(GuestClContext &context)
    { context.state_ = GuestClContext::State::made; }
    static void          say_it_was_never_made(GuestClContext &context)
    { context.state_ = GuestClContext::State::created; }
    static void          drop_the_lock(GuestClContext &context) { icd::HostChannelProbe(context.channel_).drop_locked(); }
    static void          restore_the_lock(GuestClContext &context)
    { icd::HostChannelProbe(context.channel_).restore_locked(); }
    static void          stray_link(GuestClContext &context) { static icd::DeviceLink *const nowhere = reinterpret_cast<icd::DeviceLink *>(std::uintptr_t{8u});
      icd::ContextLinkProbe(context.link_).link() = nowhere; }
    static SvmTable     &svm_of(GuestClContext &context) { return context.svm_; }
};

}

namespace av::clicd {
struct SvmTableProbe {
    static void miscount(SvmTable &table) { static_cast<void>(table.bases_.append(1u)); }
};

}

using av::clicd::GuestClContextProbe;
using av::clicd::MappedRegion;

TEST(guest_cl_context, a_mapping_of_nothing_is_not_taken_by_either_kind)
{ ClContextRig   rig;
  arena_answers(rig);
  GuestClContext context = made_context(rig);
  CHECK_STATUS(context.make(), Status::ok);
  std::uint8_t at[16] = {};
  CHECK(context.take_mapping(0u, 0x7000u, 0u, sizeof at, CL_MAP_READ) == nullptr);
  CHECK(context.take_mapping(0x6600u, 0u, 0u, sizeof at, CL_MAP_READ) == nullptr);
  CHECK(context.take_mapping(0x6600u, 0x7000u, 0u, 0u, CL_MAP_READ) == nullptr);
  CHECK(context.take_shared_mapping(0u, 0x7000u, 0u, sizeof at, at) == nullptr);
  CHECK(context.take_shared_mapping(0x6600u, 0u, 0u, sizeof at, at) == nullptr);
  CHECK(context.take_shared_mapping(0x6600u, 0x7000u, 0u, 0u, at) == nullptr);
  CHECK(context.take_shared_mapping(0x6600u, 0x7000u, 0u, sizeof at, nullptr) == nullptr);
  CHECK_EQ(context.mappings(), 0u);
  CHECK(context.invariants_hold()); }

TEST(guest_cl_context, the_mapping_table_grows_past_any_fixed_bound_and_refuses_both_kinds_out_of_memory)
{ ClContextRig   rig;
  arena_answers(rig);
  GuestClContext context = made_context(rig);
  CHECK_STATUS(context.make(), Status::ok);
  constexpr std::uint32_t most = 4096u;
  constexpr std::uint32_t grown = 600u;
  static std::uint8_t at[most][16];
  std::uint32_t taken = 0u;
  while (taken < grown && context.take_shared_mapping(0x6600u + taken, 0x7000u + taken, 0u, 16u, at[taken]) != nullptr)
      ++taken;
  CHECK_EQ(taken, grown);
  rig.platform().fail_allocation_at(1u);
  while (taken < most && context.take_shared_mapping(0x6600u + taken, 0x7000u + taken, 0u, 16u, at[taken]) != nullptr)
      ++taken;
  CHECK(taken < most);
  CHECK_EQ(context.mappings(), taken);
  CHECK(context.invariants_hold());
  CHECK(context.take_shared_mapping(0x6600u + taken, 0x7000u + taken, 0u, 16u, at[taken]) != nullptr);
  CHECK_EQ(context.mappings(), taken + 1u);
  rig.platform().fail_allocation_at(1u);
  CHECK(context.take_mapping(0x7700u, 0x8800u, 0u, 16u, CL_MAP_READ) == nullptr);
  CHECK_EQ(context.mappings(), taken + 1u);
  CHECK(context.invariants_hold()); }

TEST(guest_cl_context, a_staged_mapping_with_no_memory_for_its_block_is_not_taken)
{ ClContextRig   rig;
  arena_answers(rig);
  GuestClContext context = made_context(rig);
  CHECK_STATUS(context.make(), Status::ok);
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
  GuestClContextProbe::retarget(context, 0x6601u, at);
  CHECK(!context.invariants_hold());
  GuestClContextProbe::retarget(context, 0x6601u, other);
  CHECK(context.invariants_hold()); }

TEST(guest_cl_context, the_invariant_catches_a_made_context_that_is_missing_what_a_make_holds)
{ { ClContextRig   rig;
    arena_answers(rig);
    GuestClContext context = made_context(rig);
    CHECK_STATUS(context.make(), Status::ok);
    CHECK(context.invariants_hold());
    av::icd::HostChannelProbe channel(GuestClContextProbe::channel(context));
    std::uint8_t *const staging = channel.swap_staging(nullptr);
    CHECK(!context.invariants_hold());
    static_cast<void>(channel.swap_staging(staging));
    CHECK(context.invariants_hold()); }
  { ClContextRig   rig;
    arena_answers(rig);
    GuestClContext context = made_context(rig);
    CHECK_STATUS(context.make(), Status::ok);
    GuestClContextProbe::forget_link(context);
    CHECK(!context.invariants_hold()); }
  { ClContextRig   rig;
    GuestClContext context = made_context(rig);
    CHECK(context.invariants_hold());
    GuestClContextProbe::say_it_was_made(context);
    CHECK(!context.invariants_hold()); } }

TEST(guest_cl_context, a_context_says_whether_it_got_an_arena)
{ { ClContextRig rig;
    GuestClContext context = made_context(rig);
    CHECK(!context.has_arena()); }
  { ClContextRig rig;
    arena_answers(rig);
    GuestClContext context = made_context(rig);
    CHECK_STATUS(context.make(), Status::ok);
    CHECK(context.has_arena()); }
  { ClContextRig rig;
    arena_answers(rig, 1u);
    GuestClContext context = made_context(rig);
    CHECK_STATUS(context.make(), Status::ok);
    CHECK(!context.has_arena()); } }

TEST(guest_cl_context, the_arena_lends_its_bottom_to_buffers_and_its_top_to_transfers)
{ ClContextRig rig;
  arena_answers(rig);
  GuestClContext context = made_context(rig);
  CHECK_STATUS(context.make(), Status::ok);
  const std::uint64_t page = av::clicd::ClArenaBlocks::alignment;
  const std::uint64_t reserve = GuestClContext::staging_reserve;
  std::uint64_t       staged = ~std::uint64_t{0};
  CHECK(!context.staging().take(reserve + page, staged));
  CHECK(context.staging().take(reserve, staged));
  CHECK_EQ(staged, 0u);
  std::uint64_t named = ~std::uint64_t{0};
  CHECK(context.wire()->blobs().holds(context.staging().at(staged, reserve), reserve, named));
  const std::uint64_t buffers = named;
  CHECK(buffers > 0u);
  CHECK(buffers % page == 0u);
  CHECK(buffers + reserve <= GuestClContext::arena_bytes);
  std::uint64_t more = 0u;
  CHECK(!context.staging().take(page, more));

  std::uint64_t offset = 0u;
  CHECK(!context.blocks().take(buffers + page, offset));
  CHECK(context.blocks().take(buffers, offset));
  CHECK_EQ(offset, 0u);
  named = ~std::uint64_t{0};
  CHECK(context.wire()->blobs().holds(context.blocks().at(0u, buffers), buffers, named));
  CHECK_EQ(named, 0u);
  CHECK(context.invariants_hold()); }

TEST(guest_cl_context, the_invariant_reaches_the_allocations_and_the_mappings_it_holds)
{ ClContextRig rig;
  arena_answers(rig);
  GuestClContext context = made_context(rig);
  CHECK_STATUS(context.make(), Status::ok);
  std::uint8_t at[16] = {};
  CHECK(context.take_shared_mapping(0x6600u, 0x7000u, 0u, sizeof at, at) != nullptr);
  CHECK(context.invariants_hold());
  GuestClContextProbe::retarget(context, 0x6600u, nullptr);
  CHECK(!context.invariants_hold());
  GuestClContextProbe::retarget(context, 0x6600u, at);
  CHECK(context.invariants_hold()); }

TEST(guest_cl_context, the_invariant_catches_an_unmade_context_still_holding_something)
{ ClContextRig rig;
  arena_answers(rig);
  GuestClContext context = made_context(rig);
  CHECK_STATUS(context.make(), Status::ok);
  GuestClContextProbe::say_it_was_never_made(context);
  CHECK(!context.invariants_hold());
  GuestClContextProbe::say_it_was_made(context);
  CHECK(context.invariants_hold()); }

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

TEST(guest_cl_context, the_invariant_catches_each_thing_an_unmade_context_should_not_hold)
{ { ClContextRig rig;
    GuestClContext context = made_context(rig);
    CHECK(context.invariants_hold());
    std::uint8_t block = 0u;
    av::icd::HostChannelProbe channel(GuestClContextProbe::channel(context));
    static_cast<void>(channel.swap_staging(&block));
    CHECK(!context.invariants_hold());
    static_cast<void>(channel.swap_staging(nullptr));
    CHECK(context.invariants_hold()); }
  { ClContextRig rig;
    GuestClContext context = made_context(rig);
    GuestClContextProbe::stray_link(context);
    CHECK(!context.invariants_hold());
    GuestClContextProbe::forget_link(context);
    CHECK(context.invariants_hold()); } }

TEST(guest_cl_context, the_invariant_reaches_the_shared_allocations_it_holds)
{ ClContextRig rig;
  arena_answers(rig);
  GuestClContext context = made_context(rig);
  CHECK_STATUS(context.make(), Status::ok);
  CHECK(context.invariants_hold());
  av::clicd::SvmTableProbe::miscount(GuestClContextProbe::svm_of(context));
  CHECK(!context.invariants_hold()); }

TEST(guest_cl_context, a_driver_that_refuses_the_contexts_transport_makes_no_context)
{ ClContextRig rig;
  GuestClContext context = made_context(rig);
  rig.kmt().fail_at(av::MockKmt::Call::create_context_virtual, 1u, 0xC0000001u);
  CHECK(context.make() != Status::ok);
  CHECK(context.state() == GuestClContext::State::created);
  CHECK(context.wire() == nullptr);
  CHECK_EQ(rig.log().holding("the driver refused the context"), 1u);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK(context.invariants_hold()); }

TEST(guest_cl_context, a_staged_mapping_starts_as_zeros)
{ ClContextRig   rig;
  arena_answers(rig);
  GuestClContext context = made_context(rig);
  CHECK_STATUS(context.make(), Status::ok);
  av::clicd::MappedRegion *const region = context.take_mapping(0x6600u, 0x7000u, 0u, 64u, CL_MAP_WRITE_INVALIDATE_REGION);
  CHECK(region != nullptr);
  if (region == nullptr) return;
  const std::uint8_t *const block = region->at();
  CHECK(block != nullptr);
  bool zeros = true;
  for (std::uint32_t at = 0u; at < 64u; at++) zeros = zeros && block[at] == 0u;
  CHECK(zeros);
  CHECK(context.mapping_at(0x6600u, block) == region);
  context.give_back_mapping(*region);
  CHECK_EQ(context.mappings(), 0u); }

TEST(guest_cl_context, a_d3d11_table_that_cannot_be_true_makes_the_context_untrue)
{ ClContextRig rig;
  GuestClContext context = made_context(rig);
  int            mem = 0, resource = 0;
  const av::clicd::D3d11Resource buffer(av::clicd::D3d11Resource::Kind::buffer, 0u, 64u, 0u, 0u, 0u, 1u, 1u, 1u);
  CHECK_STATUS(context.d3d11().keep(&mem, &resource, 0u, buffer, CL_MEM_READ_WRITE), av::Status::ok);
  CHECK(!context.invariants_hold());
  void *forgotten = nullptr;
  CHECK(context.d3d11().forget(&mem, forgotten));
  CHECK(context.invariants_hold()); }
