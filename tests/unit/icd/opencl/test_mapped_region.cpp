/* One live mapping of a buffer, and the three things an unmap has to know.
 *
 * A guest cannot be handed the host's pointer, so a mapping is a pair of
 * addresses: what the application was given, and what the host's own map
 * answered - an unmap names the second, and the application only ever sees the
 * first. The third thing is whether the bytes have to go back, which is what
 * the map flags say and what makes a read-only mapping free to throw away.
 *
 * The mappings themselves live on the context, because a buffer released with
 * one still live keeps the mapping's memory until the unmap. */
#include "harness/framework/test_registry.hpp"
#include "harness/icd/cl_context_rig.hpp"
#include "icd/opencl/guest_cl_context.hpp"
#include "icd/opencl/mapped_region.hpp"

#include <cstdint>

using av::ClContextRig;
using av::Status;
using av::clicd::GuestClContext;
using av::clicd::MappedRegion;
using av::clicd::MappedRegionProbe;

namespace av::clicd {

/* A mapping is either live or wholly empty. Each of these is a halfway state
 * the class never builds: a live mapping of no bytes, or one naming neither
 * the buffer nor the address the host's own map answered, could never be
 * unmapped; and an empty one still naming a buffer would have the context
 * believe a slot is free while an unmap could still find it. */
struct MappedRegionProbe {
    static void forget_length(MappedRegion &region) { region.bytes_ = 0u; }
    static void forget_buffer(MappedRegion &region) { region.buffer_ = 0u; }
    static void forget_host_address(MappedRegion &region) { region.host_ = 0u; }
    static void stray_buffer(MappedRegion &region) { region.buffer_ = 1u; }
    static void stray_length(MappedRegion &region) { region.bytes_ = 1u; }
    static void stray_offset(MappedRegion &region) { region.offset_ = 1u; }
    static void stray_staging(MappedRegion &region) { region.memory_ = MappedRegion::Memory::staged; }
};

}  // namespace av::clicd

namespace {

constexpr std::uint64_t host_buffer = 0x6600u;
constexpr std::uint64_t host_address = 0x0000'7F44'0000'2000ull;

}  // namespace

TEST(mapped_region, one_that_holds_nothing_answers_for_nothing)
{ MappedRegion region;
  CHECK(!region.held());
  int elsewhere = 0;
  CHECK(!region.answers(&elsewhere));
  CHECK(!region.answers(nullptr));
  CHECK(!region.writes_back());
  CHECK(region.invariants_hold()); }

TEST(mapped_region, it_answers_the_pointer_the_map_returned_and_no_other)
{ MappedRegion region;
  std::uint8_t block[64] = {};
  region.take(host_buffer, host_address, 16u, sizeof block, CL_MAP_READ, block, MappedRegion::Memory::staged);
  CHECK(region.held());
  CHECK(region.answers(block));
  /* An unmap takes what it was given; an address inside the mapping is not
   * that pointer. */
  CHECK(!region.answers(block + 1));
  CHECK(!region.answers(block + sizeof block));
  CHECK_EQ(region.offset(), 16u);
  CHECK_EQ(region.buffer(), host_buffer);
  CHECK_EQ(region.host(), host_address);
  CHECK(region.invariants_hold()); }

TEST(mapped_region, a_read_only_mapping_is_thrown_away_rather_than_written_back)
{ MappedRegion region;
  std::uint8_t block[64] = {};
  /* CL_MAP_READ is a promise not to write through the pointer, so writing the
   * bytes back would overwrite whatever the device did in the meantime. */
  region.take(host_buffer, host_address, 0u, sizeof block, CL_MAP_READ, block, MappedRegion::Memory::staged);
  CHECK(!region.writes_back());
  region.take(host_buffer, host_address, 0u, sizeof block, CL_MAP_WRITE, block, MappedRegion::Memory::staged);
  CHECK(region.writes_back());
  region.take(host_buffer, host_address, 0u, sizeof block, CL_MAP_READ | CL_MAP_WRITE, block, MappedRegion::Memory::staged);
  CHECK(region.writes_back());
  region.take(host_buffer, host_address, 0u, sizeof block, CL_MAP_WRITE_INVALIDATE_REGION, block, MappedRegion::Memory::staged);
  CHECK(region.writes_back()); }

TEST(mapped_region, a_mapping_that_was_not_staged_writes_nothing_back)
{ MappedRegion region;
  std::uint8_t block[64] = {};
  /* The bytes were already the guest's own - a host-visible blob - so there
   * is nothing to copy either way. */
  region.take(host_buffer, host_address, 0u, sizeof block, CL_MAP_WRITE, block, MappedRegion::Memory::shared);
  CHECK(!region.staged());
  CHECK(!region.writes_back());
  CHECK(region.invariants_hold()); }

TEST(mapped_region, giving_it_up_leaves_it_answering_for_nothing)
{ MappedRegion region;
  std::uint8_t block[64] = {};
  region.take(host_buffer, host_address, 0u, sizeof block, CL_MAP_READ, block, MappedRegion::Memory::staged);
  region.give_up();
  CHECK(!region.held());
  CHECK(!region.answers(block));
  CHECK_EQ(region.buffer(), 0u);
  CHECK_EQ(region.host(), 0u);
  CHECK(region.invariants_hold()); }

TEST(guest_cl_context, a_mapping_is_taken_from_the_context_and_given_back_to_it)
{ ClContextRig rig;
  rig.host().blob_import(0u, 0x77u);
  GuestClContext context(rig.connection(), rig.platform(), rig.send_lock(), rig.blocks_lock(), rig.log(), rig.handles());
  CHECK_STATUS(context.make(), Status::ok);

  std::uint8_t *const at = context.take_mapping(host_buffer, host_address, 0u, 128u, CL_MAP_READ);
  CHECK(at != nullptr);
  CHECK_EQ(context.mappings(), 1u);
  MappedRegion *const region = context.mapping_at(at);
  CHECK(region != nullptr);
  CHECK(region->answers(at));
  /* A pointer no live mapping answered is not one this context can unmap. */
  CHECK(context.mapping_at(at + 1) == nullptr);
  CHECK(context.mapping_at(nullptr) == nullptr);
  CHECK(context.invariants_hold());

  context.give_back_mapping(*region);
  CHECK_EQ(context.mappings(), 0u);
  CHECK(context.mapping_at(at) == nullptr);
  CHECK(context.invariants_hold()); }

TEST(guest_cl_context, a_mapping_of_nothing_or_of_no_buffer_is_refused)
{ ClContextRig rig;
  rig.host().blob_import(0u, 0x77u);
  GuestClContext context(rig.connection(), rig.platform(), rig.send_lock(), rig.blocks_lock(), rig.log(), rig.handles());
  CHECK_STATUS(context.make(), Status::ok);
  CHECK(context.take_mapping(host_buffer, host_address, 0u, 0u, CL_MAP_READ) == nullptr);
  CHECK(context.take_mapping(0u, host_address, 0u, 16u, CL_MAP_READ) == nullptr);
  /* And one the host answered no address for: an unmap would have nothing to
   * name. */
  CHECK(context.take_mapping(host_buffer, 0u, 0u, 16u, CL_MAP_READ) == nullptr);
  CHECK_EQ(context.mappings(), 0u);
  CHECK(context.invariants_hold()); }

/* A mapping of the application's own memory is handed back where it is and
 * written back like a staged one, but never freed; and a buffer that goes
 * takes every mapping of its own with it, and none of another's. */
TEST(guest_cl_context, application_mappings_are_never_freed_and_a_released_buffer_takes_its_own)
{ ClContextRig rig;
  rig.host().blob_import(0u, 0x77u);
  GuestClContext context(rig.connection(), rig.platform(), rig.send_lock(), rig.blocks_lock(), rig.log(), rig.handles());
  CHECK_STATUS(context.make(), Status::ok);
  static std::uint8_t own[64];
  std::uint8_t *const at = context.take_application_mapping(host_buffer, host_address, 16u, 8u, CL_MAP_WRITE, own + 16);
  CHECK(at == own + 16);
  MappedRegion *const region = context.mapping_at(at);
  CHECK(region != nullptr);
  if (region == nullptr) return;
  CHECK(!region->staged());
  CHECK(region->writes_back());
  CHECK(context.take_application_mapping(host_buffer, host_address, 0u, 8u, CL_MAP_READ, nullptr) == nullptr);
  CHECK(context.take_application_mapping(0u, host_address, 0u, 8u, CL_MAP_READ, own) == nullptr);
  CHECK(context.take_mapping(host_buffer, host_address + 0x1000u, 0u, 16u, CL_MAP_READ) != nullptr);
  CHECK(context.take_mapping(host_buffer + 1u, host_address + 0x2000u, 0u, 16u, CL_MAP_READ) != nullptr);
  CHECK_EQ(context.mappings(), 3u);
  /* Two of the buffer's own - one the application's memory, one staged - go,
   * and the other buffer's stays. Giving back the application's memory frees
   * nothing: the mock platform aborts on a release of what it never lent. */
  CHECK_EQ(context.give_back_mappings_of(host_buffer), 2u);
  CHECK_EQ(context.mappings(), 1u);
  CHECK_EQ(context.give_back_mappings_of(host_buffer), 0u);
  CHECK(context.invariants_hold()); }

TEST(guest_cl_context, a_context_that_can_hold_no_more_mappings_refuses_and_stays_usable)
{ ClContextRig rig;
  rig.host().blob_import(0u, 0x77u);
  GuestClContext context(rig.connection(), rig.platform(), rig.send_lock(), rig.blocks_lock(), rig.log(), rig.handles());
  CHECK_STATUS(context.make(), Status::ok);
  /* One staged mapping, then the rest of the table with mappings of memory
   * the context already has - thousands of staged blocks are more than the
   * mock platform lends. */
  std::uint8_t *const first = context.take_mapping(host_buffer, host_address, 0u, 16u, CL_MAP_READ);
  CHECK(first != nullptr);
  static std::uint8_t shared[GuestClContext::max_mappings];
  for (std::uint32_t index = 1u; index < GuestClContext::max_mappings; ++index)
      CHECK(context.take_shared_mapping(host_buffer, host_address + index * 0x1000u, 0u, 1u, shared + index) !=
            nullptr);
  CHECK_EQ(context.mappings(), GuestClContext::max_mappings);
  CHECK(context.take_mapping(host_buffer, host_address, 0u, 16u, CL_MAP_READ) == nullptr);
  CHECK(context.invariants_hold());
  context.give_back_mapping(*context.mapping_at(first));
  CHECK(context.take_mapping(host_buffer, host_address, 0u, 16u, CL_MAP_READ) != nullptr);
  CHECK(context.invariants_hold()); }

TEST(guest_cl_context, mappings_left_live_go_with_the_context)
{ ClContextRig rig;
  rig.host().blob_import(0u, 0x77u);
  { GuestClContext context(rig.connection(), rig.platform(), rig.send_lock(), rig.blocks_lock(), rig.log(), rig.handles());
    CHECK_STATUS(context.make(), Status::ok);
    CHECK(context.take_mapping(host_buffer, host_address, 0u, 256u, CL_MAP_WRITE) != nullptr);
    CHECK_EQ(context.mappings(), 1u); }
  /* An application that never unmapped is an application that leaked nothing:
   * the blocks were the context's. */
  CHECK_EQ(rig.platform().live_allocations(), 0u); }

/* The invariant catches the halfway states the class never builds. */
TEST(mapped_region, the_invariant_catches_a_live_mapping_that_could_not_be_unmapped)
{ std::uint8_t at[64] = {};
  const auto broken = [&at](void (*corrupt)(MappedRegion &)) {
      MappedRegion region;
      region.take(0x6600u, 0x7000u, 0u, sizeof at, CL_MAP_WRITE, at, MappedRegion::Memory::staged);
      CHECK(region.invariants_hold());
      corrupt(region);
      return region.invariants_hold();
  };
  CHECK(!broken(MappedRegionProbe::forget_length));
  CHECK(!broken(MappedRegionProbe::forget_buffer));
  CHECK(!broken(MappedRegionProbe::forget_host_address)); }

TEST(mapped_region, the_invariant_catches_an_empty_mapping_that_still_names_something)
{ const auto broken = [](void (*corrupt)(MappedRegion &)) {
      MappedRegion region;
      CHECK(region.invariants_hold());
      corrupt(region);
      return region.invariants_hold();
  };
  CHECK(!broken(MappedRegionProbe::stray_buffer));
  CHECK(!broken(MappedRegionProbe::stray_length));
  CHECK(!broken(MappedRegionProbe::stray_offset));
  CHECK(!broken(MappedRegionProbe::stray_staging)); }
