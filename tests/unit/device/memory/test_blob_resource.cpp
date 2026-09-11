#include "core/support/status.hpp"
#include "device/control/control_request.hpp"
#include "device/memory/blob_resource.hpp"
#include "device/memory/blob_window.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/gpu_request_builder.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"

#include <cstdint>

using av::BlobResource;
using av::ControlRequest;
using av::GpuRequestBuilder;
using av::HostViolation;
using av::MockHost;
using av::MockRequest;
using av::Status;

namespace {

constexpr std::uint64_t ram = 0x100000u;
constexpr std::size_t   entries_at = 24u;

/* A request whose memory entries, right after the header, name `pages` pages of RAM. */
GpuRequestBuilder entries(std::uint32_t pages)
{
    GpuRequestBuilder request(0u);
    for (std::uint32_t page = 0u; page < pages; page++) request.u64(ram + page * 0x2000u).u32(0x1000u).u32(0u);
    return request;
}

}  // namespace

TEST(blob_resource_back_with_guest, holds_the_pages_that_cover_its_size)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  const GpuRequestBuilder bytes = entries(2u);
  MockRequest mock(bytes.bytes(), bytes.size());
  ControlRequest request(host, &mock);
  { BlobResource blob(host, 7u);
    CHECK_EQ(blob.id(), 7u);
    CHECK_STATUS(blob.back_with_guest(request, entries_at, 2u, 0x1800u), Status::ok);
    CHECK_EQ(blob.size(), 0x2000u);
    CHECK(!blob.mapped());
    CHECK(!blob.overlaps(0u, 0x10000u));
    CHECK_STATUS(blob.map(0u), Status::invalid_parameter);   /* guest pages are not placed */
    CHECK_STATUS(blob.unmap(), Status::invalid_state);
    CHECK_EQ(host.live_guest_mappings(), 2u); }
  CHECK_EQ(host.live_guest_mappings(), 0u); }

TEST(blob_resource_back_with_guest, pages_short_of_its_size_or_a_mapping_failure_leave_nothing_held)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  const GpuRequestBuilder bytes = entries(1u);
  MockRequest mock(bytes.bytes(), bytes.size());
  ControlRequest request(host, &mock);
  BlobResource short_blob(host, 1u);
  CHECK_STATUS(short_blob.back_with_guest(request, entries_at, 1u, 0x1001u), Status::invalid_parameter);
  CHECK_EQ(host.live_guest_mappings(), 0u);
  CHECK_EQ(short_blob.size(), 0u);
  BlobResource unmappable(host, 2u);
  CHECK(unmappable.back_with_guest(request, entries_at, 2u, 0x1000u) != Status::ok);   /* entries missing */
  CHECK_EQ(host.live_guest_mappings(), 0u); }

TEST(blob_resource_window, is_its_guest_pages_or_its_host_block)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  GpuRequestBuilder request_bytes = entries(1u);
  MockRequest mock(request_bytes.bytes(), request_bytes.size());
  ControlRequest request(host, &mock);
  BlobResource guest(host, 1u);
  CHECK_EQ(guest.window().length(), 0u);
  CHECK_STATUS(guest.back_with_guest(request, entries_at, 1u, 0x1000u), Status::ok);
  CHECK_EQ(guest.window().length(), 0x1000u);
  const std::uint8_t byte = 0x77u;
  CHECK_STATUS(guest.window().write(5u, &byte, 1u), Status::ok);
  CHECK_EQ(host.guest_ram(ram + 5u)[0], 0x77u);
  BlobResource block(host, 2u);
  CHECK_STATUS(block.back_with_host(0x3000u), Status::ok);
  CHECK_EQ(block.window().length(), 0x3000u);
  CHECK(block.window().invariants_hold()); }

TEST(blob_resource_back_with_host, maps_once_and_unmaps)
{ MockHost host;
  host.set_hostmem_size(0x10000u);
  BlobResource blob(host, 3u);
  CHECK_STATUS(blob.back_with_host(0x2000u), Status::ok);
  CHECK_EQ(blob.size(), 0x2000u);
  CHECK(!blob.overlaps(0x4000u, 0x1000u));
  CHECK_STATUS(blob.map(0x4000u), Status::ok);
  CHECK(blob.mapped());
  CHECK_EQ(blob.offset(), 0x4000u);
  CHECK(host.placed_at(0x4000u) != nullptr);
  CHECK_STATUS(blob.map(0x8000u), Status::invalid_parameter);
  CHECK_STATUS(blob.unmap(), Status::ok);
  CHECK(!blob.mapped());
  CHECK_EQ(host.live_placements(), 0u);
  CHECK_STATUS(blob.unmap(), Status::invalid_state); }

TEST(blob_resource_back_with_host, a_host_that_cannot_allocate_or_place_leaves_it_unmapped)
{ MockHost host;
  host.set_hostmem_size(0x10000u);
  BlobResource empty(host, 1u);
  host.fail_host_memory_at(1u);
  CHECK_STATUS(empty.back_with_host(0x1000u), Status::no_memory);
  CHECK_EQ(empty.size(), 0u);
  BlobResource blob(host, 2u);
  CHECK_STATUS(blob.back_with_host(0x1000u), Status::ok);
  host.fail_placement_at(1u);
  CHECK(blob.map(0u) != Status::ok);
  CHECK(!blob.mapped());
  CHECK_EQ(host.live_placements(), 0u);
  CHECK_STATUS(blob.map(0u), Status::ok); }

TEST(blob_resource_overlaps, only_a_mapped_blob_covers_bytes_and_only_its_own)
{ MockHost host;
  host.set_hostmem_size(0x10000u);
  BlobResource blob(host, 1u);
  CHECK_STATUS(blob.back_with_host(0x2000u), Status::ok);
  CHECK_STATUS(blob.map(0x4000u), Status::ok);
  CHECK(blob.overlaps(0x4000u, 0x2000u));
  CHECK(blob.overlaps(0x3000u, 0x2000u));
  CHECK(blob.overlaps(0x5000u, 0x4000u));
  CHECK(!blob.overlaps(0x2000u, 0x2000u));   /* ends where it begins */
  CHECK(!blob.overlaps(0x6000u, 0x1000u)); } /* begins where it ends */

TEST(blob_resource, a_mapped_blob_is_removed_before_its_memory_is_freed)
{ MockHost host;
  host.set_hostmem_size(0x10000u);
  { BlobResource blob(host, 1u);
    CHECK_STATUS(blob.back_with_host(0x1000u), Status::ok);
    CHECK_STATUS(blob.map(0x1000u), Status::ok); }
  CHECK_EQ(host.live_placements(), 0u);
  CHECK_EQ(host.live_host_memory(), 0u);
  CHECK_EQ(host.unclaimed_violations(), 0u); }

TEST(blob_resource, is_main_loop_only)
{ MockHost host;
  host.set_hostmem_size(0x10000u);
  struct Call {
      MockHost *host;
      Status    status;
  } call = {&host, Status::ok};
  void *thread = nullptr;
  CHECK_STATUS(host.start_thread(
                   [](void *context) {
                       auto *const c = static_cast<Call *>(context);
                       BlobResource blob(*c->host, 1u);
                       c->status = blob.unmap();
                   },
                   &call, thread),
               Status::ok);
  host.join_thread(thread);
  for (int violation = 0; violation < 3; violation++)   /* unmap, then both releases */
      CHECK(host.claim(HostViolation::main_loop_required));
  CHECK_STATUS(call.status, Status::invalid_state); }

TEST(blob_resource_pin, counts_pins_until_the_last_is_let_go)
{ MockHost host;
  BlobResource blob(host, 1u);
  CHECK(!blob.pinned());
  blob.pin();
  blob.pin();
  CHECK(blob.pinned());
  blob.unpin();
  CHECK(blob.pinned());
  blob.unpin();
  CHECK(!blob.pinned()); }
