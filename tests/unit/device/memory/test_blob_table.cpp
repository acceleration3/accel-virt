#include "core/support/status.hpp"
#include "device/control/control_request.hpp"
#include "device/memory/blob_resource.hpp"
#include "device/memory/blob_table.hpp"
#include "device/memory/blob_window.hpp"
#include "device/memory/guest_page_alias.hpp"
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
    /* A retiring mark on a slot no blob lives in. */
    static void mark_empty_slot_retiring(BlobTable &table)
    {
        for (std::uint32_t index = 0u; index < BlobTable::max_blobs; index++) {
            if (table.slots_[index].constructed()) continue;
            table.retiring_[index] = true;
            return;
        }
    }
    static void clear_marks(BlobTable &table)
    {
        for (bool &retiring : table.retiring_) retiring = false;
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
        CHECK_STATUS(table_.initialize(), Status::ok);
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
  CHECK_STATUS(table.initialize(), Status::ok);
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

TEST(blob_table_pin, keeps_a_blob_large_enough_alive_until_every_pin_is_let_go)
{ Fixture fixture;
  CHECK_STATUS(fixture.table().create_host(1u, 0x2000u), Status::ok);
  CHECK_STATUS(fixture.table().pin(2u, 0u), Status::not_found);
  CHECK_STATUS(fixture.table().pin(1u, 0x2001u), Status::invalid_parameter);
  CHECK_STATUS(fixture.table().release(1u), Status::ok);
  CHECK_STATUS(fixture.table().create_host(1u, 0x2000u), Status::ok);
  CHECK_STATUS(fixture.table().pin(1u, 0x2000u), Status::ok);
  CHECK_STATUS(fixture.table().pin(1u, 32u), Status::ok);
  CHECK_STATUS(fixture.table().release(1u), Status::invalid_state);
  fixture.table().unpin(1u);
  CHECK_STATUS(fixture.table().release(1u), Status::invalid_state);
  fixture.table().unpin(1u);
  fixture.table().unpin(7u);   /* unknown: nothing */
  CHECK_STATUS(fixture.table().release(1u), Status::ok); }

TEST(blob_table_window, is_handed_out_only_for_a_pinned_blob)
{ Fixture fixture;
  av::BlobWindow window;
  CHECK_STATUS(fixture.table().window(1u, window), Status::not_found);
  CHECK_STATUS(fixture.table().create_host(1u, 0x2000u), Status::ok);
  CHECK_STATUS(fixture.table().window(1u, window), Status::invalid_state);
  CHECK_EQ(window.length(), 0u);
  CHECK_STATUS(fixture.table().pin(1u, 0x2000u), Status::ok);
  CHECK_STATUS(fixture.table().window(1u, window), Status::ok);
  CHECK_EQ(window.length(), 0x2000u);
  fixture.table().unpin(1u); }

TEST(blob_table_unpin, a_pinned_blob_is_still_released_by_release_all)
{ Fixture fixture;
  CHECK_STATUS(fixture.table().create_host(1u, 0x1000u), Status::ok);
  CHECK_STATUS(fixture.table().pin(1u, 0x1000u), Status::ok);
  fixture.table().release_all();
  CHECK_EQ(fixture.table().live(), 0u);
  fixture.table().unpin(1u); }

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
                       static_cast<void>(c->table->pin(5u, 0u));
                       c->table->unpin(5u);
                       static_cast<void>(c->table->retire(5u));
                   },
                   &call, thread),
               Status::ok);
  fixture.host().join_thread(thread);
  for (int call_count = 0; call_count < 4; call_count++) CHECK(fixture.host().claim(HostViolation::main_loop_required));
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

TEST(blob_table_initialize, nothing_is_created_before_the_lock_exists_and_it_is_made_once)
{ MockHost host;
  host.add_guest_ram(ram, 0x1000u);
  GpuRequestBuilder bytes(0u);
  bytes.u64(ram).u32(0x1000u).u32(0u);
  MockRequest mock(bytes.bytes(), bytes.size());
  ControlRequest request(host, &mock);
  { BlobTable table(host);
    CHECK_STATUS(table.create_guest(1u, 0x1000u, request, 24u, 1u), Status::invalid_state);
    CHECK_STATUS(table.create_host(2u, 0x1000u), Status::invalid_state);
    const av::GuestPageAlias *alias = nullptr;
    av::BlobWindow            pages;
    CHECK_STATUS(table.hold_alias(1u, alias, pages), Status::not_found);
    table.release_alias(1u);
    table.release_all();
    CHECK_EQ(table.live(), 0u);
    CHECK(table.invariants_hold());
    host.fail_mutex_at(1u);
    CHECK_STATUS(table.initialize(), Status::no_memory);
    CHECK_EQ(host.live_mutexes(), 0u);
    CHECK_STATUS(table.initialize(), Status::ok);
    CHECK_STATUS(table.initialize(), Status::invalid_state);
    CHECK_EQ(host.live_mutexes(), 1u);
    CHECK_STATUS(table.create_guest(1u, 0x1000u, request, 24u, 1u), Status::ok);
    CHECK(table.invariants_hold()); }
  CHECK_EQ(host.live_mutexes(), 0u);
  CHECK_EQ(host.live_guest_mappings(), 0u); }

TEST(blob_table_hold_alias, shares_one_alias_of_a_guest_blob_and_keeps_the_blob_while_it_is_held)
{ Fixture fixture;
  GpuRequestBuilder bytes(0u);
  bytes.u64(ram + 0x3000u).u32(0x1000u).u32(0u);
  MockRequest mock(bytes.bytes(), bytes.size());
  ControlRequest request(fixture.host(), &mock);
  const av::GuestPageAlias *alias = nullptr;
  av::BlobWindow            pages;
  CHECK_STATUS(fixture.table().hold_alias(1u, alias, pages), Status::not_found);
  CHECK_STATUS(fixture.table().create_guest(1u, 0x1000u, request, 24u, 1u), Status::ok);
  CHECK_STATUS(fixture.table().create_host(2u, 0x1000u), Status::ok);
  CHECK_STATUS(fixture.table().hold_alias(2u, alias, pages), Status::invalid_parameter);
  CHECK(alias == nullptr);
  fixture.host().guest_ram(ram + 0x3008u)[0] = 0x5Bu;
  CHECK_STATUS(fixture.table().hold_alias(1u, alias, pages), Status::ok);
  CHECK_EQ(static_cast<std::uint8_t *>(alias->base())[8], 0x5Bu);
  CHECK_EQ(pages.length(), 0x1000u);
  const av::GuestPageAlias *again = nullptr;
  CHECK_STATUS(fixture.table().hold_alias(1u, again, pages), Status::ok);
  CHECK(again == alias);
  CHECK_EQ(fixture.host().live_ranges(), 1u);
  CHECK_STATUS(fixture.table().release(1u), Status::invalid_state);   /* a host API has its pages */
  fixture.table().release_alias(1u);
  CHECK_STATUS(fixture.table().release(1u), Status::invalid_state);
  CHECK(fixture.table().invariants_hold());
  fixture.table().release_alias(1u);
  fixture.table().release_alias(9u);   /* unknown: nothing */
  CHECK_EQ(fixture.host().live_ranges(), 0u);
  CHECK_STATUS(fixture.table().release(1u), Status::ok);
  CHECK(fixture.table().invariants_hold()); }

TEST(blob_table_hold_alias, reset_releases_an_aliased_blob_and_its_alias)
{ Fixture fixture;
  GpuRequestBuilder bytes(0u);
  bytes.u64(ram).u32(0x1000u).u32(0u);
  MockRequest mock(bytes.bytes(), bytes.size());
  ControlRequest request(fixture.host(), &mock);
  CHECK_STATUS(fixture.table().create_guest(1u, 0x1000u, request, 24u, 1u), Status::ok);
  const av::GuestPageAlias *alias = nullptr;
  av::BlobWindow            pages;
  CHECK_STATUS(fixture.table().hold_alias(1u, alias, pages), Status::ok);
  fixture.table().release_all();
  CHECK_EQ(fixture.table().live(), 0u);
  CHECK_EQ(fixture.host().live_ranges(), 0u);
  fixture.table().release_alias(1u); }

TEST(blob_table_hold_alias, a_worker_holds_aliases_while_the_main_loop_creates_and_releases)
{ Fixture fixture;
  GpuRequestBuilder bytes(0u);
  bytes.u64(ram).u32(0x1000u).u32(0u);
  MockRequest mock(bytes.bytes(), bytes.size());
  ControlRequest request(fixture.host(), &mock);
  CHECK_STATUS(fixture.table().create_guest(1u, 0x1000u, request, 24u, 1u), Status::ok);
  struct Call {
      BlobTable     *table;
      std::uint32_t  made;
  } call = {&fixture.table(), 0u};
  void *thread = nullptr;
  CHECK_STATUS(fixture.host().start_thread(
                   [](void *context) {
                       auto *const c = static_cast<Call *>(context);
                       for (int round = 0; round < 2000; round++) {
                           const av::GuestPageAlias *alias = nullptr;
                           av::BlobWindow            pages;
                           if (c->table->hold_alias(1u, alias, pages) != Status::ok) continue;
                           c->made += alias->length() == 0x1000u ? 1u : 0u;
                           c->table->release_alias(1u);
                       }
                   },
                   &call, thread),
               Status::ok);
  for (std::uint32_t id = 2u; id < 2002u; id++) {
      CHECK_STATUS(fixture.table().create_guest(id, 0x1000u, request, 24u, 1u), Status::ok);
      CHECK_STATUS(fixture.table().release(id), Status::ok);
  }
  fixture.host().join_thread(thread);
  CHECK_EQ(call.made, 2000u);
  CHECK_EQ(fixture.host().unclaimed_violations(), 0u);
  CHECK_EQ(fixture.host().live_ranges(), 0u); }

TEST(blob_table_invariants, a_blob_held_without_the_lock_is_detected)
{ MockHost host;
  host.set_hostmem_size(0x10000u);
  BlobTable table(host);
  CHECK(table.invariants_hold());
  av::BlobTableProbe::place(table, 1u, 0u);
  CHECK(!table.invariants_hold());
  CHECK_STATUS(table.initialize(), Status::ok);
  CHECK(table.invariants_hold());
  table.release_all(); }


TEST(blob_table_retire, an_unheld_blob_goes_at_once_and_an_unknown_one_is_not_found)
{ Fixture fixture;
  CHECK_STATUS(fixture.table().retire(1u), Status::not_found);
  CHECK_STATUS(fixture.table().create_host(1u, 0x1000u), Status::ok);
  CHECK_STATUS(fixture.table().map(1u, 0x2000u), Status::ok);
  CHECK_STATUS(fixture.table().create_host(2u, 0x1000u), Status::ok);
  CHECK_STATUS(fixture.table().retire(1u), Status::ok);
  CHECK(!fixture.table().contains(1u));
  CHECK(fixture.table().contains(2u));
  CHECK_EQ(fixture.table().live(), 1u);
  CHECK_EQ(fixture.host().live_placements(), 0u);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().retire(1u), Status::not_found);
  CHECK_STATUS(fixture.table().create_host(1u, 0x1000u), Status::ok);
  CHECK(fixture.table().invariants_hold()); }

TEST(blob_table_retire, a_pinned_blob_keeps_its_id_takes_no_new_pin_and_goes_with_the_last_unpin)
{ Fixture fixture;
  CHECK_STATUS(fixture.table().create_host(1u, 0x2000u), Status::ok);
  CHECK_STATUS(fixture.table().map(1u, 0u), Status::ok);
  CHECK_STATUS(fixture.table().pin(1u, 0x2000u), Status::ok);
  CHECK_STATUS(fixture.table().pin(1u, 0x1000u), Status::ok);
  CHECK_STATUS(fixture.table().retire(1u), Status::ok);
  CHECK(fixture.table().contains(1u));
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().pin(1u, 0u), Status::invalid_state);
  CHECK_STATUS(fixture.table().create_host(1u, 0x1000u), Status::conflict);
  CHECK_STATUS(fixture.table().release(1u), Status::invalid_state);
  CHECK_STATUS(fixture.table().retire(1u), Status::ok);   /* still held: still retiring */
  av::BlobWindow window;
  CHECK_STATUS(fixture.table().window(1u, window), Status::ok);   /* a holder's window stays good */
  CHECK_EQ(window.length(), 0x2000u);
  fixture.table().unpin(1u);
  CHECK(fixture.table().contains(1u));
  CHECK_EQ(fixture.host().live_placements(), 1u);
  CHECK(fixture.table().invariants_hold());
  fixture.table().unpin(1u);
  CHECK(!fixture.table().contains(1u));
  CHECK_EQ(fixture.table().live(), 0u);
  CHECK_EQ(fixture.host().live_placements(), 0u);
  CHECK_EQ(fixture.host().live_host_memory(), 0u);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().create_host(1u, 0x1000u), Status::ok);
  CHECK_STATUS(fixture.table().pin(1u, 0u), Status::ok);   /* the new blob of that id is not retiring */
  fixture.table().unpin(1u);
  CHECK(fixture.table().contains(1u));
  CHECK(fixture.table().invariants_hold()); }

TEST(blob_table_retire, an_unpinned_blob_that_is_not_retiring_stays)
{ Fixture fixture;
  CHECK_STATUS(fixture.table().create_host(1u, 0x1000u), Status::ok);
  CHECK_STATUS(fixture.table().create_host(2u, 0x1000u), Status::ok);
  CHECK_STATUS(fixture.table().pin(1u, 0u), Status::ok);
  CHECK_STATUS(fixture.table().pin(2u, 0u), Status::ok);
  CHECK_STATUS(fixture.table().retire(2u), Status::ok);
  fixture.table().unpin(1u);
  CHECK(fixture.table().contains(1u));
  CHECK(fixture.table().contains(2u));
  fixture.table().unpin(2u);
  CHECK(fixture.table().contains(1u));
  CHECK(!fixture.table().contains(2u));
  CHECK(fixture.table().invariants_hold()); }

TEST(blob_table_retire, an_aliased_blob_takes_no_new_hold_and_goes_once_the_main_loop_next_acts)
{ Fixture fixture;
  GpuRequestBuilder bytes(0u);
  bytes.u64(ram + 0x1000u).u32(0x1000u).u32(0u);
  MockRequest mock(bytes.bytes(), bytes.size());
  ControlRequest request(fixture.host(), &mock);
  CHECK_STATUS(fixture.table().create_guest(1u, 0x1000u, request, 24u, 1u), Status::ok);
  const av::GuestPageAlias *alias = nullptr;
  av::BlobWindow            pages;
  CHECK_STATUS(fixture.table().hold_alias(1u, alias, pages), Status::ok);
  CHECK_STATUS(fixture.table().retire(1u), Status::ok);
  CHECK(fixture.table().contains(1u));
  CHECK(fixture.table().invariants_hold());
  const av::GuestPageAlias *refused = alias;
  CHECK_STATUS(fixture.table().hold_alias(1u, refused, pages), Status::invalid_state);
  CHECK(refused == nullptr);
  CHECK_STATUS(fixture.table().create_guest(1u, 0x1000u, request, 24u, 1u), Status::conflict);
  struct Call {
      BlobTable *table;
  } call = {&fixture.table()};
  void *thread = nullptr;
  CHECK_STATUS(fixture.host().start_thread([](void *context) { static_cast<Call *>(context)->table->release_alias(1u); },
                                           &call, thread),
               Status::ok);
  fixture.host().join_thread(thread);
  CHECK_EQ(fixture.host().unclaimed_violations(), 0u);   /* the worker unmapped nothing */
  CHECK_EQ(fixture.host().live_ranges(), 0u);
  CHECK(fixture.table().contains(1u));
  CHECK_EQ(fixture.host().live_guest_mappings(), 1u);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().create_guest(1u, 0x1000u, request, 24u, 1u), Status::ok);
  CHECK_EQ(fixture.table().live(), 1u);
  CHECK_EQ(fixture.host().live_guest_mappings(), 1u);
  CHECK_STATUS(fixture.table().hold_alias(1u, alias, pages), Status::ok);   /* the new blob takes holds */
  fixture.table().release_alias(1u);
  CHECK(fixture.table().invariants_hold()); }

TEST(blob_table_retire, a_blob_let_go_by_a_worker_goes_with_the_next_create_host_retire_or_release)
{ Fixture fixture;
  GpuRequestBuilder bytes(0u);
  bytes.u64(ram).u32(0x1000u).u32(0u);
  MockRequest mock(bytes.bytes(), bytes.size());
  ControlRequest request(fixture.host(), &mock);
  const av::GuestPageAlias *alias = nullptr;
  av::BlobWindow            pages;
  CHECK_STATUS(fixture.table().create_guest(1u, 0x1000u, request, 24u, 1u), Status::ok);
  CHECK_STATUS(fixture.table().hold_alias(1u, alias, pages), Status::ok);
  CHECK_STATUS(fixture.table().retire(1u), Status::ok);
  fixture.table().release_alias(1u);
  CHECK(fixture.table().contains(1u));
  CHECK_STATUS(fixture.table().create_host(2u, 0x1000u), Status::ok);
  CHECK(!fixture.table().contains(1u));
  CHECK_EQ(fixture.host().live_guest_mappings(), 0u);
  CHECK(fixture.table().invariants_hold());

  CHECK_STATUS(fixture.table().create_guest(3u, 0x1000u, request, 24u, 1u), Status::ok);
  CHECK_STATUS(fixture.table().hold_alias(3u, alias, pages), Status::ok);
  CHECK_STATUS(fixture.table().retire(3u), Status::ok);
  fixture.table().release_alias(3u);
  CHECK_STATUS(fixture.table().retire(3u), Status::ok);
  CHECK(!fixture.table().contains(3u));
  CHECK_STATUS(fixture.table().retire(3u), Status::not_found);

  CHECK_STATUS(fixture.table().create_guest(4u, 0x1000u, request, 24u, 1u), Status::ok);
  CHECK_STATUS(fixture.table().hold_alias(4u, alias, pages), Status::ok);
  CHECK_STATUS(fixture.table().retire(4u), Status::ok);
  fixture.table().release_alias(4u);
  CHECK_STATUS(fixture.table().release(4u), Status::ok);
  CHECK(!fixture.table().contains(4u));
  CHECK_EQ(fixture.host().live_guest_mappings(), 0u);
  CHECK(fixture.table().invariants_hold()); }

TEST(blob_table_retire, a_blob_both_pinned_and_aliased_goes_only_when_both_are_let_go)
{ Fixture fixture;
  GpuRequestBuilder bytes(0u);
  bytes.u64(ram).u32(0x1000u).u32(0u);
  MockRequest mock(bytes.bytes(), bytes.size());
  ControlRequest request(fixture.host(), &mock);
  const av::GuestPageAlias *alias = nullptr;
  av::BlobWindow            pages;
  CHECK_STATUS(fixture.table().create_guest(1u, 0x1000u, request, 24u, 1u), Status::ok);
  CHECK_STATUS(fixture.table().pin(1u, 0u), Status::ok);
  CHECK_STATUS(fixture.table().hold_alias(1u, alias, pages), Status::ok);
  CHECK_STATUS(fixture.table().retire(1u), Status::ok);
  fixture.table().unpin(1u);
  CHECK(fixture.table().contains(1u));
  CHECK_EQ(fixture.host().live_ranges(), 1u);
  fixture.table().release_alias(1u);
  CHECK_STATUS(fixture.table().retire(1u), Status::ok);
  CHECK(!fixture.table().contains(1u));

  CHECK_STATUS(fixture.table().create_guest(2u, 0x1000u, request, 24u, 1u), Status::ok);
  CHECK_STATUS(fixture.table().pin(2u, 0u), Status::ok);
  CHECK_STATUS(fixture.table().hold_alias(2u, alias, pages), Status::ok);
  CHECK_STATUS(fixture.table().retire(2u), Status::ok);
  fixture.table().release_alias(2u);
  CHECK(fixture.table().contains(2u));
  fixture.table().unpin(2u);
  CHECK(!fixture.table().contains(2u));
  CHECK_EQ(fixture.host().live_guest_mappings(), 0u);
  CHECK(fixture.table().invariants_hold()); }

TEST(blob_table_retire, reset_releases_a_retiring_blob_and_forgets_it_was_retiring)
{ Fixture fixture;
  CHECK_STATUS(fixture.table().create_host(1u, 0x1000u), Status::ok);
  CHECK_STATUS(fixture.table().pin(1u, 0u), Status::ok);
  CHECK_STATUS(fixture.table().retire(1u), Status::ok);
  fixture.table().release_all();
  CHECK_EQ(fixture.table().live(), 0u);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().create_host(1u, 0x1000u), Status::ok);
  CHECK_STATUS(fixture.table().pin(1u, 0u), Status::ok);
  fixture.table().unpin(1u);
  CHECK(fixture.table().contains(1u)); }

TEST(blob_table_invariants, a_retiring_mark_on_an_empty_slot_is_detected)
{ Fixture fixture;
  CHECK_STATUS(fixture.table().create_host(1u, 0x1000u), Status::ok);
  av::BlobTableProbe::mark_empty_slot_retiring(fixture.table());
  CHECK(!fixture.table().invariants_hold());
  av::BlobTableProbe::clear_marks(fixture.table());
  CHECK(fixture.table().invariants_hold()); }
