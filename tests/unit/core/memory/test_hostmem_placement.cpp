/* Which host-visible allocation holds which part of the hostmem region. VidMm
 * chooses the offsets; this keeps the judgement the device would make of them,
 * so a refusal from the device is a bug caught here first (feature 041). */
#include "core/memory/hostmem_placement.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>

using av::HostmemPlacement;
using av::Irql;
using av::MockPlatform;
using av::Status;
using av::Violation;

namespace {

constexpr std::uint64_t page = 0x1000u;
constexpr std::uint64_t region_bytes = 64u * page;
/* Handles the allocations would be; nothing is dereferenced. */
int first_owner = 0;
int second_owner = 0;
int third_owner = 0;

void *owner(int &of) { return &of; }

}  // namespace

TEST(hostmem_placement, holds_nothing_before_a_region_is_opened)
{ MockPlatform platform;
  HostmemPlacement placement(platform);
  CHECK_STATUS(placement.take(0u, page, owner(first_owner)), Status::invalid_parameter);
  CHECK_EQ(placement.live(), 0u);
  CHECK(placement.invariants_hold()); }

TEST(hostmem_placement_take, records_what_vidmm_placed)
{ MockPlatform platform;
  HostmemPlacement placement(platform);
  placement.open(region_bytes);
  CHECK_STATUS(placement.take(4u * page, 2u * page, owner(first_owner)), Status::ok);
  CHECK_STATUS(placement.take(0u, page, owner(second_owner)), Status::ok);
  CHECK_EQ(placement.live(), 2u);
  CHECK(placement.holder(0u) == owner(second_owner));
  CHECK(placement.holder(4u * page) == owner(first_owner));
  CHECK(placement.holder(5u * page) == owner(first_owner));
  CHECK(placement.holder(page) == nullptr);
  CHECK(placement.invariants_hold()); }

/* Two allocations in the same bytes is what the device refuses and what a
 * process must never be handed: whichever arrives second is wrong. */
TEST(hostmem_placement_take, refuses_a_range_another_allocation_holds)
{ MockPlatform platform;
  HostmemPlacement placement(platform);
  placement.open(region_bytes);
  CHECK_STATUS(placement.take(4u * page, 2u * page, owner(first_owner)), Status::ok);
  CHECK_STATUS(placement.take(4u * page, page, owner(second_owner)), Status::conflict);
  CHECK_STATUS(placement.take(5u * page, 2u * page, owner(second_owner)), Status::conflict);
  CHECK_STATUS(placement.take(3u * page, 2u * page, owner(second_owner)), Status::conflict);
  CHECK_EQ(placement.live(), 1u);
  /* The pages either side of it are free. */
  CHECK_STATUS(placement.take(3u * page, page, owner(second_owner)), Status::ok);
  CHECK_STATUS(placement.take(6u * page, page, owner(third_owner)), Status::ok);
  CHECK(placement.invariants_hold()); }

/* A blob is whole pages from a page boundary: the device refuses anything else,
 * and a range that runs past the region would map memory that is not there. */
TEST(hostmem_placement_take, refuses_what_the_device_would)
{ MockPlatform platform;
  HostmemPlacement placement(platform);
  placement.open(region_bytes);
  CHECK_STATUS(placement.take(page + 1u, page, owner(first_owner)), Status::invalid_parameter);
  CHECK_STATUS(placement.take(0u, 0u, owner(first_owner)), Status::invalid_parameter);
  CHECK_STATUS(placement.take(0u, page, nullptr), Status::invalid_parameter);
  CHECK_STATUS(placement.take(region_bytes, page, owner(first_owner)), Status::invalid_parameter);
  CHECK_STATUS(placement.take(region_bytes - page, 2u * page, owner(first_owner)), Status::invalid_parameter);
  CHECK_STATUS(placement.take(0u, region_bytes + page, owner(first_owner)), Status::invalid_parameter);
  CHECK_EQ(placement.live(), 0u);
  /* The last page of the region, and the whole region, are both legal. */
  CHECK_STATUS(placement.take(region_bytes - page, page, owner(first_owner)), Status::ok);
  CHECK_STATUS(placement.take(0u, region_bytes - page, owner(second_owner)), Status::ok);
  CHECK(placement.invariants_hold()); }

/* A size that is not a page multiple takes the pages it spans, because that is
 * what the device gives it. */
TEST(hostmem_placement_take, a_partial_page_takes_a_whole_one)
{ MockPlatform platform;
  HostmemPlacement placement(platform);
  placement.open(region_bytes);
  CHECK_STATUS(placement.take(0u, page + 1u, owner(first_owner)), Status::ok);
  CHECK_STATUS(placement.take(page, page, owner(second_owner)), Status::conflict);
  CHECK_STATUS(placement.take(2u * page, page, owner(second_owner)), Status::ok);
  CHECK(placement.invariants_hold()); }

/* An allocation holds one range: a second would leave the first unreachable and
 * the region leaking. */
TEST(hostmem_placement_take, refuses_an_owner_that_already_holds_one)
{ MockPlatform platform;
  HostmemPlacement placement(platform);
  placement.open(region_bytes);
  CHECK_STATUS(placement.take(0u, page, owner(first_owner)), Status::ok);
  CHECK_STATUS(placement.take(2u * page, page, owner(first_owner)), Status::invalid_state);
  CHECK_EQ(placement.live(), 1u);
  CHECK(placement.invariants_hold()); }

TEST(hostmem_placement_release, frees_its_owner_s_range_and_nobody_else_s)
{ MockPlatform platform;
  HostmemPlacement placement(platform);
  placement.open(region_bytes);
  CHECK_STATUS(placement.take(0u, page, owner(first_owner)), Status::ok);
  CHECK_STATUS(placement.take(page, page, owner(second_owner)), Status::ok);
  placement.release(owner(first_owner));
  CHECK_EQ(placement.live(), 1u);
  CHECK(placement.holder(0u) == nullptr);
  CHECK(placement.holder(page) == owner(second_owner));
  /* The range it held is free for the next allocation. */
  CHECK_STATUS(placement.take(0u, page, owner(third_owner)), Status::ok);
  /* Releasing what was never placed changes nothing. */
  placement.release(owner(first_owner));
  placement.release(nullptr);
  CHECK_EQ(placement.live(), 2u);
  CHECK(placement.invariants_hold()); }

TEST(hostmem_placement_holder, answers_nothing_past_the_region)
{ MockPlatform platform;
  HostmemPlacement placement(platform);
  placement.open(region_bytes);
  CHECK_STATUS(placement.take(0u, page, owner(first_owner)), Status::ok);
  CHECK(placement.holder(region_bytes) == nullptr);
  CHECK(placement.holder(0xFFFFFFFFFFFFF000ull) == nullptr); }

TEST(hostmem_placement, fills_up_and_says_so)
{ MockPlatform platform;
  HostmemPlacement placement(platform);
  placement.open((HostmemPlacement::max_blobs + 2u) * page);
  int owners[HostmemPlacement::max_blobs + 1] = {};
  for (std::uint32_t index = 0u; index < HostmemPlacement::max_blobs; index++)
      CHECK_STATUS(placement.take(index * page, page, &owners[index]), Status::ok);
  CHECK_EQ(placement.live(), HostmemPlacement::max_blobs);
  CHECK_STATUS(placement.take(HostmemPlacement::max_blobs * page, page,
                              &owners[HostmemPlacement::max_blobs]), Status::no_space);
  CHECK(placement.invariants_hold()); }

/* StopDevice, or a device that turned out to be another one: nothing is held in
 * a region that is gone. */
TEST(hostmem_placement_close, forgets_every_placement)
{ MockPlatform platform;
  HostmemPlacement placement(platform);
  placement.open(region_bytes);
  CHECK_STATUS(placement.take(0u, page, owner(first_owner)), Status::ok);
  placement.close();
  CHECK_EQ(placement.live(), 0u);
  CHECK(placement.holder(0u) == nullptr);
  CHECK_STATUS(placement.take(0u, page, owner(first_owner)), Status::invalid_parameter);
  CHECK(placement.invariants_hold()); }

TEST(hostmem_placement, is_passive_only)
{ MockPlatform platform;
  HostmemPlacement placement(platform);
  placement.open(region_bytes);
  platform.set_irql(Irql::dispatch);
  static_cast<void>(placement.take(0u, page, owner(first_owner)));
  placement.release(owner(first_owner));
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  while (platform.claim(Violation::irql_ceiling)) {}
  /* The mutex it takes is a PASSIVE lock: taking one at DISPATCH is the
   * violation the ceiling exists to stop. */
  CHECK(platform.claim(Violation::blocking_at_dispatch));
  while (platform.claim(Violation::blocking_at_dispatch)) {} }
