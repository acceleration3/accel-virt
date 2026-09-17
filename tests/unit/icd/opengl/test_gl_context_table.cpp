/* DHGLRCs: a slot and a generation, so a stale or forged handle resolves to
 * nothing, and at most 256 live contexts. */
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/gl_context_rig.hpp"
#include "icd/opengl/gl_context_table.hpp"

#include <cstring>

using av::GlContextOptions;
using av::GlContextRig;
using av::Status;
using av::glicd::GlContextTable;
using av::glicd::GuestGlContext;

namespace av::glicd {

struct GuestGlContextProbe {
    static void claim_made(GuestGlContext &context) { context.state_ = GuestGlContext::State::made; }
    static void unclaim(GuestGlContext &context) { context.state_ = GuestGlContext::State::created; }
};

struct GlContextTableProbe {
    static void zero_generation(GlContextTable &table) { table.generations_[3] = 0u; }
    static void pass_generation(GlContextTable &table) { table.generations_[4] = GlContextTable::max_generation + 1u; }
    static void miscount(GlContextTable &table) { table.live_ = 1u; }
    static void last_generation(GlContextTable &table, uint32_t slot) { table.generations_[slot] = 0xFFFFu; }
    static GuestGlContext *slot(GlContextTable &table, uint32_t slot) { return table.contexts_[slot]; }
};

}  // namespace av::glicd

namespace {

struct Table {
    GlContextRig   rig;
    GlContextTable table{rig.platform(), rig.connection(), rig.send_lock(), rig.log(), rig.extensions()};
};

}  // namespace

TEST(gl_context_table, a_handle_names_its_context_until_it_is_destroyed)
{ Table fixture;
  GlContextTable &table = fixture.table;
  CHECK(table.invariants_hold());
  std::uint64_t first = 0u;
  std::uint64_t second = 0u;
  CHECK_STATUS(table.create(2, GlContextOptions(), first), Status::ok);
  CHECK_STATUS(table.create(3, GlContextOptions(), second), Status::ok);
  CHECK(table.invariants_hold());
  CHECK_EQ(first, 0x10001u);
  CHECK_EQ(second, 0x10002u);
  CHECK_EQ(table.live(), 2u);
  GuestGlContext *const context = table.resolve(second);
  CHECK(context != nullptr);
  CHECK_EQ(context->pixel_format(), 3);
  CHECK_EQ(table.handle_of(*context), second);
  CHECK_STATUS(table.destroy(first), Status::ok);
  CHECK(table.invariants_hold());
  CHECK(table.resolve(first) == nullptr);
  CHECK_STATUS(table.destroy(first), Status::invalid_parameter);
  /* The slot is taken again under a new generation: the old handle stays dead. */
  std::uint64_t third = 0u;
  CHECK_STATUS(table.create(4, GlContextOptions(), third), Status::ok);
  CHECK_EQ(third, 0x20001u);
  CHECK(table.resolve(first) == nullptr);
  CHECK_EQ(table.resolve(third)->pixel_format(), 4);
  CHECK_EQ(table.live(), 2u); }

TEST(gl_context_table, forged_handles_resolve_to_nothing)
{ Table fixture;
  std::uint64_t handle = 0u;
  CHECK_STATUS(fixture.table.create(1, GlContextOptions(), handle), Status::ok);
  const std::uint64_t forged[] = {0u, 1u, 0x10000u, 0x10003u, 0x30001u, 0x1000000010001ull, 0x10101u,
                                  0xFFFFFFFFFFFFFFFFull};
  for (const std::uint64_t each : forged) {
      CHECK(fixture.table.resolve(each) == nullptr);
      CHECK_STATUS(fixture.table.destroy(each), Status::invalid_parameter);
  }
  CHECK(fixture.table.resolve(handle) != nullptr);
  CHECK_EQ(fixture.table.live(), 1u);
  GuestGlContext stranger(fixture.rig.connection(), fixture.rig.platform(), fixture.rig.send_lock(),
                          fixture.rig.log(), fixture.rig.extensions(), 1, GlContextOptions());
  CHECK_EQ(fixture.table.handle_of(stranger), 0u); }

TEST(gl_context_table, refuses_a_257th_context_and_a_pool_that_cannot_grow)
{ Table fixture;
  std::uint64_t handle = 0u;
  for (std::uint32_t each = 0u; each < GlContextTable::max_contexts; each++)
      CHECK_STATUS(fixture.table.create(1, GlContextOptions(), handle), Status::ok);
  CHECK_EQ(handle, 0x10100u);
  CHECK_STATUS(fixture.table.create(1, GlContextOptions(), handle), Status::no_space);
  CHECK_EQ(handle, 0u);
  CHECK(fixture.table.invariants_hold());
  Table starved;
  starved.rig.platform().fail_allocation_at(1u);
  CHECK_STATUS(starved.table.create(1, GlContextOptions(), handle), Status::no_memory);
  CHECK_EQ(handle, 0u);
  CHECK_EQ(starved.table.live(), 0u); }

TEST(gl_context_table, a_generation_wraps_past_its_last_value)
{ Table fixture;
  av::glicd::GlContextTableProbe::last_generation(fixture.table, 0u);
  std::uint64_t handle = 0u;
  CHECK_STATUS(fixture.table.create(1, GlContextOptions(), handle), Status::ok);
  CHECK_EQ(handle, 0xFFFF0001u);
  CHECK_STATUS(fixture.table.destroy(handle), Status::ok);
  CHECK_STATUS(fixture.table.create(1, GlContextOptions(), handle), Status::ok);
  CHECK_EQ(handle, 0x10001u);
  CHECK(fixture.table.invariants_hold()); }

TEST(gl_context_table, reports_each_live_context_under_its_handle)
{ Table fixture;
  std::uint64_t handle = 0u;
  CHECK_STATUS(fixture.table.create(1, GlContextOptions(), handle), Status::ok);
  char bytes[512];
  av::StateWriter writer(bytes, sizeof bytes);
  fixture.table.report(writer);
  CHECK(std::strcmp(writer.dump(), "contexts/0 live=1\ncontexts/0/context/65537 state=created owner=0 "
                                   "pixel_format=1 major=0 minor=0 profile=0 flags=0 share=0 share_target=0 error=0 "
                                   "link=4 arena=false renderer= extensions=0\n"
                                   "contexts/0/context/65537/windows/0 held=0 draws_window=false "
                                   "reads_window=false\n"
                     "contexts/0/context/65537/mappings/0 held=0 persistent_writers=0") == 0); }

TEST(gl_context_table_invariants, fail_for_a_zero_generation_a_miscount_or_a_broken_context)
{ Table zero;
  av::glicd::GlContextTableProbe::zero_generation(zero.table);
  CHECK(!zero.table.invariants_hold());
  Table past;
  av::glicd::GlContextTableProbe::pass_generation(past.table);
  CHECK(!past.table.invariants_hold());
  Table counted;
  av::glicd::GlContextTableProbe::miscount(counted.table);
  CHECK(!counted.table.invariants_hold());
  Table broken;
  std::uint64_t handle = 0u;
  CHECK_STATUS(broken.table.create(1, GlContextOptions(), handle), Status::ok);
  GuestGlContext *const context = av::glicd::GlContextTableProbe::slot(broken.table, 0u);
  av::glicd::GuestGlContextProbe::claim_made(*context);
  CHECK(!broken.table.invariants_hold());
  av::glicd::GuestGlContextProbe::unclaim(*context);
  CHECK(broken.table.invariants_hold()); }
