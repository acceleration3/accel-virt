/* One host blob: host GPU memory the executor library allocated, as a dma-buf
 * the blob owns, placed in the hostmem region where the guest reaches it. The
 * descriptor is closed exactly once, with the blob, and the blob is never left
 * placed behind itself. */
#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "device/memory/host_blob.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"

#include <cstdint>
#include <sys/mman.h>
#include <unistd.h>

using av::HostBlob;
using av::HostViolation;
using av::MockHost;
using av::Status;

namespace {

constexpr std::uint64_t page = 0x1000u;

/* What the library answers: a descriptor of `size` bytes the blob owns. */
AvHostMemory memory(std::uint64_t size, std::uint32_t caching = AV_MEMORY_WRITE_BACK)
{
    const int fd = memfd_create("host blob", MFD_CLOEXEC);
    if (fd >= 0 && ftruncate(fd, static_cast<off_t>(size)) != 0) return AvHostMemory{-1, caching, size};
    return AvHostMemory{fd, caching, size};
}

}  // namespace

TEST(host_blob, says_what_it_was_made_with_and_closes_its_descriptor_once)
{ MockHost host;
  const AvHostMemory made = memory(2u * page, AV_MEMORY_WRITE_COMBINED);
  { HostBlob blob(host, 7u, made);
    CHECK_EQ(blob.context(), 7u);
    CHECK_EQ(blob.size(), 2u * page);
    CHECK_EQ(blob.caching(), static_cast<std::uint32_t>(AV_MEMORY_WRITE_COMBINED));
    CHECK(!blob.mapped());
    CHECK_EQ(blob.offset(), 0u);
    CHECK(host.descriptor_open(made.fd)); }
  CHECK(!host.descriptor_open(made.fd));
  CHECK_EQ(host.unclaimed_violations(), 0u); }

/* The guest reads, at the offset, the very memory the library allocated. */
TEST(host_blob_map, places_the_memory_where_the_guest_reads_it)
{ MockHost host;
  host.set_hostmem_size(16u * page);
  const AvHostMemory made = memory(2u * page);
  auto *const library = static_cast<std::uint8_t *>(
      mmap(nullptr, 2u * page, PROT_READ | PROT_WRITE, MAP_SHARED, made.fd, 0));
  library[page + 3u] = 0x6Cu;
  { HostBlob blob(host, 1u, made);
    CHECK_STATUS(blob.map(4u * page), Status::ok);
    CHECK(blob.mapped());
    CHECK_EQ(blob.offset(), 4u * page);
    CHECK_EQ(host.last_placement().fd, made.fd);
    CHECK_EQ(host.last_placement().size, 2u * page);
    CHECK_EQ(host.last_placement().offset, 4u * page);
    auto *const guest = static_cast<std::uint8_t *>(host.placed_at(4u * page));
    CHECK(guest != nullptr);
    CHECK_EQ(guest[page + 3u], 0x6Cu);
    CHECK_STATUS(blob.map(8u * page), Status::invalid_state);   /* placed once */
    CHECK_EQ(host.live_placements(), 1u);
    CHECK_STATUS(blob.unmap(), Status::ok);
    CHECK(!blob.mapped());
    CHECK_EQ(blob.offset(), 0u);
    CHECK_EQ(host.live_placements(), 0u);
    CHECK_STATUS(blob.unmap(), Status::invalid_state);
    CHECK_STATUS(blob.map(0u), Status::ok);   /* placed again elsewhere */
    CHECK_EQ(blob.offset(), 0u);
    CHECK(blob.mapped()); }
  CHECK_EQ(host.live_placements(), 0u);   /* the blob took itself out first */
  CHECK(!host.descriptor_open(made.fd));
  munmap(library, 2u * page); }

TEST(host_blob_map, a_host_that_cannot_place_it_leaves_it_unplaced)
{ MockHost host;
  host.set_hostmem_size(16u * page);
  HostBlob blob(host, 1u, memory(page));
  host.fail_placement_at(1u);
  CHECK_STATUS(blob.map(page), Status::no_memory);
  CHECK(!blob.mapped());
  CHECK_EQ(blob.offset(), 0u);
  CHECK(!blob.overlaps(0u, 16u * page));
  CHECK_EQ(host.live_placements(), 0u);
  CHECK_STATUS(blob.map(page), Status::ok); }

/* A descriptor shorter than the blob says it is cannot be placed whole. */
TEST(host_blob_map, memory_smaller_than_the_blob_is_refused)
{ MockHost host;
  AvHostMemory made = memory(page);
  made.size = 2u * page;
  HostBlob blob(host, 1u, made);
  CHECK_STATUS(blob.map(0u), Status::no_memory);
  CHECK(!blob.mapped()); }

TEST(host_blob_overlaps, only_a_placed_blob_covers_bytes_and_only_its_own)
{ MockHost host;
  HostBlob blob(host, 1u, memory(2u * page));
  CHECK(!blob.overlaps(4u * page, page));   /* not placed: covers nothing */
  CHECK_STATUS(blob.map(4u * page), Status::ok);
  CHECK(blob.overlaps(4u * page, 1u));
  CHECK(blob.overlaps(6u * page - 1u, 1u));
  CHECK(!blob.overlaps(6u * page, page));   /* ends where the blob ends */
  CHECK(!blob.overlaps(3u * page, page));   /* ends where the blob starts */
  CHECK(blob.overlaps(3u * page, page + 1u));
  CHECK(blob.overlaps(0u, 16u * page));     /* around it */
  CHECK(blob.overlaps(5u * page, 1u));      /* inside it */
  CHECK_STATUS(blob.unmap(), Status::ok); }

TEST(host_blob, is_placed_and_taken_down_on_the_main_loop_only)
{ MockHost host;
  struct Call {
      HostBlob *blob;
      Status    mapped;
      Status    unmapped;
  };
  HostBlob blob(host, 1u, memory(page));
  Call     call = {&blob, Status::ok, Status::ok};
  void    *thread = nullptr;
  CHECK_STATUS(host.start_thread(
                   [](void *context) {
                       auto *const c = static_cast<Call *>(context);
                       c->unmapped = c->blob->unmap();
                       c->mapped = c->blob->map(0u);
                   },
                   &call, thread),
               Status::ok);
  host.join_thread(thread);
  CHECK(host.claim(HostViolation::main_loop_required));
  CHECK(host.claim(HostViolation::main_loop_required));
  while (host.claim(HostViolation::main_loop_required)) {}   /* what the mock's own placement asserts */
  CHECK_STATUS(call.unmapped, Status::invalid_state);
  CHECK_STATUS(call.mapped, Status::ok);
  CHECK_STATUS(blob.unmap(), Status::ok); }
