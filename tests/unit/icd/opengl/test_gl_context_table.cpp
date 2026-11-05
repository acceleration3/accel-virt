#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/gl_context_rig.hpp"
#include "icd/opengl/gl_context_table.hpp"

#include <cstring>

using av::GlContextOptions;
using av::GlContextRig;
using av::RecordingThreadLock;
using av::Status;
using av::glicd::GlContextTable;
using av::glicd::GuestGlContext;

namespace av::glicd {

struct GuestGlContextProbe {
    static void claim_made(GuestGlContext &context) { context.state_ = GuestGlContext::State::made; }
    static void unclaim(GuestGlContext &context) { context.state_ = GuestGlContext::State::created; }
};

struct GlContextTableProbe {
    static void last_handle(GlContextTable &table) { table.next_ = GlContextTable::last_handle; }
    static void zero_next(GlContextTable &table) { table.next_ = 0u; }
    static void restore_next(GlContextTable &table) { table.next_ = 1u; }
    static GuestGlContext *at(GlContextTable &table, std::uint32_t slot) { return *table.contexts_.at(slot); }
    static void plant(GlContextTable &table, std::uint64_t handle, GuestGlContext *context)
    {
        std::uint32_t slot = 0u;
        static_cast<void>(table.contexts_.insert(handle, context, slot));
    }
    static void pull(GlContextTable &table, std::uint64_t handle) { static_cast<void>(table.contexts_.erase(handle)); }
};

}

namespace {

struct Table {
    GlContextRig        rig;
    RecordingThreadLock lock{rig.trace(), 'T'};
    RecordingThreadLock slice_lock{rig.trace(), 'L'};
    GlContextTable      table{rig.platform(), rig.connection(), lock, rig.send_lock(), slice_lock, rig.lines(),
                              rig.extensions(), rig.handles()};
};

}

TEST(gl_context_table, a_handle_names_its_context_until_it_is_destroyed)
{ Table fixture;
  GlContextTable &table = fixture.table;
  CHECK(table.invariants_hold());
  std::uint64_t first = 0u;
  std::uint64_t second = 0u;
  CHECK_STATUS(table.create(2, GlContextOptions(), first), Status::ok);
  CHECK_STATUS(table.create(3, GlContextOptions(), second), Status::ok);
  CHECK(table.invariants_hold());
  CHECK_EQ(first, 1u);
  CHECK_EQ(second, 2u);
  CHECK_EQ(table.live(), 2u);
  GuestGlContext *const context = table.resolve(second);
  CHECK(context != nullptr);
  CHECK_EQ(context->pixel_format(), 3);
  CHECK_STATUS(table.destroy(first), Status::ok);
  CHECK(table.invariants_hold());
  CHECK(table.resolve(first) == nullptr);
  CHECK_STATUS(table.destroy(first), Status::invalid_parameter);
  std::uint64_t third = 0u;
  CHECK_STATUS(table.create(4, GlContextOptions(), third), Status::ok);
  CHECK_EQ(third, 3u);
  CHECK(table.resolve(first) == nullptr);
  CHECK_EQ(table.resolve(third)->pixel_format(), 4);
  CHECK_EQ(table.live(), 2u); }

TEST(gl_context_table, forged_handles_resolve_to_nothing)
{ Table fixture;
  std::uint64_t handle = 0u;
  CHECK_STATUS(fixture.table.create(1, GlContextOptions(), handle), Status::ok);
  const std::uint64_t forged[] = {0u, 2u, 0x10000u, 0x10001u, 0x100000001ull, 0xFFFFFFFFu,
                                  0xFFFFFFFFFFFFFFFFull};
  for (const std::uint64_t each : forged) {
      CHECK(fixture.table.resolve(each) == nullptr);
      CHECK_STATUS(fixture.table.destroy(each), Status::invalid_parameter);
  }
  CHECK(fixture.table.resolve(handle) != nullptr);
  CHECK_EQ(fixture.table.live(), 1u); }

TEST(gl_context_table, holds_contexts_past_the_old_bound_and_refuses_one_with_no_memory)
{ Table fixture;
  constexpr std::uint32_t many = 600u;
  static std::uint64_t handles[many];
  for (std::uint32_t each = 0u; each < many; each++) {
      CHECK_STATUS(fixture.table.create(static_cast<std::int32_t>(each % 7u) + 1, GlContextOptions(), handles[each]),
                   Status::ok);
      CHECK_EQ(handles[each], each + 1u);
  }
  CHECK_EQ(fixture.table.live(), many);
  for (std::uint32_t each = 0u; each < many; each++)
      CHECK_EQ(fixture.table.resolve(handles[each])->pixel_format(), static_cast<std::int32_t>(each % 7u) + 1);
  CHECK(fixture.table.invariants_hold());
  for (std::uint32_t each = 0u; each < many; each += 2u) CHECK_STATUS(fixture.table.destroy(handles[each]), Status::ok);
  CHECK_EQ(fixture.table.live(), many / 2u);
  CHECK(fixture.table.resolve(handles[0]) == nullptr);
  CHECK(fixture.table.resolve(handles[1]) != nullptr);
  CHECK(fixture.table.invariants_hold());
  Table starved;
  std::uint64_t handle = 7u;
  starved.rig.platform().fail_allocation_at(1u);
  CHECK_STATUS(starved.table.create(1, GlContextOptions(), handle), Status::no_memory);
  CHECK_EQ(handle, 0u);
  CHECK_EQ(starved.table.live(), 0u);
  handle = 7u;
  starved.rig.platform().fail_allocation_at(3u);
  CHECK_STATUS(starved.table.create(1, GlContextOptions(), handle), Status::no_memory);
  CHECK_EQ(handle, 0u);
  CHECK_EQ(starved.table.live(), 0u);
  CHECK(starved.table.invariants_hold()); }

TEST(gl_context_table, a_handle_wraps_past_the_last_and_skips_one_in_use)
{ Table fixture;
  std::uint64_t first = 0u, last = 0u, wrapped = 0u;
  CHECK_STATUS(fixture.table.create(1, GlContextOptions(), first), Status::ok);
  CHECK_EQ(first, 1u);
  av::glicd::GlContextTableProbe::last_handle(fixture.table);
  CHECK_STATUS(fixture.table.create(1, GlContextOptions(), last), Status::ok);
  CHECK_EQ(last, 0xFFFFFFFFu);
  CHECK_STATUS(fixture.table.create(1, GlContextOptions(), wrapped), Status::ok);
  CHECK_EQ(wrapped, 2u);
  av::glicd::GlContextTableProbe::last_handle(fixture.table);
  std::uint64_t again = 0u;
  CHECK_STATUS(fixture.table.create(1, GlContextOptions(), again), Status::ok);
  CHECK_EQ(again, 3u);
  CHECK(fixture.table.invariants_hold()); }

TEST(gl_context_table, reports_each_live_context_under_its_handle)
{ Table fixture;
  std::uint64_t handle = 0u;
  CHECK_STATUS(fixture.table.create(1, GlContextOptions(), handle), Status::ok);
  char bytes[1024];
  av::StateWriter writer(bytes, sizeof bytes);
  fixture.table.report(writer);
  CHECK(std::strcmp(writer.dump(), "contexts/0 live=1 retired=0\n"
                                   "contexts/0/context/1 state=created owner=0 pixel_format=1 major=0 minor=0 "
                                   "profile=0 flags=0 share=0 share_target=0 error=0 device=0 lost=false "
                                   "fault_code=0 arena=false renderer= extensions=0\n"
                                   "contexts/0/context/1/windows/0 held=0 draws_window=false reads_window=false\n"
                                   "contexts/0/context/1/mappings/0 held=0 persistent_writers=0\n"
                                   "contexts/0/context/1/sliced_buffers/0 granted=false stopped=false group=1 "
                                   "owned=0 held=0\n"
                                   "contexts/0/context/1/shadows/0 trusted=true held=0 held_bytes=0 sizes=0") == 0); }

TEST(gl_context_table_invariants, fail_for_no_next_handle_or_a_broken_context)
{ Table zero;
  av::glicd::GlContextTableProbe::zero_next(zero.table);
  CHECK(!zero.table.invariants_hold());
  av::glicd::GlContextTableProbe::restore_next(zero.table);
  CHECK(zero.table.invariants_hold());
  Table broken;
  std::uint64_t handle = 0u;
  CHECK_STATUS(broken.table.create(1, GlContextOptions(), handle), Status::ok);
  GuestGlContext *const context = av::glicd::GlContextTableProbe::at(broken.table, 0u);
  av::glicd::GuestGlContextProbe::claim_made(*context);
  CHECK(!broken.table.invariants_hold());
  av::glicd::GuestGlContextProbe::unclaim(*context);
  CHECK(broken.table.invariants_hold());
  av::glicd::GlContextTableProbe::plant(broken.table, 99u, nullptr);
  CHECK(!broken.table.invariants_hold());
  av::glicd::GlContextTableProbe::pull(broken.table, 99u);
  av::glicd::GlContextTableProbe::plant(broken.table, 99u, context);
  CHECK(!broken.table.invariants_hold());
  av::glicd::GlContextTableProbe::pull(broken.table, 99u);
  CHECK(broken.table.invariants_hold()); }
