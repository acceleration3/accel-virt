/* One context's imports of host blobs into its executor (docs/host-memory.md):
 * only a blob made for this context is imported, the executor is handed the
 * memory it allocated under the blob's id, every import is named by a handle
 * no other answers to, and every refusal leaves nothing new behind. Everything
 * runs over ImportRig - the device's host blob table allocating through a
 * mock executor library with real memory, and the context's one worker. */
#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "device/execution/blob_imports.hpp"
#include "device/recording/wire_recorder.hpp"
#include "harness/executor/import_rig.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/recording/recorded_file.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"

#include <cstdint>

using av::BlobImports;
using av::HostViolation;
using av::ImportRig;
using av::MockExecutorOps;
using av::Status;

namespace av {

struct BlobImportsProbe {
    static std::uint64_t swap_handle(BlobImports &imports, std::uint32_t slot, std::uint64_t handle)
    {
        const std::uint64_t kept = imports.device_memory_[slot];
        imports.device_memory_[slot] = handle;
        return kept;
    }
    static std::uint64_t handle(BlobImports &imports, std::uint32_t slot) { return imports.device_memory_[slot]; }
    static std::uint64_t swap_blob_id(BlobImports &imports, std::uint64_t blob_id)
    {
        const std::uint64_t kept = imports.blob_ids_[0];
        imports.blob_ids_[0] = blob_id;
        return kept;
    }
};

}  // namespace av

namespace {

/* A little-endian u64 of a record's payload, as docs/protocol.md frames one. */
std::uint64_t read_u64(const std::uint8_t *bytes)
{
    std::uint64_t value = 0u;
    for (std::uint32_t index = 0u; index < 8u; index++)
        value |= static_cast<std::uint64_t>(bytes[index]) << (8u * index);
    return value;
}

/* An import on the worker: its status, with the handle in `device_memory`. */
Status import(ImportRig &rig, std::uint32_t resource, std::uint64_t &device_memory)
{
    Status status = Status::invalid_state;
    rig.on_worker([&] { status = rig.imports().import(resource, device_memory); });
    CHECK(rig.imports().invariants_hold());
    return status;
}

void release(ImportRig &rig, std::uint64_t device_memory)
{
    rig.on_worker([&] { rig.imports().release(device_memory); });
    CHECK(rig.imports().invariants_hold());
}

}  // namespace

/* The executor is handed the very memory the library allocated under the
 * blob's id - not a copy, and not another blob's. */
TEST(blob_imports_import, hands_the_executor_the_memory_allocated_under_the_blob_s_id)
{ ImportRig rig;
  CHECK_STATUS(rig.create_host_blob(rig.blob(3u), ImportRig::context, 2u), Status::ok);
  CHECK_STATUS(rig.create_host_blob(rig.blob(4u), ImportRig::context), Status::ok);
  std::uint64_t device_memory = 0u;
  CHECK_STATUS(import(rig, rig.blob(3u), device_memory), Status::ok);
  CHECK(device_memory != 0u);
  std::uint64_t key = 0u;
  CHECK(rig.ops().import_of(1u, key));   /* the first import id this context handed out */
  CHECK_EQ(key, rig.blob(3u));
  CHECK_EQ(device_memory, 1u | MockExecutorOps::handle_bit);
  CHECK_EQ(rig.imports().imports(), 1u);
  CHECK_EQ(rig.ops().imported(), 1u); }

TEST(blob_imports_import, each_import_gets_an_id_never_used_before_and_its_own_handle)
{ ImportRig rig;
  CHECK_STATUS(rig.create_host_blob(rig.blob(0u), ImportRig::context), Status::ok);
  std::uint64_t first = 0u;
  std::uint64_t second = 0u;
  std::uint64_t third = 0u;
  CHECK_STATUS(import(rig, rig.blob(0u), first), Status::ok);
  CHECK_STATUS(import(rig, rig.blob(0u), second), Status::ok);   /* the same blob twice is two imports */
  release(rig, first);
  CHECK_STATUS(import(rig, rig.blob(0u), third), Status::ok);
  CHECK_EQ(first, 1u | MockExecutorOps::handle_bit);
  CHECK_EQ(second, 2u | MockExecutorOps::handle_bit);
  CHECK_EQ(third, 3u | MockExecutorOps::handle_bit);   /* id 1 is not reused */
  CHECK_EQ(rig.imports().imports(), 2u); }

/* A context imports only what its own process opened: another context's blob,
 * an id that is no host blob, and a blob already released are not found, and
 * the executor is never asked. */
TEST(blob_imports_import, a_blob_this_context_did_not_open_is_not_found_and_asks_nothing)
{ ImportRig rig;
  CHECK_STATUS(rig.create_host_blob(rig.blob(1u), ImportRig::other_context), Status::ok);
  CHECK_STATUS(rig.create_host_blob(rig.blob(2u), ImportRig::context), Status::ok);
  CHECK_STATUS(rig.host_blobs().release(rig.blob(2u)), Status::ok);
  std::uint64_t device_memory = 7u;
  CHECK_STATUS(import(rig, rig.blob(1u), device_memory), Status::not_found);
  CHECK_EQ(device_memory, 0u);
  CHECK_STATUS(import(rig, rig.blob(2u), device_memory), Status::not_found);
  CHECK_STATUS(import(rig, rig.blob(5u), device_memory), Status::not_found);
  CHECK_STATUS(import(rig, 12u, device_memory), Status::not_found);
  CHECK_STATUS(import(rig, 0u, device_memory), Status::not_found);
  CHECK_EQ(rig.ops().imported(), 0u);
  CHECK_EQ(rig.imports().imports(), 0u);
  CHECK_EQ(rig.host().live_allocations(), 0u);   /* no table was taken for a refusal */ }

/* The library keeps the memory for as long as the import holds it, whatever
 * the guest releases meanwhile. */
TEST(blob_imports_import, an_import_keeps_the_memory_after_the_guest_releases_the_blob)
{ ImportRig rig;
  CHECK_STATUS(rig.create_host_blob(rig.blob(6u), ImportRig::context), Status::ok);
  rig.memory_of(rig.blob(6u))[5] = 0x5Eu;
  std::uint64_t device_memory = 0u;
  CHECK_STATUS(import(rig, rig.blob(6u), device_memory), Status::ok);
  CHECK_STATUS(rig.host_blobs().release(rig.blob(6u)), Status::ok);
  CHECK_EQ(rig.ops().held_memory(), 0u);
  CHECK_EQ(rig.ops().live_memory(), 1u);
  CHECK(rig.memory_of(rig.blob(6u)) != nullptr);
  CHECK_EQ(rig.memory_of(rig.blob(6u))[5], 0x5Eu);
  release(rig, device_memory);
  CHECK_EQ(rig.ops().live_memory(), 0u); }

TEST(blob_imports_import, an_executor_that_refuses_names_nothing_or_names_two_imports_alike_is_not_supported)
{ ImportRig rig;
  CHECK_STATUS(rig.create_host_blob(rig.blob(4u), ImportRig::context), Status::ok);
  std::uint64_t device_memory = 0u;
  rig.ops().refuse_import_at(1u);
  CHECK_STATUS(import(rig, rig.blob(4u), device_memory), Status::not_supported);
  CHECK_EQ(device_memory, 0u);
  rig.ops().withhold_handle_at(1u);
  CHECK_STATUS(import(rig, rig.blob(4u), device_memory), Status::not_supported);
  CHECK_EQ(rig.ops().live_imports(), 0u);   /* the nameless import was given back */
  rig.ops().answer_handle(0x77u);
  CHECK_STATUS(import(rig, rig.blob(4u), device_memory), Status::ok);
  CHECK_EQ(device_memory, 0x77u);
  std::uint64_t twin = 0u;
  CHECK_STATUS(import(rig, rig.blob(4u), twin), Status::not_supported);
  CHECK_EQ(twin, 0u);
  CHECK_EQ(rig.imports().imports(), 1u);
  CHECK_EQ(rig.ops().live_imports(), 1u);   /* the second was given back */
  ImportRig unexecuted(false);
  CHECK_STATUS(unexecuted.create_host_blob(unexecuted.blob(0u), ImportRig::context), Status::ok);
  CHECK_STATUS(import(unexecuted, unexecuted.blob(0u), device_memory), Status::not_supported);
  CHECK_EQ(unexecuted.imports().imports(), 0u); }

TEST(blob_imports_import, a_context_holding_every_import_it_may_has_no_space)
{ ImportRig rig;
  CHECK_STATUS(rig.create_host_blob(rig.blob(5u), ImportRig::context), Status::ok);
  static std::uint64_t handles[BlobImports::max_imports] = {};
  Status               failed = Status::ok;
  rig.on_worker([&] {
      for (std::uint32_t index = 0u; index < BlobImports::max_imports && failed == Status::ok; index++)
          failed = rig.imports().import(rig.blob(5u), handles[index]);
  });
  CHECK_STATUS(failed, Status::ok);
  CHECK_EQ(rig.imports().imports(), BlobImports::max_imports);
  CHECK(rig.imports().invariants_hold());
  std::uint64_t device_memory = 0u;
  CHECK_STATUS(import(rig, rig.blob(5u), device_memory), Status::no_space);
  CHECK_EQ(rig.ops().imported(), BlobImports::max_imports);   /* the executor was not asked */
  release(rig, handles[100]);
  CHECK_STATUS(import(rig, rig.blob(5u), device_memory), Status::ok);
  CHECK_EQ(rig.imports().imports(), BlobImports::max_imports); }

/* The table is the host's memory, taken by the first import: a host with none
 * refuses that import and the next one tries again. */
TEST(blob_imports_import, a_host_with_no_memory_for_the_table_has_no_space_and_nothing_is_held)
{ ImportRig rig;
  CHECK_STATUS(rig.create_host_blob(rig.blob(5u), ImportRig::context), Status::ok);
  const std::uint32_t allocations = rig.host().live_allocations();
  std::uint64_t device_memory = 0u;
  rig.host().fail_allocation_at(1u);
  CHECK_STATUS(import(rig, rig.blob(5u), device_memory), Status::no_space);
  CHECK_EQ(rig.host().live_allocations(), allocations);
  CHECK_EQ(rig.ops().imported(), 0u);
  CHECK_STATUS(import(rig, rig.blob(5u), device_memory), Status::ok);
  CHECK_EQ(rig.host().live_allocations(), allocations + 1u);
  CHECK_STATUS(import(rig, rig.blob(5u), device_memory), Status::ok);
  CHECK_EQ(rig.host().live_allocations(), allocations + 1u); }

TEST(blob_imports_release, lets_go_of_the_import_its_handle_names_and_only_that)
{ ImportRig rig;
  CHECK_STATUS(rig.create_host_blob(rig.blob(7u), ImportRig::context), Status::ok);
  std::uint64_t first = 0u;
  std::uint64_t second = 0u;
  CHECK_STATUS(import(rig, rig.blob(7u), first), Status::ok);
  CHECK_STATUS(import(rig, rig.blob(7u), second), Status::ok);
  release(rig, second);
  CHECK_EQ(rig.ops().released(), 1u);
  CHECK_EQ(rig.imports().imports(), 1u);
  std::uint64_t key = 0u;
  CHECK(rig.ops().import_of(1u, key));
  CHECK(!rig.ops().import_of(2u, key));
  release(rig, second);   /* already let go: nothing */
  CHECK_EQ(rig.ops().released(), 1u); }

TEST(blob_imports_release, a_handle_this_context_never_made_releases_nothing)
{ ImportRig rig;
  release(rig, 1u | MockExecutorOps::handle_bit);   /* before any import: there is no table to look in */
  CHECK_EQ(rig.imports().imports(), 0u);
  CHECK_EQ(rig.host().live_allocations(), 0u);
  CHECK_STATUS(rig.create_host_blob(rig.blob(7u), ImportRig::context), Status::ok);
  std::uint64_t device_memory = 0u;
  CHECK_STATUS(import(rig, rig.blob(7u), device_memory), Status::ok);
  release(rig, 0u);   /* 0 names no import, though a free slot holds 0 */
  release(rig, device_memory + 1u);
  CHECK_EQ(rig.ops().released(), 0u);
  CHECK_EQ(rig.imports().imports(), 1u); }

/* A context going away gives back its table; what it imported the executor
 * releases with itself. */
TEST(blob_imports, the_table_goes_with_the_context)
{ ImportRig rig;
  CHECK_STATUS(rig.create_host_blob(rig.blob(8u), ImportRig::context), Status::ok);
  const std::uint32_t allocations = rig.host().live_allocations();
  {
      BlobImports context(rig.host(), rig.host_blobs(), rig.executor(), rig.recorder(), ImportRig::context);
      std::uint64_t first = 0u;
      rig.on_worker([&] { CHECK_STATUS(context.import(rig.blob(8u), first), Status::ok); });
      CHECK_EQ(context.imports(), 1u);
      CHECK_EQ(rig.host().live_allocations(), allocations + 1u);
  }
  CHECK_EQ(rig.host().live_allocations(), allocations);
  { BlobImports unused(rig.host(), rig.host_blobs(), rig.executor(), rig.recorder(), ImportRig::context); }
  CHECK_EQ(rig.host().live_allocations(), allocations); }

TEST(blob_imports, imports_and_releases_on_the_worker_only)
{ ImportRig rig;
  std::uint64_t device_memory = 0u;
  CHECK_STATUS(rig.imports().import(rig.blob(0u), device_memory), Status::not_found);
  rig.imports().release(1u);
  CHECK(rig.host().claim(HostViolation::worker_required));
  CHECK(rig.host().claim(HostViolation::worker_required));
  CHECK(!rig.host().claim(HostViolation::worker_required)); }

TEST(blob_imports_invariants, each_rule_is_detected)
{ ImportRig rig;
  CHECK(rig.imports().invariants_hold());
  CHECK_STATUS(rig.create_host_blob(rig.blob(10u), ImportRig::context), Status::ok);
  std::uint64_t device_memory = 0u;
  CHECK_STATUS(import(rig, rig.blob(10u), device_memory), Status::ok);
  CHECK_STATUS(import(rig, rig.blob(10u), device_memory), Status::ok);
  const std::uint64_t kept_handle =
      av::BlobImportsProbe::swap_handle(rig.imports(), 1u, av::BlobImportsProbe::handle(rig.imports(), 0u));
  CHECK(!rig.imports().invariants_hold());   /* two imports answering to one handle */
  static_cast<void>(av::BlobImportsProbe::swap_handle(rig.imports(), 1u, kept_handle));
  CHECK(rig.imports().invariants_hold());
  const std::uint64_t kept_id = av::BlobImportsProbe::swap_blob_id(rig.imports(), 0u);
  CHECK(!rig.imports().invariants_hold());   /* an id the device never hands out */
  static_cast<void>(av::BlobImportsProbe::swap_blob_id(rig.imports(), 3u));
  CHECK(!rig.imports().invariants_hold());   /* an id not handed out yet */
  static_cast<void>(av::BlobImportsProbe::swap_blob_id(rig.imports(), 2u));
  CHECK(rig.imports().invariants_hold());    /* the last one handed out */
  static_cast<void>(av::BlobImportsProbe::swap_blob_id(rig.imports(), kept_id));
  CHECK(rig.imports().invariants_hold()); }

/* What a replay pairs an import with its release by: the action, the import's
 * id and the resource it imported. */
TEST(blob_imports_import, records_the_import_and_its_release_under_one_id)
{ ImportRig rig;
  CHECK_STATUS(rig.create_host_blob(rig.blob(3u), ImportRig::context), Status::ok);
  CHECK_STATUS(rig.recorder().open("imports.av", 0u, "all", av::AtCeiling::stop, av::GpuIdentity(),
                                   av::ProtocolVersion(1u, 1u)),
               Status::ok);
  std::uint64_t device_memory = 0u;
  CHECK_STATUS(import(rig, rig.blob(3u), device_memory), Status::ok);
  release(rig, device_memory);
  av::RecordedFile    file(rig.host(), "imports.av");
  av::RecordHeader    record;
  const std::uint8_t *imported = file.expect(av::RecordKind::blob, record);
  CHECK(imported != nullptr);
  CHECK_EQ(record.context(), ImportRig::context);
  CHECK_EQ(record.length(), av::WireRecorder::blob_payload);
  if (imported != nullptr) {
      CHECK_EQ(imported[0], 1u);   /* imported */
      CHECK_EQ(read_u64(imported + 8u), 1u);
      CHECK_EQ(read_u64(imported + 24u), rig.blob(3u));
  }
  const std::uint8_t *released = file.expect(av::RecordKind::blob, record);
  CHECK(released != nullptr);
  if (released != nullptr) {
      CHECK_EQ(released[0], 2u);   /* released */
      CHECK_EQ(read_u64(released + 8u), 1u);
  }
  CHECK_EQ(file.remaining(), 0u);
  rig.recorder().close(); }

TEST(blob_imports_import, an_import_refused_records_nothing)
{ ImportRig rig;
  CHECK_STATUS(rig.create_host_blob(rig.blob(5u), ImportRig::context), Status::ok);
  CHECK_STATUS(rig.recorder().open("refused.av", 0u, "all", av::AtCeiling::stop, av::GpuIdentity(),
                                   av::ProtocolVersion(1u, 1u)),
               Status::ok);
  rig.ops().refuse_import_at(1u);
  std::uint64_t device_memory = 0u;
  CHECK_STATUS(import(rig, rig.blob(5u), device_memory), Status::not_supported);
  CHECK_STATUS(import(rig, rig.blob(6u), device_memory), Status::not_found);
  release(rig, 5u);
  CHECK_EQ(av::RecordedFile(rig.host(), "refused.av").remaining(), 0u);
  rig.recorder().close(); }
