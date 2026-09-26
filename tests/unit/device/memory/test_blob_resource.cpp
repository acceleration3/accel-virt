#include "core/support/status.hpp"
#include "device/control/control_request.hpp"
#include "device/memory/blob_resource.hpp"
#include "device/memory/blob_window.hpp"
#include "device/memory/blob_dma_buf.hpp"
#include "device/memory/held_blob.hpp"
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

/* The pages at 0x100000 and 0x102000 entries() names: two runs in one range. */
TEST(blob_resource_hold, maps_its_guest_pages_for_the_first_holder_and_unmaps_after_the_last)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  const GpuRequestBuilder bytes = entries(2u);
  MockRequest mock(bytes.bytes(), bytes.size());
  ControlRequest request(host, &mock);
  BlobResource blob(host, 7u);
  CHECK_STATUS(blob.back_with_guest(request, entries_at, 2u, 0x2000u), Status::ok);
  host.guest_ram(ram + 0x2010u)[0] = 0x9Du;
  CHECK(!blob.held());
  av::HeldBlob first;
  CHECK_STATUS(blob.hold(first), Status::ok);
  CHECK(blob.held());
  CHECK(first.base() != nullptr);
  CHECK_EQ(first.length(), 0x2000u);
  CHECK_EQ(host.live_aliases(), 2u);
  CHECK_EQ(static_cast<std::uint8_t *>(first.base())[0x1010u], 0x9Du);
  /* The extents the hold keeps valid are what a dma-buf is made over: one
   * item a run, the guest's own pages. */
  { av::BlobDmaBuf shared(host);
    CHECK_STATUS(first.share(shared, 0u, 0x2000u), Status::ok);
    CHECK_EQ(shared.items(), 2u);
    CHECK_EQ(shared.length(), 0x2000u);
    CHECK_EQ(host.host_dmabufs(), 0u); }
  av::HeldBlob second;
  CHECK_STATUS(blob.hold(second), Status::ok);
  CHECK(second.base() == first.base());
  CHECK_EQ(host.live_ranges(), 1u);   /* made once, however many hold it */
  blob.release_hold();
  CHECK(blob.held());
  CHECK_EQ(host.live_ranges(), 1u);
  blob.release_hold();
  CHECK(!blob.held());
  CHECK_EQ(host.live_ranges(), 0u);
  blob.release_hold();   /* no holder: nothing */
  CHECK(!blob.held());
  CHECK_EQ(host.unclaimed_violations(), 0u);
  CHECK_STATUS(blob.hold(first), Status::ok);   /* and made again for the next */
  CHECK_EQ(host.live_ranges(), 1u); }

/* A host block is one mapping already (docs/host-window.md): it is held as
 * itself, aliases nothing, and its dma-buf is the host's over the block. */
TEST(blob_resource_hold, a_host_block_is_held_as_its_own_view_and_counted_like_an_alias)
{ MockHost host;
  host.set_hostmem_size(0x10000u);
  BlobResource block(host, 1u);
  CHECK_STATUS(block.back_with_host(0x3000u), Status::ok);
  av::BlobWindow view = block.window();
  std::uint64_t  bytes = 0u;
  void *const    mapping = view.extent(0u, bytes);
  av::HeldBlob held;
  CHECK_STATUS(block.hold(held), Status::ok);
  CHECK(block.held());
  CHECK(held.base() == mapping);
  CHECK_EQ(held.length(), 0x3000u);
  CHECK_EQ(host.live_ranges(), 0u);
  { av::BlobDmaBuf shared(host);
    CHECK_STATUS(held.share(shared, 0x1000u, 0x2000u), Status::ok);
    CHECK_EQ(host.host_dmabufs(), 1u);
    CHECK(host.last_host_dmabuf().memory == block.host_memory());
    CHECK_EQ(host.last_host_dmabuf().offset, 0x1000u);
    CHECK_EQ(host.last_host_dmabuf().size, 0x2000u); }
  CHECK_EQ(host.live_dmabufs(), 0u);
  av::HeldBlob again;
  CHECK_STATUS(block.hold(again), Status::ok);
  block.release_hold();
  CHECK(block.held());
  block.release_hold();
  CHECK(!block.held());
  CHECK_EQ(host.unclaimed_violations(), 0u); }

TEST(blob_resource_hold, memory_the_host_cannot_give_holds_nothing)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  host.set_hostmem_size(0x10000u);
  host.empty_host_memory_at(1u);
  BlobResource empty(host, 1u);
  CHECK_STATUS(empty.back_with_host(0x1000u), Status::ok);
  av::HeldBlob held;
  CHECK_STATUS(empty.hold(held), Status::no_memory);   /* a handle with no mapping behind it */
  CHECK(held.base() == nullptr);
  CHECK(!empty.held());
  const GpuRequestBuilder bytes = entries(1u);
  MockRequest mock(bytes.bytes(), bytes.size());
  ControlRequest request(host, &mock);
  BlobResource guest(host, 2u);
  CHECK_STATUS(guest.back_with_guest(request, entries_at, 1u, 0x1000u), Status::ok);
  host.fail_reserve_at(1u);
  CHECK_STATUS(guest.hold(held), Status::no_memory);
  CHECK(!guest.held());
  BlobResource unbacked(host, 3u);
  CHECK_STATUS(unbacked.hold(held), Status::malformed);
  CHECK(!unbacked.held());
  CHECK_EQ(host.live_ranges(), 0u); }
