/* Where in the device's hostmem region each host-visible allocation sits. The
 * protocol puts the offset in the guest's hands - RESOURCE_MAP_BLOB carries it
 * and the device only judges it - so this is the driver's allocator, and every
 * rule the device enforces is a rule this must already keep (feature 041). */
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

TEST(hostmem_placement, places_nothing_before_a_region_is_opened)
{ MockPlatform platform;
  HostmemPlacement placement(platform);
  std::uint64_t offset = 0xEEu;
  CHECK_STATUS(placement.place(page, owner(first_owner), offset), Status::no_space);
  CHECK_EQ(offset, 0u);
  CHECK_EQ(placement.live(), 0u);
  CHECK(placement.invariants_hold()); }

TEST(hostmem_placement_place, takes_the_lowest_offset_with_room)
{ MockPlatform platform;
  HostmemPlacement placement(platform);
  placement.open(region_bytes);
  std::uint64_t first = 0u, second = 0u, third = 0u;
  CHECK_STATUS(placement.place(2u * page, owner(first_owner), first), Status::ok);
  CHECK_STATUS(placement.place(page, owner(second_owner), second), Status::ok);
  CHECK_EQ(first, 0u);
  CHECK_EQ(second, 2u * page);
  /* The hole the first one leaves is the lowest room there is. */
  placement.release(owner(first_owner));
  CHECK_STATUS(placement.place(page, owner(third_owner), third), Status::ok);
  CHECK_EQ(third, 0u);
  CHECK_EQ(placement.live(), 2u);
  CHECK(placement.invariants_hold()); }

/* A blob is a page multiple or it is not a blob: the device refuses a
 * misaligned offset, and a size that is not a page multiple would leave one. */
TEST(hostmem_placement_place, rounds_a_request_up_to_whole_pages)
{ MockPlatform platform;
  HostmemPlacement placement(platform);
  placement.open(region_bytes);
  std::uint64_t first = 0u, second = 0u;
  CHECK_STATUS(placement.place(page + 1u, owner(first_owner), first), Status::ok);
  CHECK_STATUS(placement.place(page, owner(second_owner), second), Status::ok);
  CHECK_EQ(first, 0u);
  CHECK_EQ(second, 2u * page);   /* the first took two whole pages */
  CHECK(placement.invariants_hold()); }

TEST(hostmem_placement_place, refuses_what_the_region_cannot_hold)
{ MockPlatform platform;
  HostmemPlacement placement(platform);
  placement.open(region_bytes);
  std::uint64_t offset = 0xEEu;
  CHECK_STATUS(placement.place(region_bytes + page, owner(first_owner), offset), Status::no_space);
  CHECK_EQ(offset, 0u);
  CHECK_STATUS(placement.place(0u, owner(first_owner), offset), Status::invalid_parameter);
  CHECK_EQ(placement.live(), 0u);
  /* The whole region at once is not too much. */
  CHECK_STATUS(placement.place(region_bytes, owner(first_owner), offset), Status::ok);
  CHECK_EQ(offset, 0u);
  CHECK_STATUS(placement.place(page, owner(second_owner), offset), Status::no_space);
  CHECK(placement.invariants_hold()); }

/* An allocation places once: a second placement for the same owner would leave
 * the first unreachable and the region leaking. */
TEST(hostmem_placement_place, refuses_an_owner_that_already_holds_one)
{ MockPlatform platform;
  HostmemPlacement placement(platform);
  placement.open(region_bytes);
  std::uint64_t offset = 0u;
  CHECK_STATUS(placement.place(page, owner(first_owner), offset), Status::ok);
  CHECK_STATUS(placement.place(page, owner(first_owner), offset), Status::invalid_state);
  CHECK_EQ(placement.live(), 1u);
  CHECK_STATUS(placement.place(page, nullptr, offset), Status::invalid_parameter);
  CHECK(placement.invariants_hold()); }

TEST(hostmem_placement_release, frees_its_owner_s_range_and_nobody_else_s)
{ MockPlatform platform;
  HostmemPlacement placement(platform);
  placement.open(region_bytes);
  std::uint64_t first = 0u, second = 0u;
  CHECK_STATUS(placement.place(page, owner(first_owner), first), Status::ok);
  CHECK_STATUS(placement.place(page, owner(second_owner), second), Status::ok);
  placement.release(owner(first_owner));
  CHECK_EQ(placement.live(), 1u);
  CHECK(placement.holder(first) == nullptr);
  CHECK(placement.holder(second) == owner(second_owner));
  /* Releasing what was never placed changes nothing. */
  placement.release(owner(third_owner));
  placement.release(nullptr);
  CHECK_EQ(placement.live(), 1u);
  CHECK(placement.invariants_hold()); }

/* Which allocation holds a byte, so a mapping the device still has cannot be
 * mistaken for a live one. */
TEST(hostmem_placement_holder, names_the_allocation_that_holds_a_byte)
{ MockPlatform platform;
  HostmemPlacement placement(platform);
  placement.open(region_bytes);
  std::uint64_t offset = 0u;
  CHECK_STATUS(placement.place(2u * page, owner(first_owner), offset), Status::ok);
  CHECK(placement.holder(offset) == owner(first_owner));
  CHECK(placement.holder(offset + page) == owner(first_owner));
  CHECK(placement.holder(offset + 2u * page) == nullptr);
  CHECK(placement.holder(region_bytes) == nullptr); }

TEST(hostmem_placement, fills_up_and_says_so)
{ MockPlatform platform;
  HostmemPlacement placement(platform);
  placement.open(HostmemPlacement::max_blobs * page + page);
  int owners[HostmemPlacement::max_blobs + 1] = {};
  std::uint64_t offset = 0u;
  for (std::uint32_t index = 0u; index < HostmemPlacement::max_blobs; index++)
      CHECK_STATUS(placement.place(page, &owners[index], offset), Status::ok);
  CHECK_EQ(placement.live(), HostmemPlacement::max_blobs);
  CHECK_STATUS(placement.place(page, &owners[HostmemPlacement::max_blobs], offset), Status::no_space);
  CHECK(placement.invariants_hold()); }

/* StopDevice, or a device that turned out to be another one: nothing is
 * placed in a region that is gone. */
TEST(hostmem_placement_close, forgets_every_placement)
{ MockPlatform platform;
  HostmemPlacement placement(platform);
  placement.open(region_bytes);
  std::uint64_t offset = 0u;
  CHECK_STATUS(placement.place(page, owner(first_owner), offset), Status::ok);
  placement.close();
  CHECK_EQ(placement.live(), 0u);
  CHECK(placement.holder(0u) == nullptr);
  CHECK_STATUS(placement.place(page, owner(first_owner), offset), Status::no_space);
  CHECK(placement.invariants_hold()); }

TEST(hostmem_placement, is_passive_only)
{ MockPlatform platform;
  HostmemPlacement placement(platform);
  placement.open(region_bytes);
  platform.set_irql(Irql::dispatch);
  std::uint64_t offset = 0u;
  static_cast<void>(placement.place(page, owner(first_owner), offset));
  placement.release(owner(first_owner));
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  while (platform.claim(Violation::irql_ceiling)) {}
  /* The mutex it takes is a PASSIVE lock: taking one at DISPATCH is the
   * violation the ceiling exists to stop. */
  CHECK(platform.claim(Violation::blocking_at_dispatch));
  while (platform.claim(Violation::blocking_at_dispatch)) {} }
