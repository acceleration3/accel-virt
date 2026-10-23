/* A device's host blobs by the ids HostBlobId gives them: each one memory the
 * context plane allocated for one context, judged before it is kept, placed
 * apart from every other blob in the hostmem region, and freed back to the
 * library under the same id when the guest releases it. */
#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
#include "device/abi/av_executor.h"
#include "device/memory/host_blob_owners.hpp"
#include "device/memory/guest_ram_lendings.hpp"
#include "device/memory/host_blob_table.hpp"
#include "harness/context/recording_context_plane.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"
#include "device/control/control_request.hpp"
#include "harness/memory/host_blob_table_probe.hpp"
#include "proto/resources/host_blob_id.hpp"
#include "proto/resources/host_memory_request.hpp"

#include <cstdint>
#include <cstring>

using av::HostBlobId;
using av::HostBlobOwners;
using av::HostBlobTable;
using av::HostMemoryRequest;
using av::HostViolation;
using av::MockHost;
using av::RecordingContextPlane;
using av::Status;

namespace {

constexpr std::uint64_t page = 0x1000u;
constexpr std::uint32_t first = HostBlobId::first;
constexpr std::uint32_t context = 3u;

HostMemoryRequest upload() { return HostMemoryRequest(HostMemoryRequest::Kind::upload, 0u); }

class Fixture {
public:
    Fixture() : table_(host_)
    {
        host_.set_hostmem_size(16u * page);
        CHECK_STATUS(table_.initialize(plane_), Status::ok);
    }

    MockHost              &host() { return host_; }
    RecordingContextPlane &plane() { return plane_; }
    HostBlobTable         &table() { return table_; }

private:
    MockHost              host_;
    RecordingContextPlane plane_;
    HostBlobTable         table_;
};

}  // namespace

TEST(host_blob_table_create, allocates_the_memory_asked_for_the_context_under_the_id)
{ Fixture fixture;
  const HostMemoryRequest vulkan(HostMemoryRequest::Kind::vulkan, 5u);
  CHECK_STATUS(fixture.table().create(first + 4u, context, vulkan, 2u * page), Status::ok);
  CHECK(fixture.table().contains(first + 4u));
  CHECK_EQ(fixture.table().live(), 1u);
  CHECK_EQ(fixture.plane().memory_allocations(), 1u);
  CHECK_EQ(fixture.plane().last_memory().context, context);
  CHECK_EQ(fixture.plane().last_memory().key, first + 4u);
  CHECK_EQ(fixture.plane().last_memory().size, 2u * page);
  CHECK_EQ(fixture.plane().last_memory().request, vulkan.encoded());
  CHECK(fixture.table().owns(context, first + 4u));
  CHECK(fixture.table().invariants_hold()); }

TEST(host_blob_table_create, refuses_an_id_outside_the_range_or_one_already_live)
{ Fixture fixture;
  CHECK_STATUS(fixture.table().create(first - 1u, context, upload(), page), Status::not_found);
  CHECK_STATUS(fixture.table().create(0u, context, upload(), page), Status::not_found);
  CHECK_STATUS(fixture.table().create(0xFFFFFFFFu, context, upload(), page), Status::ok);
  CHECK_STATUS(fixture.table().create(first, context, upload(), page), Status::ok);
  CHECK_STATUS(fixture.table().create(first, context, upload(), page), Status::conflict);
  CHECK_EQ(fixture.plane().memory_allocations(), 2u);   /* nothing asked for a refused id */
  CHECK_EQ(fixture.table().live(), 2u);
  CHECK(fixture.table().invariants_hold()); }

TEST(host_blob_table_create, refuses_a_request_or_size_no_blob_can_have_before_allocating)
{ Fixture fixture;
  CHECK_STATUS(fixture.table().create(first, context, HostMemoryRequest(), page), Status::invalid_parameter);
  CHECK_STATUS(fixture.table().create(first, context, HostMemoryRequest(0x109u), page), Status::invalid_parameter);
  CHECK_STATUS(fixture.table().create(first, context, upload(), 0u), Status::invalid_parameter);
  CHECK_STATUS(fixture.table().create(first, context, upload(), page + 1u), Status::invalid_parameter);
  CHECK_STATUS(fixture.table().create(first, context, upload(), page - 1u), Status::invalid_parameter);
  CHECK_EQ(fixture.plane().memory_allocations(), 0u);
  CHECK_EQ(fixture.table().live(), 0u);
  CHECK_STATUS(fixture.table().create(first, context, HostMemoryRequest(HostMemoryRequest::Kind::readback, 0u), page),
               Status::ok); }

/* The 16384 blobs the device once kept are no bound: blobs well past them
 * are made, found by their ids wherever in the range they fall, placed in any
 * order without one landing on another, and released. */
TEST(host_blob_table_create, keeps_blobs_well_past_the_old_range)
{ MockHost              host;
  RecordingContextPlane plane;
  HostBlobTable         table(host);
  host.set_hostmem_size(400u * page);
  CHECK_STATUS(table.initialize(plane), Status::ok);
  constexpr std::uint32_t many = 20000u;
  for (std::uint32_t index = 0u; index < many; index++)
      CHECK_STATUS(table.create(first + index * 7u, context + index % 5u, upload(), page), Status::ok);
  CHECK_EQ(table.live(), many);
  CHECK(table.invariants_hold());
  CHECK(table.owns(context + (many - 1u) % 5u, first + (many - 1u) * 7u));
  CHECK(!table.contains(first + 1u));
  std::uint32_t caching = 0u;
  for (std::uint32_t placed = 0u; placed < 200u; placed++) {
      const std::uint32_t at = (placed * 37u) % 200u;
      CHECK_STATUS(table.map(first + placed * 7u, std::uint64_t{at} * 2u * page, caching), Status::ok);
  }
  CHECK(table.invariants_hold());
  CHECK_STATUS(table.map(first + 300u * 7u, 2u * page, caching), Status::invalid_parameter);   /* on another */
  CHECK_STATUS(table.map(first + 300u * 7u, 3u * page, caching), Status::ok);                  /* between two */
  CHECK_STATUS(table.unmap(first + 300u * 7u), Status::ok);
  CHECK_EQ(host.live_placements(), 200u);
  table.release_all();
  CHECK_EQ(table.live(), 0u);
  CHECK_EQ(host.live_placements(), 0u);
  CHECK(table.invariants_hold()); }

/* A host with no memory to keep one more blob in refuses the create as out of
 * memory before the library is asked, and the next create is kept. */
TEST(host_blob_table_create, a_table_that_cannot_grow_is_out_of_memory_before_the_library_is_asked)
{ Fixture fixture;
  fixture.host().fail_allocation_at(1u);
  CHECK_STATUS(fixture.table().create(first, context, upload(), page), Status::no_memory);
  CHECK_EQ(fixture.plane().memory_allocations(), 0u);
  CHECK(!fixture.table().contains(first));
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().create(first, context, upload(), page), Status::ok);
  CHECK(fixture.table().contains(first)); }

/* The pool the blob lives in can be the one thing short of memory: the
 * library's memory is closed and freed under the id, and nothing is kept. */
TEST(host_blob_table_create, a_blob_with_nowhere_to_live_gives_the_library_s_memory_back)
{ Fixture fixture;
  /* A pool chunk holds 64: the 65th blob takes a second chunk, which goes
   * back when it is released, while the id table keeps its room - so the
   * next 65th needs a chunk and nothing else. */
  for (std::uint32_t index = 0u; index <= 64u; index++)
      CHECK_STATUS(fixture.table().create(first + index, context, upload(), page), Status::ok);
  CHECK_STATUS(fixture.table().release(first + 64u), Status::ok);
  CHECK_EQ(fixture.table().live(), 64u);
  fixture.host().fail_allocation_at(1u);
  CHECK_STATUS(fixture.table().create(first + 64u, context, upload(), page), Status::no_memory);
  CHECK_EQ(fixture.plane().memory_allocations(), 66u);
  CHECK_EQ(fixture.plane().freed_count(), 2u);
  CHECK_EQ(fixture.plane().freed(1u), first + 64u);
  CHECK(!fixture.table().contains(first + 64u));
  CHECK_EQ(fixture.table().live(), 64u);
  CHECK(fixture.table().invariants_hold()); }

TEST(host_blob_table_create, memory_the_library_refuses_is_out_of_memory_and_leaves_nothing)
{ Fixture fixture;
  fixture.plane().refuse_memory_at(1u);
  CHECK_STATUS(fixture.table().create(first, context, upload(), page), Status::no_memory);
  CHECK(!fixture.table().contains(first));
  CHECK_EQ(fixture.plane().freed_count(), 0u);   /* nothing was allocated to free */
  CHECK_STATUS(fixture.table().create(first, context, upload(), page), Status::ok); }

/* An answer the ABI does not allow - no descriptor, less memory than asked,
 * a caching no guest mapping can match - is memory the device cannot place:
 * its descriptor is closed and the key freed at once. */
TEST(host_blob_table_create, an_answer_breaking_the_abi_is_refused_and_given_back)
{ Fixture without;
  without.plane().answer_without_descriptor();
  CHECK_STATUS(without.table().create(first, context, upload(), page), Status::no_memory);
  CHECK_EQ(without.plane().freed_count(), 1u);
  CHECK_EQ(without.plane().freed(0u), first);
  CHECK_EQ(without.table().live(), 0u);
  Fixture short_answer;
  short_answer.plane().answer_short();
  CHECK_STATUS(short_answer.table().create(first + 1u, context, upload(), 2u * page), Status::no_memory);
  CHECK_EQ(short_answer.plane().freed(0u), first + 1u);
  CHECK_EQ(short_answer.table().live(), 0u);
  const std::uint32_t refused[] = {0u, 3u, 0xFFFFFFFFu};
  for (const std::uint32_t caching : refused) {
      Fixture uncached;
      uncached.plane().answer_caching(caching);
      CHECK_STATUS(uncached.table().create(first, context, upload(), page), Status::no_memory);
      CHECK_EQ(uncached.plane().freed_count(), 1u);
      CHECK_EQ(uncached.table().live(), 0u);
  }
  CHECK_EQ(short_answer.host().unclaimed_violations(), 0u); }

/* The library may round an allocation up; the blob is exactly what the guest
 * placed a window range for, or its mapping would run into the next range. */
TEST(host_blob_table_create, more_memory_than_asked_is_kept_to_the_size_asked)
{ Fixture fixture;
  fixture.plane().answer_long();
  CHECK_STATUS(fixture.table().create(first, context, upload(), page), Status::ok);
  std::uint32_t caching = 0u;
  CHECK_STATUS(fixture.table().map(first, 0u, caching), Status::ok);
  CHECK_EQ(fixture.host().last_placement().size, page);
  CHECK_STATUS(fixture.table().create(first + 1u, context, upload(), page), Status::ok);
  CHECK_STATUS(fixture.table().map(first + 1u, page, caching), Status::ok);   /* the next page is free */
  CHECK_STATUS(fixture.table().release(first), Status::ok); }

TEST(host_blob_table_map, places_a_blob_and_answers_how_the_host_caches_it)
{ Fixture fixture;
  CHECK_STATUS(fixture.table().create(first, context, upload(), 2u * page), Status::ok);
  fixture.plane().answer_caching(AV_MEMORY_WRITE_COMBINED);
  CHECK_STATUS(fixture.table().create(first + 1u, context, upload(), page), Status::ok);
  std::uint32_t caching = 0u;
  CHECK_STATUS(fixture.table().map(first, 4u * page, caching), Status::ok);
  CHECK_EQ(caching, static_cast<std::uint32_t>(AV_MEMORY_WRITE_BACK));
  CHECK(fixture.host().placed_at(4u * page) != nullptr);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().map(first + 1u, 6u * page, caching), Status::ok);   /* right after it */
  CHECK_EQ(caching, static_cast<std::uint32_t>(AV_MEMORY_WRITE_COMBINED));
  CHECK_EQ(fixture.host().live_placements(), 2u);
  CHECK(fixture.table().invariants_hold()); }

TEST(host_blob_table_map, keeps_placements_inside_the_region_and_apart)
{ Fixture fixture;
  CHECK_STATUS(fixture.table().create(first, context, upload(), 2u * page), Status::ok);
  CHECK_STATUS(fixture.table().create(first + 1u, context, upload(), 2u * page), Status::ok);
  std::uint32_t caching = 77u;
  CHECK_STATUS(fixture.table().map(first + 2u, 0u, caching), Status::not_found);
  CHECK_STATUS(fixture.table().map(first - 1u, 0u, caching), Status::not_found);
  CHECK_STATUS(fixture.table().map(first, 15u * page, caching), Status::invalid_parameter);   /* runs past the end */
  CHECK_STATUS(fixture.table().map(first, 16u * page, caching), Status::invalid_parameter);
  CHECK_STATUS(fixture.table().map(first, page + 1u, caching), Status::invalid_parameter);    /* misaligned */
  CHECK_EQ(caching, 77u);
  CHECK_STATUS(fixture.table().map(first, 14u * page, caching), Status::ok);   /* the last two pages */
  CHECK_STATUS(fixture.table().map(first, 0u, caching), Status::invalid_parameter);   /* already placed */
  CHECK_STATUS(fixture.table().map(first + 1u, 13u * page, caching), Status::invalid_parameter);   /* over it */
  CHECK_STATUS(fixture.table().map(first + 1u, 12u * page, caching), Status::ok);
  CHECK_EQ(fixture.host().live_placements(), 2u);
  CHECK_EQ(fixture.host().unclaimed_violations(), 0u);
  CHECK(fixture.table().invariants_hold()); }

TEST(host_blob_table_map, a_host_that_cannot_map_it_is_out_of_memory_and_it_can_be_placed_later)
{ Fixture fixture;
  CHECK_STATUS(fixture.table().create(first, context, upload(), page), Status::ok);
  fixture.host().fail_placement_at(1u);
  std::uint32_t caching = 77u;
  CHECK_STATUS(fixture.table().map(first, 0u, caching), Status::no_memory);
  CHECK_EQ(caching, 77u);
  CHECK_EQ(fixture.host().live_placements(), 0u);
  CHECK_STATUS(fixture.table().map(first, 0u, caching), Status::ok); }

/* A placement there is no memory to note is never made: the host is not
 * asked to place it, and the blob can be placed once memory is back. */
TEST(host_blob_table_map, a_placement_that_cannot_be_noted_is_out_of_memory_and_not_made)
{ Fixture fixture;
  CHECK_STATUS(fixture.table().create(first, context, upload(), page), Status::ok);
  fixture.host().fail_allocation_at(1u);
  std::uint32_t caching = 77u;
  CHECK_STATUS(fixture.table().map(first, 0u, caching), Status::no_memory);
  CHECK_EQ(caching, 77u);
  CHECK_EQ(fixture.host().live_placements(), 0u);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().map(first, 0u, caching), Status::ok);
  CHECK_EQ(fixture.host().live_placements(), 1u); }

TEST(host_blob_table_unmap, takes_a_placement_out_and_frees_its_pages_for_another)
{ Fixture fixture;
  CHECK_STATUS(fixture.table().create(first, context, upload(), page), Status::ok);
  CHECK_STATUS(fixture.table().create(first + 1u, context, upload(), page), Status::ok);
  CHECK_STATUS(fixture.table().unmap(first + 2u), Status::not_found);
  CHECK_STATUS(fixture.table().unmap(first), Status::invalid_parameter);   /* never placed */
  std::uint32_t caching = 0u;
  CHECK_STATUS(fixture.table().map(first, 0u, caching), Status::ok);
  CHECK_STATUS(fixture.table().unmap(first), Status::ok);
  CHECK_EQ(fixture.host().live_placements(), 0u);
  CHECK_STATUS(fixture.table().unmap(first), Status::invalid_parameter);
  CHECK_STATUS(fixture.table().map(first + 1u, 0u, caching), Status::ok);   /* the pages are free */
  CHECK(fixture.table().contains(first));
  CHECK(fixture.table().invariants_hold()); }

/* Released, a blob is out of the region, its descriptor closed and its key
 * freed in the library - under the id the guest named it by. */
TEST(host_blob_table_release, frees_the_memory_under_its_id_and_takes_it_out_of_the_region)
{ Fixture fixture;
  CHECK_STATUS(fixture.table().release(first), Status::not_found);
  CHECK_STATUS(fixture.table().release(7u), Status::not_found);
  CHECK_STATUS(fixture.table().create(first + 9u, context, upload(), page), Status::ok);
  const int fd = fixture.host().last_placement().fd;
  std::uint32_t caching = 0u;
  CHECK_STATUS(fixture.table().map(first + 9u, 0u, caching), Status::ok);
  const int placed = fixture.host().last_placement().fd;
  CHECK(fd != placed);
  CHECK(fixture.host().descriptor_open(placed));
  CHECK_STATUS(fixture.table().release(first + 9u), Status::ok);
  CHECK(!fixture.table().contains(first + 9u));
  CHECK(!fixture.host().descriptor_open(placed));
  CHECK_EQ(fixture.host().live_placements(), 0u);
  CHECK_EQ(fixture.plane().freed_count(), 1u);
  CHECK_EQ(fixture.plane().freed(0u), first + 9u);
  CHECK(!fixture.table().owns(context, first + 9u));
  CHECK_STATUS(fixture.table().release(first + 9u), Status::not_found);
  CHECK_STATUS(fixture.table().create(first + 9u, context, upload(), page), Status::ok);   /* the id is free again */
  CHECK(fixture.table().invariants_hold()); }

TEST(host_blob_table_release_all, frees_every_blob_as_a_reset_does)
{ Fixture fixture;
  CHECK_STATUS(fixture.table().create(first, context, upload(), page), Status::ok);
  CHECK_STATUS(fixture.table().create(first + 2u, context + 1u, upload(), page), Status::ok);
  std::uint32_t caching = 0u;
  CHECK_STATUS(fixture.table().map(first + 2u, 0u, caching), Status::ok);
  fixture.table().release_all();
  CHECK_EQ(fixture.table().live(), 0u);
  CHECK_EQ(fixture.host().live_placements(), 0u);
  CHECK_EQ(fixture.plane().freed_count(), 2u);
  CHECK_EQ(fixture.plane().freed(0u), first);
  CHECK_EQ(fixture.plane().freed(1u), first + 2u);
  fixture.table().release_all();   /* nothing left: nothing more freed */
  CHECK_EQ(fixture.plane().freed_count(), 2u);
  CHECK(fixture.table().invariants_hold()); }

/* A table going away takes its blobs out of the region and closes them, but
 * frees nothing in the library: the device's domain takes its memory. */
TEST(host_blob_table, going_away_closes_its_blobs_and_leaves_the_library_s_memory_to_the_domain)
{ MockHost host;
  host.set_hostmem_size(16u * page);
  RecordingContextPlane plane;
  int placed = -1;
  { HostBlobTable table(host);
    CHECK_STATUS(table.initialize(plane), Status::ok);
    CHECK_STATUS(table.create(first, context, upload(), page), Status::ok);
    std::uint32_t caching = 0u;
    CHECK_STATUS(table.map(first, 0u, caching), Status::ok);
    placed = host.last_placement().fd; }
  CHECK(!host.descriptor_open(placed));
  CHECK_EQ(host.live_placements(), 0u);
  CHECK_EQ(plane.freed_count(), 0u);
  CHECK_EQ(host.live_mutexes(), 0u); }

TEST(host_blob_table_owns, answers_only_for_a_live_blob_of_that_context)
{ Fixture fixture;
  CHECK(!fixture.table().owns(context, first));
  CHECK_STATUS(fixture.table().create(first, context, upload(), page), Status::ok);
  CHECK(fixture.table().owns(context, first));
  CHECK(!fixture.table().owns(context + 1u, first));
  CHECK(!fixture.table().owns(context, first + 1u));
  CHECK(!fixture.table().owns(context, first - 1u));
  CHECK(!fixture.table().owns(context, 0u));
  CHECK(!fixture.table().contains(first - 1u));
  CHECK(!fixture.table().contains(0xFFFFFFFFu));
  CHECK(!fixture.table().contains(first + 1u));
  CHECK(fixture.table().contains(first));
  CHECK_STATUS(fixture.table().release(first), Status::ok);
  CHECK(!fixture.table().owns(context, first)); }

/* A context's imports ask through HostBlobOwners, the table's one face to
 * them: the same answers come back through it. */
TEST(host_blob_table_owns, answers_the_same_through_the_owners_a_context_asks)
{ Fixture fixture;
  HostBlobOwners &owners = fixture.table();
  CHECK_STATUS(fixture.table().create(first, context, upload(), page), Status::ok);
  CHECK(owners.owns(context, first));
  CHECK(!owners.owns(context + 1u, first));
  CHECK_STATUS(fixture.table().release(first), Status::ok);
  CHECK(!owners.owns(context, first)); }

TEST(host_blob_table_initialize, nothing_is_made_before_the_lock_exists_and_it_is_made_once)
{ MockHost host;
  host.set_hostmem_size(16u * page);
  RecordingContextPlane plane;
  { HostBlobTable table(host);
    CHECK_STATUS(table.create(first, context, upload(), page), Status::invalid_state);
    CHECK(!table.owns(context, first));
    CHECK_EQ(plane.memory_allocations(), 0u);
    CHECK(table.invariants_hold());
    host.fail_mutex_at(1u);
    CHECK_STATUS(table.initialize(plane), Status::no_memory);
    CHECK_EQ(host.live_mutexes(), 0u);
    CHECK_STATUS(table.create(first, context, upload(), page), Status::invalid_state);
    CHECK_STATUS(table.initialize(plane), Status::ok);
    CHECK_STATUS(table.initialize(plane), Status::invalid_state);
    CHECK_EQ(host.live_mutexes(), 1u);
    CHECK_STATUS(table.create(first, context, upload(), page), Status::ok);
    CHECK(table.invariants_hold()); }
  CHECK_EQ(host.live_mutexes(), 0u); }

/* A blob over lent pages: write-back memory only, laid a piece at a time,
 * and only a live blob can be laid. */
TEST(host_blob_table_adopt, lays_a_live_write_back_blob_over_the_pages_named)
{ Fixture fixture;
  fixture.host().add_guest_ram(0x100000u, 0x10000u);
  const HostMemoryRequest readback(HostMemoryRequest::Kind::readback, 0u);
  std::uint8_t entries[32u + 16u] = {};
  const std::uint64_t address = 0x102000u;
  const std::uint32_t length = 0x1000u;
  std::memcpy(entries + 32u, &address, sizeof address);
  std::memcpy(entries + 40u, &length, sizeof length);
  av::MockRequest     mock(entries, sizeof entries);
  av::ControlRequest  request(fixture.host(), &mock);
  CHECK_STATUS(fixture.table().adopt(first, request, 32u, 1u), Status::not_found);
  CHECK_STATUS(fixture.table().create_over_guest(first, context, readback, page), Status::ok);
  CHECK_STATUS(fixture.table().adopt(first, request, 32u, 1u), Status::ok);
  CHECK_EQ(fixture.host().overlaid_bytes(), page);
  char            bytes[512] = {};
  av::StateWriter writer(bytes, sizeof bytes);
  fixture.table().report(writer);
  CHECK(std::strstr(writer.dump(), "lent=true lender=true") != nullptr);
  CHECK_STATUS(fixture.table().release(first), Status::ok);
  CHECK_EQ(fixture.host().overlaid_bytes(), 0u);
  /* Memory the library maps write-combined is given back, as the first piece
   * is laid: a blob is made before its pages are known. */
  fixture.plane().answer_caching(AV_MEMORY_WRITE_COMBINED);
  CHECK_STATUS(fixture.table().create_over_guest(first, context, readback, page), Status::ok);
  CHECK_STATUS(fixture.table().adopt(first, request, 32u, 1u), Status::not_supported);
  CHECK_EQ(fixture.plane().freed_count(), 2u);
  CHECK_STATUS(fixture.table().release(first), Status::ok);
  CHECK_STATUS(fixture.table().create_over_guest(first - 1u, context, readback, page), Status::not_found); }

/* Two adapters' devices lend the same pages: the second finds them lent whole
 * and holds the lending, its blob naming the lender's memory in its executor
 * instead of memory of its own; the pages go back once both blobs are gone. */
TEST(host_blob_table_adopt, a_second_device_lending_the_same_pages_joins_the_first_ones_lending)
{ MockHost host;
  host.add_guest_ram(0x100000u, 0x10000u);
  host.set_hostmem_size(16u * page);
  av::GuestRamLendings lendings(host);
  RecordingContextPlane first_plane;
  RecordingContextPlane second_plane;
  HostBlobTable         first_table(host);
  HostBlobTable         second_table(host);
  first_table.share_lendings(lendings);
  second_table.share_lendings(lendings);
  CHECK_STATUS(first_table.initialize(first_plane), Status::ok);
  CHECK_STATUS(second_table.initialize(second_plane), Status::ok);
  const HostMemoryRequest readback(HostMemoryRequest::Kind::readback, 0u);
  std::uint8_t entries[32u + 16u] = {};
  const std::uint64_t address = 0x102000u;
  const std::uint32_t length = 0x1000u;
  std::memcpy(entries + 32u, &address, sizeof address);
  std::memcpy(entries + 40u, &length, sizeof length);
  av::MockRequest    mock(entries, sizeof entries);
  av::ControlRequest request(host, &mock);
  CHECK_STATUS(first_table.create_over_guest(first, context, readback, page), Status::ok);
  CHECK_STATUS(first_table.adopt(first, request, 32u, 1u), Status::ok);
  CHECK_EQ(first_plane.memory_allocations(), 1u);
  AvHostMemory lender_memory = {-1, AV_MEMORY_WRITE_BACK, page};
  second_plane.alias_as(lender_memory);
  CHECK_STATUS(second_table.create_over_guest(first, context + 1u, readback, page), Status::ok);
  CHECK_STATUS(second_table.adopt(first, request, 32u, 1u), Status::ok);
  /* No memory of its own: an alias of the first device's, and one overlay. */
  CHECK_EQ(second_plane.memory_allocations(), 0u);
  CHECK_EQ(second_plane.aliases(), 1u);
  CHECK(second_plane.last_alias().lender == &first_plane && second_plane.last_alias().lender_key == first);
  CHECK_EQ(host.overlaid_bytes(), page);
  CHECK_EQ(lendings.live(), 1u);
  /* The lender releases first: the pages stay under the lending. */
  CHECK_STATUS(first_table.release(first), Status::ok);
  CHECK_EQ(host.overlaid_bytes(), page);
  CHECK_STATUS(second_table.release(first), Status::ok);
  CHECK_EQ(host.overlaid_bytes(), 0u);
  CHECK_EQ(lendings.live(), 0u);
  /* The memory of a joiner is judged: an executor that refuses the alias
   * leaves the blob waiting, and the pages with their lender. */
  CHECK_STATUS(first_table.create_over_guest(first, context, readback, page), Status::ok);
  CHECK_STATUS(first_table.adopt(first, request, 32u, 1u), Status::ok);
  RecordingContextPlane refusing;
  HostBlobTable         third_table(host);
  third_table.share_lendings(lendings);
  CHECK_STATUS(third_table.initialize(refusing), Status::ok);
  CHECK_STATUS(third_table.create_over_guest(first, context + 2u, readback, page), Status::ok);
  CHECK_STATUS(third_table.adopt(first, request, 32u, 1u), Status::no_memory);
  CHECK_EQ(lendings.live(), 1u);
  CHECK_STATUS(first_table.release(first), Status::ok);
  CHECK_EQ(lendings.live(), 0u); }

TEST(host_blob_table, reports_every_blob_and_where_it_is)
{ Fixture fixture;
  CHECK_STATUS(fixture.table().create(first + 1u, context, upload(), 2u * page), Status::ok);
  fixture.plane().answer_caching(AV_MEMORY_WRITE_COMBINED);
  CHECK_STATUS(fixture.table().create(first + 2u, context + 1u, upload(), page), Status::ok);
  std::uint32_t caching = 0u;
  CHECK_STATUS(fixture.table().map(first + 1u, 3u * page, caching), Status::ok);
  char bytes[1024];
  av::StateWriter writer(bytes, sizeof bytes);
  fixture.table().report(writer);
  CHECK(std::strstr(writer.dump(), "host_blobs/0 live=2") != nullptr);
  CHECK(std::strstr(writer.dump(), "host_blob/16777217 context=3 size=8192 caching=1 mapped=true offset=12288") !=
        nullptr);
  CHECK(std::strstr(writer.dump(), "host_blob/16777218 context=4 size=4096 caching=2 mapped=false offset=0") != nullptr);
  CHECK(std::strstr(writer.dump(), "host_blob/16777216") == nullptr); }

TEST(host_blob_table, creates_places_and_releases_on_the_main_loop_only)
{ Fixture fixture;
  struct Call {
      HostBlobTable *table;
      Status         created;
      Status         mapped;
      Status         unmapped;
      Status         released;
  } call = {&fixture.table(), Status::ok, Status::ok, Status::ok, Status::ok};
  void *thread = nullptr;
  CHECK_STATUS(fixture.host().start_thread(
                   [](void *context_pointer) {
                       auto *const   c = static_cast<Call *>(context_pointer);
                       std::uint32_t caching = 0u;
                       c->created = c->table->create(first - 1u, context, upload(), page);
                       c->mapped = c->table->map(first - 1u, 0u, caching);
                       c->unmapped = c->table->unmap(first - 1u);
                       c->released = c->table->release(first - 1u);
                       c->table->release_all();
                       static_cast<void>(c->table->owns(context, first));   /* any thread */
                   },
                   &call, thread),
               Status::ok);
  fixture.host().join_thread(thread);
  for (int count = 0; count < 5; count++) CHECK(fixture.host().claim(HostViolation::main_loop_required));
  CHECK(!fixture.host().claim(HostViolation::main_loop_required));
  CHECK_STATUS(call.created, Status::not_found);
  CHECK_STATUS(call.mapped, Status::not_found);
  CHECK_STATUS(call.unmapped, Status::not_found);
  CHECK_STATUS(call.released, Status::not_found); }

/* A worker asks who owns a blob while the main loop makes and lets go of it:
 * the lock holds the two apart, so the answer is always one of the two
 * states and never a torn one. */
TEST(host_blob_table_owns, a_worker_asks_while_the_main_loop_creates_and_releases)
{ Fixture fixture;
  struct Call {
      HostBlobTable *table;
      std::uint32_t  owned;
      std::uint32_t  strangers;
  } call = {&fixture.table(), 0u, 0u};
  void *thread = nullptr;
  CHECK_STATUS(fixture.host().start_thread(
                   [](void *context_pointer) {
                       auto *const c = static_cast<Call *>(context_pointer);
                       for (int round = 0; round < 4000; round++) {
                           c->owned += c->table->owns(context, first) ? 1u : 0u;
                           c->strangers += c->table->owns(context + 1u, first) ? 1u : 0u;
                       }
                   },
                   &call, thread),
               Status::ok);
  for (int round = 0; round < 500; round++) {
      CHECK_STATUS(fixture.table().create(first, context, upload(), page), Status::ok);
      CHECK_STATUS(fixture.table().release(first), Status::ok);
  }
  fixture.host().join_thread(thread);
  CHECK_EQ(call.strangers, 0u);
  CHECK_EQ(fixture.host().unclaimed_violations(), 0u); }

TEST(host_blob_table_invariants, overlapping_placements_are_detected)
{ Fixture fixture;
  CHECK_STATUS(fixture.table().create(first, context, upload(), 2u * page), Status::ok);
  std::uint32_t caching = 0u;
  CHECK_STATUS(fixture.table().map(first, 2u * page, caching), Status::ok);
  CHECK(fixture.table().invariants_hold());
  av::HostBlobTableProbe(fixture.table()).place(first + 1u, page, 3u * page);
  CHECK(fixture.host().claim(HostViolation::overlapping_placement));
  CHECK(!fixture.table().invariants_hold()); }

TEST(host_blob_table_invariants, placements_out_of_order_or_apart_from_their_blobs_are_detected)
{ Fixture fixture;
  CHECK_STATUS(fixture.table().create(first, context, upload(), page), Status::ok);
  CHECK_STATUS(fixture.table().create(first + 1u, context, upload(), page), Status::ok);
  std::uint32_t caching = 0u;
  CHECK_STATUS(fixture.table().map(first, 2u * page, caching), Status::ok);
  CHECK_STATUS(fixture.table().map(first + 1u, 4u * page, caching), Status::ok);
  CHECK(fixture.table().invariants_hold());
  av::HostBlobTableProbe(fixture.table()).misplace(1u, page);
  CHECK(!fixture.table().invariants_hold());   /* before the one it follows */
  av::HostBlobTableProbe(fixture.table()).misplace(1u, 5u * page);
  CHECK(!fixture.table().invariants_hold());   /* noted where its blob is not */
  av::HostBlobTableProbe(fixture.table()).misplace(1u, 4u * page);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().unmap(first + 1u), Status::ok);
  CHECK(fixture.table().invariants_hold()); }

TEST(host_blob_table_invariants, a_blob_without_the_lock_is_detected)
{ MockHost host;
  host.set_hostmem_size(16u * page);
  RecordingContextPlane plane;
  HostBlobTable table(host);
  CHECK(table.invariants_hold());
  av::HostBlobTableProbe(table).place(first, page, 0u);
  CHECK(!table.invariants_hold());
  CHECK_STATUS(table.initialize(plane), Status::ok);
  CHECK(table.invariants_hold()); }
