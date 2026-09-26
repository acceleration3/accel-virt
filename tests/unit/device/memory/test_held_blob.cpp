/* A held blob: the one host range an importer is handed, and the dma-buf over
 * part of it made from what the blob is - its guest pages, or the host block
 * it is (docs/host-window.md). Over MockHost's file-backed guest RAM, whose
 * dma-bufs are the kernel's own. */
#include "core/support/status.hpp"
#include "device/memory/blob_dma_buf.hpp"
#include "device/memory/blob_window.hpp"
#include "device/memory/held_blob.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"

#include <cstdint>

using av::BlobDmaBuf;
using av::BlobWindow;
using av::HeldBlob;
using av::MockHost;
using av::Status;

namespace {

constexpr std::uint64_t page = 4096u;
constexpr std::uint64_t ram = 0x40000000u;

}  // namespace

TEST(held_blob, an_empty_one_holds_nothing)
{ const HeldBlob held;
  CHECK(held.base() == nullptr);
  CHECK_EQ(held.length(), 0u); }

TEST(held_blob_share, a_guest_blob_is_shared_over_its_pages)
{ MockHost host;
  host.add_guest_ram(ram, 4u * page);
  void *const         runs[2] = {host.guest_ram(ram + 2u * page), host.guest_ram(ram)};
  const std::uint64_t lengths[2] = {page, page};
  const BlobWindow    pages(runs, lengths, 2u, 2u * page);
  std::uint8_t        alias[1] = {};
  const HeldBlob      held(alias, 2u * page, pages, nullptr);
  CHECK(held.base() == alias);
  CHECK_EQ(held.length(), 2u * page);
  BlobDmaBuf shared(host);
  const Status made = held.share(shared, 0u, 2u * page);
  /* A kernel with no udmabuf answers not_supported; either way the guest's
   * pages were asked, never the host block's. */
  CHECK(made == Status::ok || made == Status::not_supported);
  if (made == Status::ok) CHECK_EQ(shared.items(), 2u);
  CHECK_EQ(host.host_dmabufs(), 0u); }

TEST(held_blob_share, a_host_blob_is_shared_over_its_block_by_the_host)
{ MockHost host;
  void       *view = nullptr;
  void *const memory = host.create_host_memory(3u * page, view);
  const BlobWindow window(&view, nullptr, 0u, 0u);
  const HeldBlob   held(view, 3u * page, window, memory);
  { BlobDmaBuf shared(host);
    CHECK_STATUS(held.share(shared, page, 2u * page), Status::ok);
    CHECK_EQ(shared.items(), 1u);
    CHECK_EQ(shared.length(), 2u * page);
    CHECK(host.last_host_dmabuf().memory == memory);
    CHECK_EQ(host.last_host_dmabuf().offset, page);
    CHECK_EQ(host.last_host_dmabuf().size, 2u * page);
    BlobDmaBuf past(host);
    CHECK_STATUS(held.share(past, page, 3u * page), Status::too_large);   /* judged against the held length */
    CHECK_EQ(host.host_dmabufs(), 1u); }
  CHECK_EQ(host.live_dmabufs(), 0u);
  host.destroy_host_memory(memory); }
