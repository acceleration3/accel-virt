#include "core/support/status.hpp"
#include "device/control/control_request.hpp"
#include "device/memory/blob_resource.hpp"
#include "device/memory/blob_table.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/gpu_request_builder.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"

#include <cstdint>

using av::BlobTable;
using av::ControlRequest;
using av::GpuRequestBuilder;
using av::HostViolation;
using av::MockHost;
using av::MockRequest;
using av::Status;

namespace av {

struct BlobTableProbe {
    /* A host blob mapped at `offset` in a free slot, behind the table's back. */
    static void place(BlobTable &table, std::uint32_t id, std::uint64_t offset)
    {
        for (ObjectStorage<BlobResource> &slot : table.slots_) {
            if (slot.construct(table.host_, id) != Status::ok) continue;
            static_cast<void>(slot.object().back_with_host(0x1000u));
            static_cast<void>(slot.object().map(offset));
            return;
        }
    }
};

}  // namespace av

namespace {

constexpr std::uint64_t ram = 0x100000u;

class Fixture {
public:
    Fixture() : table_(host_)
    {
        host_.add_guest_ram(ram, 0x10000u);
        host_.set_hostmem_size(0x10000u);
    }

    MockHost  &host() { return host_; }
    BlobTable &table() { return table_; }

private:
    MockHost  host_;
    BlobTable table_;
};

}  // namespace

TEST(blob_table_create_guest, keeps_ids_unique)
{ Fixture fixture;
  GpuRequestBuilder bytes(0u);
  bytes.u64(ram).u32(0x1000u).u32(0u);
  MockRequest mock(bytes.bytes(), bytes.size());
  ControlRequest request(fixture.host(), &mock);
  CHECK_STATUS(fixture.table().create_guest(1u, 0x1000u, request, 24u, 1u), Status::ok);
  CHECK(fixture.table().contains(1u));
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().create_guest(1u, 0x1000u, request, 24u, 1u), Status::conflict);
  CHECK_STATUS(fixture.table().create_guest(2u, 0x2000u, request, 24u, 1u), Status::invalid_parameter);
  CHECK(!fixture.table().contains(2u));
  CHECK_EQ(fixture.table().live(), 1u);
  CHECK(fixture.table().invariants_hold()); }

TEST(blob_table_create_host, keeps_ids_unique_and_leaves_nothing_behind_a_failure)
{ Fixture fixture;
  CHECK_STATUS(fixture.table().create_host(1u, 0x1000u), Status::ok);
  CHECK_STATUS(fixture.table().create_host(1u, 0x1000u), Status::conflict);
  fixture.host().fail_host_memory_at(1u);
  CHECK_STATUS(fixture.table().create_host(2u, 0x1000u), Status::no_memory);
  CHECK_EQ(fixture.table().live(), 1u);
  CHECK(fixture.table().invariants_hold()); }

TEST(blob_table_create_host, a_full_table_is_out_of_space)
{ MockHost host;
  BlobTable table(host);
  GpuRequestBuilder bytes(0u);
  host.add_guest_ram(ram, 0x1000u);
  bytes.u64(ram).u32(0x10u).u32(0u);
  MockRequest mock(bytes.bytes(), bytes.size());
  ControlRequest request(host, &mock);
  for (std::uint32_t id = 1u; id <= BlobTable::max_blobs; id++)
      if (table.create_guest(id, 0x10u, request, 24u, 1u) != Status::ok) break;
  CHECK_EQ(table.live(), BlobTable::max_blobs);
  CHECK_STATUS(table.create_host(9999u, 0x1000u), Status::no_space);
  CHECK_STATUS(table.create_guest(9999u, 0x10u, request, 24u, 1u), Status::no_space);
  table.release_all();
  CHECK_EQ(table.live(), 0u);
  CHECK_EQ(host.live_guest_mappings(), 0u); }

TEST(blob_table_map, keeps_mappings_inside_the_region_and_apart)
{ Fixture fixture;
  CHECK_STATUS(fixture.table().create_host(1u, 0x2000u), Status::ok);
  CHECK_STATUS(fixture.table().create_host(2u, 0x2000u), Status::ok);
  CHECK_STATUS(fixture.table().map(3u, 0u), Status::not_found);
  CHECK_STATUS(fixture.table().map(1u, 0xF000u), Status::invalid_parameter);
  CHECK_STATUS(fixture.table().map(1u, 0x2000u), Status::ok);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().map(2u, 0x3000u), Status::invalid_parameter);
  CHECK_STATUS(fixture.table().map(2u, 0x4000u), Status::ok);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().unmap(1u), Status::ok);
  CHECK_STATUS(fixture.table().unmap(1u), Status::invalid_state);
  CHECK_STATUS(fixture.table().unmap(3u), Status::not_found);
  CHECK_STATUS(fixture.table().map(1u, 0x3000u), Status::invalid_parameter);
  CHECK_STATUS(fixture.table().map(1u, 0u), Status::ok);
  CHECK(fixture.table().invariants_hold()); }

TEST(blob_table_release, frees_one_blob_or_all_of_them)
{ Fixture fixture;
  CHECK_STATUS(fixture.table().create_host(1u, 0x1000u), Status::ok);
  CHECK_STATUS(fixture.table().create_host(2u, 0x1000u), Status::ok);
  CHECK_STATUS(fixture.table().map(1u, 0u), Status::ok);
  CHECK_STATUS(fixture.table().release(1u), Status::ok);
  CHECK_STATUS(fixture.table().release(1u), Status::not_found);
  CHECK_EQ(fixture.host().live_placements(), 0u);
  CHECK(fixture.table().invariants_hold());
  fixture.table().release_all();
  CHECK(!fixture.table().contains(2u));
  CHECK_EQ(fixture.host().live_host_memory(), 0u); }

TEST(blob_table, is_main_loop_only)
{ Fixture fixture;
  struct Call {
      BlobTable *table;
      Status     status;
  } call = {&fixture.table(), Status::ok};
  void *thread = nullptr;
  CHECK_STATUS(fixture.host().start_thread(
                   [](void *context) {
                       auto *const c = static_cast<Call *>(context);
                       c->status = c->table->release(5u);
                   },
                   &call, thread),
               Status::ok);
  fixture.host().join_thread(thread);
  CHECK(fixture.host().claim(HostViolation::main_loop_required));
  CHECK_STATUS(call.status, Status::not_found); }

TEST(blob_table_invariants, a_duplicate_id_or_overlapping_mappings_are_detected)
{ Fixture duplicate;
  CHECK_STATUS(duplicate.table().create_host(1u, 0x1000u), Status::ok);
  CHECK_STATUS(duplicate.table().create_host(2u, 0x1000u), Status::ok);
  av::BlobTableProbe::place(duplicate.table(), 1u, 0x8000u);
  CHECK(!duplicate.table().invariants_hold());
  Fixture overlapping;
  CHECK_STATUS(overlapping.table().create_host(1u, 0x1000u), Status::ok);
  CHECK_STATUS(overlapping.table().map(1u, 0x2000u), Status::ok);
  av::BlobTableProbe::place(overlapping.table(), 2u, 0x2000u);
  CHECK(overlapping.host().claim(HostViolation::overlapping_placement));
  CHECK(!overlapping.table().invariants_hold());
  overlapping.table().release_all(); }
