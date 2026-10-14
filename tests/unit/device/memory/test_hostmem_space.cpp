/* The hostmem region host blobs are placed in: a placement fits only page
 * aligned, of at least one byte, and wholly inside the region the host says it
 * has - asked at each placement, since the host answers it. */
#include "device/memory/hostmem_space.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"

#include <cstdint>

using av::HostmemSpace;
using av::MockHost;

namespace {

constexpr std::uint64_t page = HostmemSpace::page_size;
constexpr std::uint64_t region = 16u * page;

}  // namespace

TEST(hostmem_space_admits, the_whole_region_and_a_last_page_that_ends_on_its_end)
{ MockHost host;
  host.set_hostmem_size(region);
  const HostmemSpace space(host);
  CHECK(space.admits(0u, region));
  CHECK(space.admits(region - page, page));
  CHECK(space.admits(3u * page, 2u * page)); }

TEST(hostmem_space_admits, refuses_a_placement_one_byte_past_the_end)
{ MockHost host;
  host.set_hostmem_size(region);
  const HostmemSpace space(host);
  CHECK(!space.admits(0u, region + 1u));
  CHECK(!space.admits(region - page, page + 1u));
  CHECK(!space.admits(region, page)); }

TEST(hostmem_space_admits, refuses_an_offset_that_is_not_page_aligned)
{ MockHost host;
  host.set_hostmem_size(region);
  const HostmemSpace space(host);
  CHECK(!space.admits(1u, page));
  CHECK(!space.admits(page - 1u, page));
  CHECK(!space.admits(page + page / 2u, page)); }

TEST(hostmem_space_admits, refuses_a_placement_of_no_bytes_even_inside)
{ MockHost host;
  host.set_hostmem_size(region);
  const HostmemSpace space(host);
  CHECK(!space.admits(0u, 0u));
  CHECK(!space.admits(region, 0u)); }

/* offset + size would wrap: the comparison is done without adding them. */
TEST(hostmem_space_admits, refuses_a_size_that_would_wrap_past_the_offset)
{ MockHost host;
  host.set_hostmem_size(region);
  const HostmemSpace space(host);
  CHECK(!space.admits(page, UINT64_MAX - page + 1u));
  CHECK(!space.admits(page, UINT64_MAX));
  CHECK(!space.admits(UINT64_MAX - page + 1u, page)); }

TEST(hostmem_space_admits, a_host_with_no_region_admits_nothing)
{ MockHost host;
  const HostmemSpace space(host);
  CHECK(!space.admits(0u, page));
  CHECK(!space.admits(0u, 0u)); }

/* The region is the host's answer at the time of asking, not a copy. */
TEST(hostmem_space_admits, follows_the_region_the_host_reports_now)
{ MockHost host;
  host.set_hostmem_size(region);
  const HostmemSpace space(host);
  CHECK(!space.admits(region, page));
  host.set_hostmem_size(region + page);
  CHECK(space.admits(region, page));
  host.set_hostmem_size(page);
  CHECK(!space.admits(page, page)); }
