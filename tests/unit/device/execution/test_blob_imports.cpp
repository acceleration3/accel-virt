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
    static void name_one_zero(BlobImports &imports, std::uint64_t blob_id)
    {
        std::uint32_t slot = 0u;
        static_cast<void>(imports.blob_ids_.insert(0u, blob_id, slot));
    }
    static void forget_zero(BlobImports &imports) { static_cast<void>(imports.blob_ids_.erase(0u)); }
    static std::uint64_t swap_blob_id(BlobImports &imports, std::uint64_t blob_id)
    {
        std::uint64_t *const kept = imports.blob_ids_.at(0u);
        const std::uint64_t  previous = *kept;
        *kept = blob_id;
        return previous;
    }
};

}

namespace {
std::uint64_t read_u64(const std::uint8_t *bytes)
{
    std::uint64_t value = 0u;
    for (std::uint32_t index = 0u; index < 8u; index++)
        value |= static_cast<std::uint64_t>(bytes[index]) << (8u * index);
    return value;
}

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

}

TEST(blob_imports_import, hands_the_executor_the_memory_allocated_under_the_blob_s_id)
{ ImportRig rig;
  CHECK_STATUS(rig.create_host_blob(rig.blob(3u), ImportRig::context, 2u), Status::ok);
  CHECK_STATUS(rig.create_host_blob(rig.blob(4u), ImportRig::context), Status::ok);
  std::uint64_t device_memory = 0u;
  CHECK_STATUS(import(rig, rig.blob(3u), device_memory), Status::ok);
  CHECK(device_memory != 0u);
  std::uint64_t key = 0u;
  CHECK(rig.ops().import_of(1u, key));
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
  CHECK_STATUS(import(rig, rig.blob(0u), second), Status::ok);
  release(rig, first);
  CHECK_STATUS(import(rig, rig.blob(0u), third), Status::ok);
  CHECK_EQ(first, 1u | MockExecutorOps::handle_bit);
  CHECK_EQ(second, 2u | MockExecutorOps::handle_bit);
  CHECK_EQ(third, 3u | MockExecutorOps::handle_bit);
  CHECK_EQ(rig.imports().imports(), 2u); }

TEST(blob_imports_import, a_blob_this_context_did_not_open_is_not_found_and_asks_nothing)
{ ImportRig rig;
  CHECK_STATUS(rig.create_host_blob(rig.blob(1u), ImportRig::other_context), Status::ok);
  CHECK_STATUS(rig.create_host_blob(rig.blob(2u), ImportRig::context), Status::ok);
  CHECK_STATUS(rig.host_blobs().release(rig.blob(2u)), Status::ok);
  const std::uint32_t allocations = rig.host().live_allocations();
  std::uint64_t device_memory = 7u;
  CHECK_STATUS(import(rig, rig.blob(1u), device_memory), Status::not_found);
  CHECK_EQ(device_memory, 0u);
  CHECK_STATUS(import(rig, rig.blob(2u), device_memory), Status::not_found);
  CHECK_STATUS(import(rig, rig.blob(5u), device_memory), Status::not_found);
  CHECK_STATUS(import(rig, 12u, device_memory), Status::not_found);
  CHECK_STATUS(import(rig, 0u, device_memory), Status::not_found);
  CHECK_EQ(rig.ops().imported(), 0u);
  CHECK_EQ(rig.imports().imports(), 0u);
  CHECK_EQ(rig.host().live_allocations(), allocations);   }

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
  CHECK_EQ(rig.ops().live_imports(), 0u);
  rig.ops().answer_handle(0x77u);
  CHECK_STATUS(import(rig, rig.blob(4u), device_memory), Status::ok);
  CHECK_EQ(device_memory, 0x77u);
  std::uint64_t twin = 0u;
  CHECK_STATUS(import(rig, rig.blob(4u), twin), Status::not_supported);
  CHECK_EQ(twin, 0u);
  CHECK_EQ(rig.imports().imports(), 1u);
  CHECK_EQ(rig.ops().live_imports(), 1u);
  ImportRig unexecuted(false);
  CHECK_STATUS(unexecuted.create_host_blob(unexecuted.blob(0u), ImportRig::context), Status::ok);
  CHECK_STATUS(import(unexecuted, unexecuted.blob(0u), device_memory), Status::not_supported);
  CHECK_EQ(unexecuted.imports().imports(), 0u); }

TEST(blob_imports_import, a_context_imports_well_past_the_old_table)
{ ImportRig rig;
  CHECK_STATUS(rig.create_host_blob(rig.blob(5u), ImportRig::context), Status::ok);
  constexpr std::uint32_t many = 6000u;
  static std::uint64_t    handles[many] = {};
  Status                  failed = Status::ok;
  rig.on_worker([&] {
      for (std::uint32_t index = 0u; index < many && failed == Status::ok; index++)
          failed = rig.imports().import(rig.blob(5u), handles[index]);
  });
  CHECK_STATUS(failed, Status::ok);
  CHECK_EQ(rig.imports().imports(), many);
  CHECK_EQ(rig.ops().live_imports(), many);
  CHECK(rig.imports().invariants_hold());
  for (std::uint32_t index = 0u; index < many; index++) release(rig, handles[index]);
  CHECK_EQ(rig.imports().imports(), 0u);
  CHECK_EQ(rig.ops().live_imports(), 0u); }

TEST(blob_imports_import, a_host_with_no_memory_for_the_table_is_out_of_memory_and_nothing_is_held)
{ ImportRig rig;
  CHECK_STATUS(rig.create_host_blob(rig.blob(5u), ImportRig::context), Status::ok);
  const std::uint32_t allocations = rig.host().live_allocations();
  std::uint64_t device_memory = 0u;
  rig.host().fail_allocation_at(1u);
  CHECK_STATUS(import(rig, rig.blob(5u), device_memory), Status::no_memory);
  CHECK_EQ(device_memory, 0u);
  CHECK_EQ(rig.host().live_allocations(), allocations);
  CHECK_EQ(rig.ops().imported(), 0u);
  CHECK_STATUS(import(rig, rig.blob(5u), device_memory), Status::ok);
  CHECK(rig.host().live_allocations() > allocations);
  CHECK_EQ(rig.ops().imported(), 1u); }

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
  release(rig, second);
  CHECK_EQ(rig.ops().released(), 1u); }

TEST(blob_imports_release, a_handle_this_context_never_made_releases_nothing)
{ ImportRig rig;
  release(rig, 1u | MockExecutorOps::handle_bit);
  CHECK_EQ(rig.imports().imports(), 0u);
  CHECK_EQ(rig.host().live_allocations(), 0u);
  CHECK_STATUS(rig.create_host_blob(rig.blob(7u), ImportRig::context), Status::ok);
  std::uint64_t device_memory = 0u;
  CHECK_STATUS(import(rig, rig.blob(7u), device_memory), Status::ok);
  release(rig, 0u);
  release(rig, device_memory + 1u);
  CHECK_EQ(rig.ops().released(), 0u);
  CHECK_EQ(rig.imports().imports(), 1u); }

TEST(blob_imports, the_table_goes_with_the_context)
{ ImportRig rig;
  CHECK_STATUS(rig.create_host_blob(rig.blob(8u), ImportRig::context), Status::ok);
  const std::uint32_t allocations = rig.host().live_allocations();
  {
      BlobImports context(rig.host(), rig.host_blobs(), rig.executor(), rig.recorder(), ImportRig::context);
      std::uint64_t first = 0u;
      rig.on_worker([&] { CHECK_STATUS(context.import(rig.blob(8u), first), Status::ok); });
      CHECK_EQ(context.imports(), 1u);
      CHECK(rig.host().live_allocations() > allocations);
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
  av::BlobImportsProbe::name_one_zero(rig.imports(), 1u);
  CHECK(!rig.imports().invariants_hold());
  av::BlobImportsProbe::forget_zero(rig.imports());
  CHECK(rig.imports().invariants_hold());
  const std::uint64_t kept_id = av::BlobImportsProbe::swap_blob_id(rig.imports(), 0u);
  CHECK(!rig.imports().invariants_hold());
  static_cast<void>(av::BlobImportsProbe::swap_blob_id(rig.imports(), 3u));
  CHECK(!rig.imports().invariants_hold());
  static_cast<void>(av::BlobImportsProbe::swap_blob_id(rig.imports(), 2u));
  CHECK(rig.imports().invariants_hold());
  static_cast<void>(av::BlobImportsProbe::swap_blob_id(rig.imports(), kept_id));
  CHECK(rig.imports().invariants_hold()); }

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
      CHECK_EQ(imported[0], 1u);
      CHECK_EQ(read_u64(imported + 8u), 1u);
      CHECK_EQ(read_u64(imported + 24u), rig.blob(3u));
  }
  const std::uint8_t *released = file.expect(av::RecordKind::blob, record);
  CHECK(released != nullptr);
  if (released != nullptr) {
      CHECK_EQ(released[0], 2u);
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
