/* One context's imports of its chunks into its executor (feature 045): each
 * chunk aliased once for every import made through it, the alias and the
 * blob's hold gone with the last of them, and every refusal leaving nothing new
 * behind. Everything runs
 * over ImportRig - real file-backed guest pages, the device's blob table and a
 * version 3 mock executor on the context's one worker. */
#include "core/support/status.hpp"
#include "device/abi/rvk_executor.h"
#include "device/execution/blob_imports.hpp"
#include "device/recording/wire_recorder.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "harness/executor/import_rig.hpp"
#include "harness/recording/recorded_file.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "proto/context/context_resources.hpp"

#include <cstdint>
#include <cstring>
#include <sys/mman.h>

using av::BlobImports;
using av::ContextResources;
using av::HostViolation;
using av::ImportRig;
using av::MockExecutorOps;
using av::Status;

namespace av {

struct BlobImportsProbe {
    static std::uint64_t swap_handle(BlobImports &imports, std::uint64_t handle)
    {
        const std::uint64_t kept = imports.device_memory_[1];
        imports.device_memory_[1] = handle;
        return kept;
    }
    static std::uint64_t first_handle(BlobImports &imports) { return imports.device_memory_[0]; }
    static std::uint32_t swap_resource(BlobImports &imports, std::uint32_t resource)
    {
        const std::uint32_t kept = imports.resources_[0];
        imports.resources_[0] = resource;
        return kept;
    }
    static std::uint64_t swap_blob_id(BlobImports &imports, std::uint64_t blob_id)
    {
        const std::uint64_t kept = imports.blob_ids_[0];
        imports.blob_ids_[0] = blob_id;
        return kept;
    }
};

}  // namespace av

namespace {

constexpr std::uint64_t page = ImportRig::page;

/* The bytes behind a handle the executor was given, whichever kind it is: a
 * dma-buf mapped for as long as this lives, a host pointer as it stands. What
 * the host GPU would see, which is what every byte assertion here is about. */
class Handed {
public:
    explicit Handed(const RvkBlobHandle &handle)
        : bytes_(nullptr), length_(handle.size), mapped_(handle.kind == RVK_BLOB_DMA_BUF)
    {
        if (!mapped_) {
            bytes_ = static_cast<std::uint8_t *>(handle.pointer);
            return;
        }
        void *const at = mmap(nullptr, length_, PROT_READ | PROT_WRITE, MAP_SHARED, handle.fd, 0);
        bytes_ = at == MAP_FAILED ? nullptr : static_cast<std::uint8_t *>(at);
    }
    ~Handed() { if (mapped_ && bytes_ != nullptr) munmap(bytes_, length_); }
    Handed(const Handed &) = delete;
    Handed &operator=(const Handed &) = delete;

    std::uint8_t *bytes() const { return bytes_; }

private:
    std::uint8_t *bytes_;
    std::uint64_t length_;
    bool          mapped_;
};

/* An import on the worker: its status, with the handle in `device_memory`. */
Status import(ImportRig &rig, std::uint32_t resource, std::uint64_t offset, std::uint64_t size,
              std::uint32_t memory_type, std::uint64_t &device_memory)
{
    Status status = Status::invalid_state;
    rig.on_worker([&] { status = rig.imports().import(resource, offset, size, memory_type, device_memory); });
    CHECK(rig.imports().invariants_hold());
    return status;
}

void release(ImportRig &rig, std::uint64_t device_memory)
{
    rig.on_worker([&] { rig.imports().release(device_memory); });
    CHECK(rig.imports().invariants_hold());
}

}  // namespace

TEST(blob_imports_import, a_chunk_is_aliased_and_the_executor_is_handed_the_range_asked_for)
{ ImportRig rig;
  const std::uint32_t pages[] = {9u, 3u, 4u, 20u};
  CHECK_STATUS(rig.create_guest_blob(rig.chunk(0u), pages, 4u), Status::ok);
  CHECK(rig.imports().invariants_hold());
  std::uint64_t device_memory = 0u;
  CHECK_STATUS(import(rig, rig.chunk(0u), page, 2u * page, 5u, device_memory), Status::ok);
  CHECK_EQ(device_memory, 1u | MockExecutorOps::handle_bit);   /* the device's first blob id */
  CHECK_EQ(rig.imports().imports(), 1u);
  CHECK_EQ(rig.host().live_ranges(), 1u);
  CHECK_EQ(rig.host().live_aliases(), 3u);   /* 9 | 3-4 | 20 */
  CHECK_STATUS(rig.blobs().release(rig.chunk(0u)), Status::invalid_state);   /* the executor has its pages */
  RvkBlobHandle handed = {};
  CHECK(rig.ops().import_of(1u, handed));
  /* A dma-buf, because the host makes one: the guest's pages reach a GPU that
   * refuses a file-backed userptr only that way. */
  CHECK_EQ(handed.kind, std::uint32_t{RVK_BLOB_DMA_BUF});
  CHECK_EQ(handed.reserved, 5u);
  CHECK(handed.fd >= 0);
  CHECK(handed.pointer == nullptr);
  CHECK_EQ(handed.size, 2u * page);
  Handed shared(handed);
  auto *const pointer = shared.bytes();
  CHECK(pointer != nullptr);
  CHECK(std::memcmp(pointer, rig.guest_page(3u), page) == 0);
  CHECK(std::memcmp(pointer + page, rig.guest_page(4u), page) == 0);
  pointer[page + 7u] = 0xD4u;   /* what the host GPU writes is what the guest reads */
  CHECK_EQ(rig.guest_page(4u)[7], 0xD4u);
  rig.guest_page(3u)[0] = 0x3Cu;
  CHECK_EQ(pointer[0], 0x3Cu);
  CHECK_EQ(rig.ops().misuses(), 0u); }

/* The same import on a host that makes no dma-buf - no udmabuf in its kernel -
 * which is the path every host took before there was another one. */
TEST(blob_imports_import, a_host_that_makes_no_dma_buf_hands_a_pointer_into_the_alias)
{ ImportRig rig;
  rig.host().refuse_dmabufs();
  const std::uint32_t pages[] = {9u, 3u, 4u, 20u};
  CHECK_STATUS(rig.create_guest_blob(rig.chunk(0u), pages, 4u), Status::ok);
  std::uint64_t device_memory = 0u;
  CHECK_STATUS(import(rig, rig.chunk(0u), page, 2u * page, 5u, device_memory), Status::ok);
  CHECK_EQ(rig.host().live_dmabufs(), 0u);
  RvkBlobHandle handed = {};
  CHECK(rig.ops().import_of(1u, handed));
  CHECK_EQ(handed.kind, std::uint32_t{RVK_BLOB_HOST_POINTER});
  CHECK_EQ(handed.reserved, 5u);
  CHECK_EQ(handed.fd, -1);
  CHECK_EQ(handed.size, 2u * page);
  auto *const pointer = static_cast<std::uint8_t *>(handed.pointer);
  CHECK(std::memcmp(pointer, rig.guest_page(3u), page) == 0);
  pointer[page + 7u] = 0xD4u;
  CHECK_EQ(rig.guest_page(4u)[7], 0xD4u);
  /* Both attempts were the one import, so the id it was made under is the
   * first the context hands out and the next import gets the second. */
  std::uint64_t second = 0u;
  CHECK_STATUS(import(rig, rig.chunk(0u), 0u, page, 5u, second), Status::ok);
  CHECK(rig.ops().import_of(2u, handed));
  CHECK_EQ(rig.ops().misuses(), 0u); }

TEST(blob_imports_import, imports_through_one_chunk_share_its_alias_until_the_last_is_released)
{ ImportRig rig;
  const std::uint32_t pages[] = {30u, 31u};
  CHECK_STATUS(rig.create_guest_blob(rig.chunk(63u), pages, 2u), Status::ok);
  std::uint64_t first = 0u;
  std::uint64_t second = 0u;
  CHECK_STATUS(import(rig, rig.chunk(63u), 0u, page, 0u, first), Status::ok);
  CHECK_STATUS(import(rig, rig.chunk(63u), page, page, 1u, second), Status::ok);
  CHECK(first != second);
  CHECK_EQ(rig.host().live_ranges(), 1u);
  CHECK_EQ(rig.host().live_aliases(), 1u);
  RvkBlobHandle one = {};
  RvkBlobHandle two = {};
  CHECK(rig.ops().import_of(1u, one) && rig.ops().import_of(2u, two));
  /* One alias, two dma-bufs: each names its own page of the same blob. */
  CHECK(one.fd != two.fd);
  { Handed lower(one);
    Handed upper(two);
    CHECK(lower.bytes() != nullptr && upper.bytes() != nullptr);
    CHECK(std::memcmp(lower.bytes(), rig.guest_page(30u), page) == 0);
    CHECK(std::memcmp(upper.bytes(), rig.guest_page(31u), page) == 0); }
  release(rig, first);
  CHECK_EQ(rig.imports().imports(), 1u);
  CHECK_EQ(rig.host().live_ranges(), 1u);
  CHECK_STATUS(rig.blobs().release(rig.chunk(63u)), Status::invalid_state);
  CHECK_EQ(rig.ops().live_imports(), 1u);
  release(rig, first);   /* already released: nothing */
  CHECK_EQ(rig.ops().released(), 1u);
  release(rig, second);
  CHECK_EQ(rig.imports().imports(), 0u);
  CHECK_EQ(rig.host().live_ranges(), 0u);
  CHECK_EQ(rig.ops().released(), 2u);
  CHECK_EQ(rig.ops().live_imports(), 0u);
  CHECK_STATUS(rig.blobs().release(rig.chunk(63u)), Status::ok); }

TEST(blob_imports_import, a_resource_that_is_no_guest_chunk_of_this_context_is_not_found_and_maps_nothing)
{ ImportRig rig;
  const std::uint32_t pages[] = {1u};
  const std::uint32_t transport[] = {ImportRig::ring, ImportRig::ring + 1u, ImportRig::ring + 2u};
  for (const std::uint32_t id : transport) CHECK_STATUS(rig.create_guest_blob(id, pages, 1u), Status::ok);
  CHECK_STATUS(rig.create_guest_blob(ImportRig::ring + ContextResources::per_context, pages, 1u), Status::ok);
  CHECK_STATUS(rig.blobs().create_host(rig.chunk(1u), page), Status::ok);
  const std::uint32_t ids[] = {0u, ImportRig::ring - 1u, ImportRig::ring, ImportRig::ring + 1u, ImportRig::ring + 2u,
                               ImportRig::ring + ContextResources::per_context, 0xFFFFFFFFu,
                               rig.chunk(0u),    /* a chunk slot no blob holds */
                               rig.chunk(1u)};   /* a blob the host backs */
  for (const std::uint32_t id : ids) {
      std::uint64_t device_memory = 7u;
      CHECK_STATUS(import(rig, id, 0u, page, 0u, device_memory), Status::not_found);
      CHECK_EQ(device_memory, 0u);
  }
  CHECK_EQ(rig.host().live_ranges(), 0u);
  CHECK_EQ(rig.ops().imported(), 0u); }

TEST(blob_imports_import, a_range_past_the_blob_is_too_large_and_the_alias_made_for_it_goes)
{ ImportRig rig;
  const std::uint32_t pages[] = {40u, 42u};
  CHECK_STATUS(rig.create_guest_blob(rig.chunk(2u), pages, 2u), Status::ok);
  struct Case { std::uint64_t offset; std::uint64_t size; };
  const Case outside[] = {{0u, 3u * page}, {2u * page, page}, {page, 2u * page},
                          {0xFFFFFFFFFFFFF000u, page}, {page, 0xFFFFFFFFFFFFF000u}};
  for (const Case &c : outside) {
      std::uint64_t device_memory = 0u;
      CHECK_STATUS(import(rig, rig.chunk(2u), c.offset, c.size, 0u, device_memory), Status::too_large);
      CHECK_EQ(rig.host().live_ranges(), 0u);
  }
  std::uint64_t device_memory = 0u;
  CHECK_STATUS(import(rig, rig.chunk(2u), page, page, 0u, device_memory), Status::ok);   /* the last page exactly */
  CHECK_STATUS(import(rig, rig.chunk(2u), 0u, 3u * page, 0u, device_memory), Status::too_large);
  CHECK_EQ(rig.host().live_ranges(), 1u);   /* held by the first import, not dropped by the refusal */
  CHECK_EQ(rig.ops().imported(), 1u); }

TEST(blob_imports_import, pages_the_host_cannot_alias_are_no_memory_and_leave_nothing)
{ ImportRig rig;
  const std::uint32_t pages[] = {50u, 52u};
  CHECK_STATUS(rig.create_guest_blob(rig.chunk(3u), pages, 2u), Status::ok);
  std::uint64_t device_memory = 0u;
  rig.host().fail_reserve_at(1u);
  CHECK_STATUS(import(rig, rig.chunk(3u), 0u, page, 0u, device_memory), Status::no_memory);
  rig.host().fail_alias_at(2u);
  CHECK_STATUS(import(rig, rig.chunk(3u), 0u, page, 0u, device_memory), Status::no_memory);
  CHECK_EQ(rig.host().live_ranges(), 0u);
  CHECK_EQ(rig.ops().imported(), 0u);
  CHECK_STATUS(import(rig, rig.chunk(3u), 0u, page, 0u, device_memory), Status::ok); }

/* A library that takes no dma-buf - OpenCL's, whose import is a host pointer -
 * is handed the pointer instead, under the same id: the refusal left it
 * holding nothing. */
TEST(blob_imports_import, a_library_that_refuses_the_dma_buf_is_handed_the_pointer_under_the_same_id)
{ ImportRig rig;
  const std::uint32_t pages[] = {61u};
  CHECK_STATUS(rig.create_guest_blob(rig.chunk(4u), pages, 1u), Status::ok);
  std::uint64_t device_memory = 0u;
  rig.ops().refuse_import_at(1u);
  CHECK_STATUS(import(rig, rig.chunk(4u), 0u, page, 3u, device_memory), Status::ok);
  CHECK_EQ(rig.imports().imports(), 1u);
  CHECK_EQ(rig.host().live_dmabufs(), 0u);   /* the one it refused was closed */
  RvkBlobHandle handed = {};
  CHECK(rig.ops().import_of(1u, handed));
  CHECK_EQ(handed.kind, std::uint32_t{RVK_BLOB_HOST_POINTER});
  CHECK_EQ(handed.reserved, 3u);
  CHECK(handed.pointer != nullptr);
  CHECK_EQ(rig.ops().live_imports(), 1u); }

TEST(blob_imports_import, an_executor_that_refuses_names_nothing_or_names_two_imports_alike_is_not_supported)
{ ImportRig rig;
  /* One attempt per import, so a refusal is the whole answer. */
  rig.host().refuse_dmabufs();
  const std::uint32_t pages[] = {60u};
  CHECK_STATUS(rig.create_guest_blob(rig.chunk(4u), pages, 1u), Status::ok);
  std::uint64_t device_memory = 0u;
  rig.ops().refuse_import_at(1u);
  CHECK_STATUS(import(rig, rig.chunk(4u), 0u, page, 0u, device_memory), Status::not_supported);
  CHECK_EQ(rig.host().live_ranges(), 0u);
  rig.ops().withhold_handle_at(1u);
  CHECK_STATUS(import(rig, rig.chunk(4u), 0u, page, 0u, device_memory), Status::not_supported);
  CHECK_EQ(rig.ops().live_imports(), 0u);
  CHECK_EQ(rig.host().live_ranges(), 0u);
  rig.ops().answer_handle(0x77u);
  CHECK_STATUS(import(rig, rig.chunk(4u), 0u, page, 0u, device_memory), Status::ok);
  CHECK_EQ(device_memory, 0x77u);
  std::uint64_t twin = 0u;
  CHECK_STATUS(import(rig, rig.chunk(4u), 0u, page, 0u, twin), Status::not_supported);
  CHECK_EQ(twin, 0u);
  CHECK_EQ(rig.imports().imports(), 1u);
  CHECK_EQ(rig.ops().live_imports(), 1u);   /* the second was given back */
  CHECK_EQ(rig.host().live_ranges(), 1u);   /* the first still holds the alias */
  ImportRig unexecuted(false);
  CHECK_STATUS(unexecuted.create_guest_blob(unexecuted.chunk(0u), pages, 1u), Status::ok);
  CHECK_STATUS(import(unexecuted, unexecuted.chunk(0u), 0u, page, 0u, device_memory), Status::not_supported);
  CHECK_EQ(unexecuted.host().live_ranges(), 0u); }

TEST(blob_imports_import, a_context_holding_every_import_it_may_has_no_space)
{ ImportRig rig;
  const std::uint32_t pages[] = {70u};
  CHECK_STATUS(rig.create_guest_blob(rig.chunk(5u), pages, 1u), Status::ok);
  std::uint64_t handles[BlobImports::max_imports] = {};
  Status        failed = Status::ok;
  rig.on_worker([&] {
      for (std::uint32_t index = 0u; index < BlobImports::max_imports && failed == Status::ok; index++)
          failed = rig.imports().import(rig.chunk(5u), 0u, page, 0u, handles[index]);
  });
  CHECK_STATUS(failed, Status::ok);
  CHECK_EQ(rig.imports().imports(), BlobImports::max_imports);
  CHECK(rig.imports().invariants_hold());
  std::uint64_t device_memory = 0u;
  CHECK_STATUS(import(rig, rig.chunk(5u), 0u, page, 0u, device_memory), Status::no_space);
  CHECK_EQ(rig.ops().imported(), BlobImports::max_imports);
  release(rig, handles[100]);
  CHECK_STATUS(import(rig, rig.chunk(5u), 0u, page, 0u, device_memory), Status::ok);
  CHECK_EQ(rig.host().live_ranges(), 1u); }

/* The table is the host's memory, taken by the first import: a host with none
 * refuses that import and the next one tries again. */
TEST(blob_imports_import, a_host_with_no_memory_for_the_table_has_no_space_and_nothing_is_held)
{ ImportRig rig;
  const std::uint32_t pages[] = {71u};
  CHECK_STATUS(rig.create_guest_blob(rig.chunk(5u), pages, 1u), Status::ok);
  CHECK_EQ(rig.imports().imports(), 0u);
  const std::uint32_t allocations = rig.host().live_allocations();
  std::uint64_t device_memory = 0u;
  rig.host().fail_allocation_at(1u);
  CHECK_STATUS(import(rig, rig.chunk(5u), 0u, page, 0u, device_memory), Status::no_space);
  CHECK_EQ(rig.host().live_ranges(), 0u);
  CHECK_EQ(rig.host().live_allocations(), allocations);
  CHECK_STATUS(import(rig, rig.chunk(5u), 0u, page, 0u, device_memory), Status::ok);
  CHECK_EQ(rig.host().live_allocations(), allocations + 1u);
  CHECK_STATUS(import(rig, rig.chunk(5u), 0u, page, 0u, device_memory), Status::ok);
  CHECK_EQ(rig.host().live_allocations(), allocations + 1u); }

/* A chunk unbound and bound again reuses its id over other pages: the blob is
 * not released while an import holds its alias, so the old pages are never
 * handed out under the new blob's id. */
TEST(blob_imports_import, a_held_chunk_is_not_released_and_its_successor_is_imported_over_its_own_pages)
{ ImportRig rig;
  const std::uint32_t old_pages[] = {80u};
  const std::uint32_t new_pages[] = {81u};
  CHECK_STATUS(rig.create_guest_blob(rig.chunk(6u), old_pages, 1u), Status::ok);
  std::uint64_t held = 0u;
  CHECK_STATUS(import(rig, rig.chunk(6u), 0u, page, 0u, held), Status::ok);
  CHECK_STATUS(rig.blobs().release(rig.chunk(6u)), Status::invalid_state);
  release(rig, held);
  CHECK_STATUS(rig.blobs().release(rig.chunk(6u)), Status::ok);
  CHECK_STATUS(rig.create_guest_blob(rig.chunk(6u), new_pages, 1u), Status::ok);
  std::uint64_t device_memory = 0u;
  CHECK_STATUS(import(rig, rig.chunk(6u), 0u, page, 0u, device_memory), Status::ok);
  RvkBlobHandle handed = {};
  CHECK(rig.ops().import_of(2u, handed));
  Handed successor(handed);
  CHECK(successor.bytes() != nullptr);
  CHECK_EQ(successor.bytes()[0], static_cast<std::uint8_t>(0x10u + 81u)); }

TEST(blob_imports_release, a_handle_this_context_never_made_releases_nothing)
{ ImportRig rig;
  release(rig, 1u | MockExecutorOps::handle_bit);   /* before any import: there is no table to look in */
  CHECK_EQ(rig.imports().imports(), 0u);
  CHECK_EQ(rig.host().live_allocations(), 0u);
  const std::uint32_t pages[] = {90u};
  CHECK_STATUS(rig.create_guest_blob(rig.chunk(7u), pages, 1u), Status::ok);
  std::uint64_t device_memory = 0u;
  CHECK_STATUS(import(rig, rig.chunk(7u), 0u, page, 0u, device_memory), Status::ok);
  release(rig, 0u);
  release(rig, device_memory + 1u);
  CHECK_EQ(rig.ops().released(), 0u);
  CHECK_EQ(rig.imports().imports(), 1u); }

TEST(blob_imports, the_aliases_still_held_go_with_the_context)
{ ImportRig rig;
  const std::uint32_t pages[] = {100u, 102u};
  CHECK_STATUS(rig.create_guest_blob(rig.chunk(8u), pages, 2u), Status::ok);
  CHECK_STATUS(rig.create_guest_blob(rig.chunk(9u), pages, 2u), Status::ok);
  const std::uint32_t allocations = rig.host().live_allocations();
  {
      BlobImports context(rig.host(), rig.blobs(), rig.executor(), rig.recorder(), 1u);
      context.configure(ImportRig::ring);
      std::uint64_t first = 0u;
      std::uint64_t second = 0u;
      std::uint64_t third = 0u;
      rig.on_worker([&] {
          CHECK_STATUS(context.import(rig.chunk(8u), 0u, page, 0u, first), Status::ok);
          CHECK_STATUS(context.import(rig.chunk(8u), page, page, 0u, second), Status::ok);
          CHECK_STATUS(context.import(rig.chunk(9u), page, page, 0u, third), Status::ok);
      });
      CHECK_EQ(context.imports(), 3u);
      CHECK_EQ(rig.host().live_ranges(), 2u);
      CHECK_STATUS(rig.blobs().release(rig.chunk(8u)), Status::invalid_state);
  }
  CHECK_EQ(rig.host().live_ranges(), 0u);
  CHECK_EQ(rig.host().live_aliases(), 0u);
  CHECK_EQ(rig.host().live_allocations(), allocations);
  CHECK_STATUS(rig.blobs().release(rig.chunk(8u)), Status::ok);
  CHECK_STATUS(rig.blobs().release(rig.chunk(9u)), Status::ok);
  const std::uint32_t without_blobs = rig.host().live_allocations();
  { BlobImports unused(rig.host(), rig.blobs(), rig.executor(), rig.recorder(), 1u); }   /* never imported: nothing to let go */
  CHECK_EQ(rig.host().live_allocations(), without_blobs); }

TEST(blob_imports, configures_on_the_main_loop_and_imports_on_the_worker)
{ ImportRig rig;
  std::uint64_t device_memory = 0u;
  CHECK_STATUS(rig.imports().import(rig.chunk(0u), 0u, page, 0u, device_memory), Status::not_found);
  rig.imports().release(1u);
  CHECK(rig.host().claim(HostViolation::worker_required));
  CHECK(rig.host().claim(HostViolation::worker_required));
  rig.on_worker([&] { rig.imports().configure(ImportRig::ring); });
  CHECK(rig.host().claim(HostViolation::main_loop_required)); }

TEST(blob_imports_invariants, each_rule_is_detected)
{ ImportRig rig;
  CHECK(rig.imports().invariants_hold());
  const std::uint32_t pages[] = {110u};
  CHECK_STATUS(rig.create_guest_blob(rig.chunk(10u), pages, 1u), Status::ok);
  std::uint64_t device_memory = 0u;
  CHECK_STATUS(import(rig, rig.chunk(10u), 0u, page, 0u, device_memory), Status::ok);
  CHECK_STATUS(import(rig, rig.chunk(10u), 0u, page, 0u, device_memory), Status::ok);
  const std::uint64_t kept_handle =
      av::BlobImportsProbe::swap_handle(rig.imports(), av::BlobImportsProbe::first_handle(rig.imports()));
  CHECK(!rig.imports().invariants_hold());   /* two imports answering to one handle */
  static_cast<void>(av::BlobImportsProbe::swap_handle(rig.imports(), kept_handle));
  CHECK(rig.imports().invariants_hold());
  const std::uint32_t kept_resource = av::BlobImportsProbe::swap_resource(rig.imports(), ImportRig::ring + 2u);
  CHECK(!rig.imports().invariants_hold());   /* an import of the reply slot */
  static_cast<void>(av::BlobImportsProbe::swap_resource(rig.imports(), kept_resource));
  const std::uint64_t kept_id = av::BlobImportsProbe::swap_blob_id(rig.imports(), 0u);
  CHECK(!rig.imports().invariants_hold());   /* an id the device never hands out */
  static_cast<void>(av::BlobImportsProbe::swap_blob_id(rig.imports(), 3u));
  CHECK(!rig.imports().invariants_hold());   /* an id not handed out yet */
  static_cast<void>(av::BlobImportsProbe::swap_blob_id(rig.imports(), kept_id));
  CHECK(rig.imports().invariants_hold()); }

TEST(blob_imports_import, records_the_blob_it_handed_over_and_the_bytes_behind_it)
{ ImportRig rig;
  const std::uint32_t pages[] = {7u, 8u};
  CHECK_STATUS(rig.create_guest_blob(rig.chunk(3u), pages, 2u), Status::ok);
  CHECK_STATUS(rig.recorder().open("imports.rvk", 0u, "all", av::AtCeiling::stop, av::GpuIdentity(),
                                   av::ProtocolVersion(1u, 1u)),
               Status::ok);
  std::uint64_t device_memory = 0u;
  rig.on_worker([&] {
      CHECK_STATUS(rig.imports().import(rig.chunk(3u), page, page, 5u, device_memory), Status::ok);
  });
  CHECK(rig.imports().invariants_hold());
  av::RecordedFile    file(rig.host(), "imports.rvk");
  av::RecordHeader    record;
  const std::uint8_t *payload = file.expect(av::RecordKind::blob, record);
  CHECK(payload != nullptr);
  if (payload != nullptr) {
      CHECK_EQ(payload[0], 1u);            /* imported */
      CHECK_EQ(payload[4], 5u);            /* the memory type the guest asked for */
      CHECK_EQ(payload[16], page & 0xFFu); /* the offset */
      CHECK_EQ(payload[17], (page >> 8) & 0xFFu);
  }
  CHECK_EQ(record.context(), 1u);
  payload = file.expect(av::RecordKind::blob_bytes, record);
  CHECK(payload != nullptr);
  CHECK_EQ(record.length(), 16u + page);
  /* The second page of the blob is guest page 8, which the rig filled with
   * 0x10 + 8; a replay recreates the import from exactly these bytes. */
  if (payload != nullptr) CHECK_EQ(payload[16], 0x18u);
  CHECK_EQ(file.remaining(), 0u);

  rig.on_worker([&] { rig.imports().release(device_memory); });
  av::RecordedFile after(rig.host(), "imports.rvk");
  CHECK_EQ(after.remaining(), 3u);
  rig.recorder().close(); }

TEST(blob_imports_import, a_recording_that_does_not_keep_a_blobs_bytes_records_only_its_lifetime)
{ ImportRig rig;
  const std::uint32_t pages[] = {7u};
  CHECK_STATUS(rig.create_guest_blob(rig.chunk(4u), pages, 1u), Status::ok);
  CHECK_STATUS(rig.recorder().open("lifetimes.rvk", 0u, "blob", av::AtCeiling::stop, av::GpuIdentity(),
                                   av::ProtocolVersion(1u, 1u)),
               Status::ok);
  std::uint64_t device_memory = 0u;
  rig.on_worker([&] {
      CHECK_STATUS(rig.imports().import(rig.chunk(4u), 0u, page, 0u, device_memory), Status::ok);
      rig.imports().release(device_memory);
  });
  av::RecordedFile file(rig.host(), "lifetimes.rvk");
  av::RecordHeader record;
  CHECK(file.expect(av::RecordKind::blob, record) != nullptr);
  CHECK(file.expect(av::RecordKind::blob, record) != nullptr);
  CHECK_EQ(file.remaining(), 0u);
  rig.recorder().close(); }

TEST(blob_imports_import, an_import_the_executor_refuses_records_nothing)
{ ImportRig rig;
  const std::uint32_t pages[] = {7u};
  CHECK_STATUS(rig.create_guest_blob(rig.chunk(5u), pages, 1u), Status::ok);
  CHECK_STATUS(rig.recorder().open("refused.rvk", 0u, "all", av::AtCeiling::stop, av::GpuIdentity(),
                                   av::ProtocolVersion(1u, 1u)),
               Status::ok);
  rig.ops().refuse_import_at(1u);
  std::uint64_t device_memory = 0u;
  rig.on_worker([&] {
      CHECK_STATUS(rig.imports().import(rig.chunk(5u), 0u, page, 0u, device_memory), Status::not_supported);
  });
  CHECK_EQ(av::RecordedFile(rig.host(), "refused.rvk").remaining(), 0u);
  rig.recorder().close(); }
