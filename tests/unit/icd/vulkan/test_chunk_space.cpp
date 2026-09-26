/* One host-visible chunk of a connection and the page ranges placed in it
 * (research.md §7, contracts/host_visible_memory.md §1): opened as a bound
 * chunk the size the kernel answered, ranges placed first fit at whole pages
 * and removed by where they begin, closed only when nothing is placed.
 * Invariants after every change. */
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/open_connection.hpp"
#include "icd/vulkan/chunk_space.hpp"

#include <cstdint>
#include <cstring>

using av::MockKmt;
using av::OpenConnection;
using av::Status;
using av::icd::ChunkSpace;

namespace av::icd {

struct ChunkSpaceProbe {
    static void set_range(ChunkSpace &space, std::uint32_t index, std::uint64_t first, std::uint64_t pages)
    {
        space.firsts_[index] = first;
        space.lengths_[index] = pages;
    }
    static void set_placed(ChunkSpace &space, std::uint32_t placed) { space.placed_ = placed; }
    static void set_pages(ChunkSpace &space, std::uint64_t pages) { space.pages_ = pages; }
};

}  // namespace av::icd

using av::icd::ChunkSpaceProbe;

namespace {

constexpr std::uint64_t page = ChunkSpace::page_bytes;
constexpr std::uint64_t chunk_pages = 16384u;

std::uint64_t place(ChunkSpace &space, std::uint64_t pages)
{ std::uint64_t offset = ~std::uint64_t{0};
  CHECK_STATUS(space.place(pages, offset), Status::ok);
  CHECK(space.invariants_hold());
  return offset; }

}  // namespace

TEST(chunk_space, is_closed_and_places_nothing_until_opened)
{ OpenConnection      rig;
  const std::uint32_t made = rig.kmt().calls(MockKmt::Call::create_allocation);
  ChunkSpace          space(rig.connection());
  CHECK(space.invariants_hold());
  CHECK(!space.opened());
  CHECK(space.empty());
  CHECK_EQ(space.resource_id(), 0u);
  CHECK_EQ(space.pages(), 0u);
  CHECK(space.view() == nullptr);
  std::uint64_t offset = 7u;
  CHECK_STATUS(space.place(1u, offset), Status::invalid_state);
  CHECK_EQ(offset, 7u);
  CHECK_STATUS(space.remove(0u), Status::invalid_parameter);
  CHECK_STATUS(space.close(), Status::ok);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_allocation), made); }

TEST(chunk_space, opens_as_a_bound_chunk_of_the_pages_asked)
{ OpenConnection rig;
  ChunkSpace     space(rig.connection());
  CHECK_STATUS(space.open(chunk_pages), Status::ok);
  CHECK(space.invariants_hold());
  CHECK(space.opened());
  CHECK(space.empty());
  CHECK_EQ(space.pages(), chunk_pages);
  CHECK_EQ(space.resource_id(), MockKmt::first_chunk_resource);
  CHECK(space.view() != nullptr);
  CHECK_EQ(rig.kmt().allocation_pages(rig.kmt().bound_allocation(0u)), chunk_pages);
  CHECK_STATUS(space.open(chunk_pages), Status::invalid_state);
  CHECK_EQ(rig.kmt().bindings(), 1u);
  CHECK_STATUS(space.close(), Status::ok);
  CHECK(space.invariants_hold());
  CHECK(!space.opened());
  CHECK_EQ(space.pages(), 0u);
  CHECK_EQ(rig.kmt().bindings(), 0u);
  CHECK_EQ(rig.kmt().open_objects(), OpenConnection::link_objects); }

/* Zero pages, and a count whose bytes do not fit 64 bits, are not asked of the
 * kernel at all. */
TEST(chunk_space, a_size_that_is_no_chunk_is_refused_unasked)
{ OpenConnection      rig;
  const std::uint32_t made = rig.kmt().calls(MockKmt::Call::create_allocation);
  ChunkSpace          space(rig.connection());
  const std::uint64_t sizes[] = {0u, (~std::uint64_t{0} >> 12u) + 1u, ~std::uint64_t{0}};
  for (const std::uint64_t pages : sizes) {
      CHECK_STATUS(space.open(pages), Status::invalid_parameter);
      CHECK(!space.opened());
  }
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_allocation), made); }

TEST(chunk_space, the_kernel_s_refusal_is_the_answer)
{ OpenConnection rig;
  ChunkSpace     space(rig.connection());
  rig.kmt().fail_at(MockKmt::Call::escape, 2u, MockKmt::invalid_parameter);   /* the bind */
  CHECK(space.open(chunk_pages) != Status::ok);
  CHECK(space.invariants_hold());
  CHECK(!space.opened());
  CHECK_EQ(rig.kmt().open_objects(), OpenConnection::link_objects); }

/* A kernel that bound fewer pages than were asked for would have the device
 * refuse every import past them: refused, with the chunk given back. One that
 * gave more - a window grants whole pages of at least what was asked - is
 * taken, and only the pages asked are placed in. */
TEST(chunk_space, a_chunk_smaller_than_asked_is_refused_and_a_larger_one_used_as_asked)
{ { OpenConnection rig;
    ChunkSpace     space(rig.connection());
    rig.kmt().answer_next_bind_pages(chunk_pages - 1u);
    CHECK_STATUS(space.open(chunk_pages), Status::malformed);
    CHECK(space.invariants_hold());
    CHECK(!space.opened());
    CHECK_EQ(rig.kmt().bindings(), 0u);
    CHECK_EQ(rig.kmt().open_objects(), OpenConnection::link_objects); }
  { OpenConnection rig;
    ChunkSpace     space(rig.connection());
    rig.kmt().answer_next_bind_pages(chunk_pages + 1u);
    CHECK_STATUS(space.open(chunk_pages), Status::ok);
    CHECK_EQ(space.pages(), chunk_pages);
    std::uint64_t offset = 0u;
    CHECK_STATUS(space.place(chunk_pages, offset), Status::ok);
    CHECK_STATUS(space.place(1u, offset), Status::no_memory);
    CHECK_STATUS(space.remove(0u), Status::ok);
    CHECK_STATUS(space.close(), Status::ok);
    CHECK_EQ(rig.kmt().bindings(), 0u); } }

/* On a device with a host window the space is a chunk of it: its resource,
 * its view in the window, and placement exactly as over guest RAM. */
TEST(chunk_space, opens_over_a_window_chunk_when_the_kernel_grants_one)
{ OpenConnection rig;
  rig.kmt().open_host_window(2u * chunk_pages * page);
  const std::uint32_t made = rig.kmt().calls(MockKmt::Call::create_allocation);
  ChunkSpace space(rig.connection());
  CHECK_STATUS(space.open(chunk_pages), Status::ok);
  CHECK(space.invariants_hold());
  CHECK_EQ(space.pages(), chunk_pages);
  CHECK_EQ(space.resource_id(), MockKmt::first_window_resource);
  CHECK(space.view() == rig.kmt().window_base());
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_allocation), made);
  CHECK_EQ(rig.kmt().bindings(), 0u);
  CHECK_EQ(place(space, 2u), 0u);
  CHECK_EQ(place(space, 1u), 2u * page);
  CHECK_STATUS(space.close(), Status::invalid_state);
  CHECK_STATUS(space.remove(0u), Status::ok);
  CHECK_STATUS(space.remove(2u * page), Status::ok);
  CHECK_STATUS(space.close(), Status::ok);
  CHECK_EQ(rig.kmt().window_chunks(), 0u);
  CHECK_EQ(rig.kmt().open_objects(), OpenConnection::link_objects); }

TEST(chunk_space, places_ranges_first_fit_at_whole_pages)
{ OpenConnection rig;
  ChunkSpace     space(rig.connection());
  CHECK_STATUS(space.open(chunk_pages), Status::ok);
  CHECK_EQ(place(space, 1u), 0u);
  CHECK_EQ(place(space, 3u), 1u * page);
  CHECK_EQ(place(space, 2u), 4u * page);
  CHECK_EQ(space.placed(), 3u);
  CHECK(!space.empty());
  /* The hole the second range leaves takes a range that fits it, and one that
   * does not goes after the last. */
  CHECK_STATUS(space.remove(1u * page), Status::ok);
  CHECK(space.invariants_hold());
  CHECK_EQ(place(space, 4u), 6u * page);
  CHECK_EQ(place(space, 3u), 1u * page);
  CHECK_EQ(place(space, 1u), 10u * page);
  CHECK_STATUS(space.remove(0u), Status::ok);
  CHECK_EQ(place(space, 1u), 0u);
  CHECK_EQ(space.placed(), 5u);
  CHECK_STATUS(space.close(), Status::invalid_state);
  CHECK(space.opened()); }

/* A range as large as the chunk, then nothing more; one page more than the
 * chunk never. */
TEST(chunk_space, a_range_of_the_whole_chunk_fills_it)
{ OpenConnection rig;
  ChunkSpace     space(rig.connection());
  CHECK_STATUS(space.open(chunk_pages), Status::ok);
  std::uint64_t offset = 0u;
  CHECK_STATUS(space.place(chunk_pages + 1u, offset), Status::no_memory);
  CHECK(space.empty());
  CHECK_EQ(place(space, chunk_pages), 0u);
  CHECK_STATUS(space.place(1u, offset), Status::no_memory);
  CHECK_STATUS(space.remove(0u), Status::ok);
  CHECK(space.empty());
  CHECK_EQ(place(space, chunk_pages - 1u), 0u);
  CHECK_EQ(place(space, 1u), (chunk_pages - 1u) * page);
  CHECK_STATUS(space.place(1u, offset), Status::no_memory); }

/* Free pages scattered into holes too small for a range are not room. */
TEST(chunk_space, fragmentation_is_no_room)
{ OpenConnection rig;
  ChunkSpace     space(rig.connection());
  CHECK_STATUS(space.open(8u), Status::ok);
  for (std::uint64_t index = 0u; index < 8u; index++) CHECK_EQ(place(space, 1u), index * page);
  for (std::uint64_t index = 0u; index < 8u; index += 2u) CHECK_STATUS(space.remove(index * page), Status::ok);
  CHECK(space.invariants_hold());
  std::uint64_t offset = 0u;
  CHECK_STATUS(space.place(2u, offset), Status::no_memory);
  CHECK_EQ(place(space, 1u), 0u); }

TEST(chunk_space, zero_pages_are_no_range)
{ OpenConnection rig;
  ChunkSpace     space(rig.connection());
  CHECK_STATUS(space.open(chunk_pages), Status::ok);
  std::uint64_t offset = 5u;
  CHECK_STATUS(space.place(0u, offset), Status::invalid_parameter);
  CHECK_EQ(offset, 5u);
  CHECK(space.empty()); }

/* The ranges a chunk can hold are bounded, as a context's imports are. */
TEST(chunk_space, holds_at_most_its_ranges)
{ OpenConnection rig;
  ChunkSpace     space(rig.connection());
  CHECK_STATUS(space.open(chunk_pages), Status::ok);
  for (std::uint32_t index = 0u; index < ChunkSpace::max_ranges; index++) CHECK_EQ(place(space, 1u), index * page);
  std::uint64_t offset = 0u;
  CHECK_STATUS(space.place(1u, offset), Status::no_memory);
  CHECK_EQ(space.placed(), ChunkSpace::max_ranges);
  CHECK_STATUS(space.remove(100u * page), Status::ok);
  CHECK_EQ(place(space, 1u), 100u * page); }

/* Only where a range begins removes it: inside one, past the chunk, a page
 * boundary nothing starts at, and one already removed are refused. */
TEST(chunk_space, removes_a_range_only_where_it_begins)
{ OpenConnection rig;
  ChunkSpace     space(rig.connection());
  CHECK_STATUS(space.open(chunk_pages), Status::ok);
  CHECK_EQ(place(space, 4u), 0u);
  const std::uint64_t refused[] = {1u, page, 4u * page, chunk_pages * page, ~std::uint64_t{0}};
  for (const std::uint64_t offset : refused) CHECK_STATUS(space.remove(offset), Status::invalid_parameter);
  CHECK_EQ(space.placed(), 1u);
  CHECK_STATUS(space.remove(0u), Status::ok);
  CHECK_STATUS(space.remove(0u), Status::invalid_parameter);
  CHECK(space.invariants_hold()); }

/* Destroyed while open - its device or instance went - it gives the chunk back
 * whatever is still placed. */
TEST(chunk_space, destroying_it_gives_the_chunk_back)
{ OpenConnection rig;
  {
      ChunkSpace space(rig.connection());
      CHECK_STATUS(space.open(chunk_pages), Status::ok);
      CHECK_EQ(place(space, 2u), 0u);
  }
  CHECK_EQ(rig.kmt().bindings(), 0u);
  CHECK_EQ(rig.kmt().open_objects(), OpenConnection::link_objects);
  CHECK_EQ(rig.kmt().misuses(), 0u); }

TEST(chunk_space, a_refused_release_still_closes_it)
{ OpenConnection rig;
  ChunkSpace     space(rig.connection());
  CHECK_STATUS(space.open(chunk_pages), Status::ok);
  rig.kmt().fail_at(MockKmt::Call::unlock, 1u, MockKmt::invalid_parameter);
  CHECK(space.close() != Status::ok);
  CHECK(!space.opened());
  CHECK(space.invariants_hold()); }

TEST(chunk_space, invariants_refuse_ranges_that_overlap_leave_the_chunk_or_hold_nothing)
{ OpenConnection rig;
  ChunkSpace     space(rig.connection());
  ChunkSpaceProbe::set_placed(space, 1u);
  CHECK(!space.invariants_hold());
  ChunkSpaceProbe::set_placed(space, 0u);
  CHECK(space.invariants_hold());
  ChunkSpaceProbe::set_pages(space, 1u);
  CHECK(!space.invariants_hold());
  ChunkSpaceProbe::set_pages(space, 0u);
  CHECK_STATUS(space.open(16u), Status::ok);
  CHECK_EQ(place(space, 2u), 0u);
  CHECK_EQ(place(space, 2u), 2u * page);
  ChunkSpaceProbe::set_range(space, 1u, 1u, 2u);
  CHECK(!space.invariants_hold());
  ChunkSpaceProbe::set_range(space, 1u, 15u, 2u);
  CHECK(!space.invariants_hold());
  ChunkSpaceProbe::set_range(space, 1u, 4u, 0u);
  CHECK(!space.invariants_hold());
  ChunkSpaceProbe::set_range(space, 1u, ~std::uint64_t{0}, 2u);
  CHECK(!space.invariants_hold());
  ChunkSpaceProbe::set_range(space, 1u, 2u, 2u);
  CHECK(space.invariants_hold());
  ChunkSpaceProbe::set_placed(space, ChunkSpace::max_ranges + 1u);
  CHECK(!space.invariants_hold());
  ChunkSpaceProbe::set_placed(space, 2u);
  ChunkSpaceProbe::set_pages(space, 0u);
  CHECK(!space.invariants_hold());
  ChunkSpaceProbe::set_pages(space, 16u);
  CHECK(space.invariants_hold()); }
