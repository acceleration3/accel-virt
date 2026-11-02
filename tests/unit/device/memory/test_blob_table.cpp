#include "core/support/status.hpp"
#include "device/control/control_request.hpp"
#include "device/memory/blob_resource.hpp"
#include "device/memory/blob_table.hpp"
#include "device/memory/blob_window.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/gpu_request_builder.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"

#include <cstdint>

using av::BlobTable;
using av::BlobWindow;
using av::ControlRequest;
using av::GpuRequestBuilder;
using av::HostViolation;
using av::MockHost;
using av::MockRequest;
using av::Status;

namespace av {

struct BlobTableProbe {
    static void place(BlobTable &table, std::uint32_t id)
    {
        for (ObjectStorage<BlobResource> &slot : table.slots_)
            if (slot.construct(table.host_, id) == Status::ok) return;
    }
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

}

namespace {

constexpr std::uint64_t ram = 0x100000u;
constexpr std::size_t   entries_at = 24u;

GpuRequestBuilder entries(std::uint32_t pages)
{
    GpuRequestBuilder bytes(0u);
    for (std::uint32_t page = 0u; page < pages; page++) bytes.u64(ram + page * 0x1000u).u32(0x1000u).u32(0u);
    return bytes;
}

class Fixture {
public:
    explicit Fixture(std::uint32_t pages = 1u)
        : bytes_(entries(pages)), mock_(bytes_.bytes(), bytes_.size()), request_(host_, &mock_), table_(host_)
    {
        host_.add_guest_ram(ram, 0x10000u);
        CHECK_STATUS(table_.initialize(), Status::ok);
    }

    MockHost  &host() { return host_; }
    BlobTable &table() { return table_; }
    Status create(std::uint32_t id, std::uint64_t size = 0x1000u, std::uint32_t entries = 1u)
    {
        return table_.create_guest(id, size, request_, entries_at, entries);
    }

private:
    MockHost          host_;
    GpuRequestBuilder bytes_;
    MockRequest       mock_;
    ControlRequest    request_;
    BlobTable         table_;
};

}

TEST(blob_table_create_guest, keeps_ids_unique_and_leaves_nothing_behind_a_refusal)
{ Fixture fixture;
  CHECK_STATUS(fixture.create(1u), Status::ok);
  CHECK(fixture.table().contains(1u));
  CHECK(fixture.table().invariants_hold());
  CHECK_EQ(fixture.host().live_guest_mappings(), 1u);
  CHECK_STATUS(fixture.create(1u), Status::conflict);
  CHECK_STATUS(fixture.create(2u, 0x2000u), Status::invalid_parameter);
  CHECK(!fixture.table().contains(2u));
  CHECK_EQ(fixture.table().live(), 1u);
  CHECK_EQ(fixture.host().live_guest_mappings(), 1u);
  CHECK(fixture.table().invariants_hold()); }

TEST(blob_table_create_guest, a_full_table_is_out_of_space)
{ Fixture fixture;
  for (std::uint32_t id = 1u; id <= BlobTable::max_blobs; id++)
      if (fixture.create(id) != Status::ok) break;
  CHECK_EQ(fixture.table().live(), BlobTable::max_blobs);
  CHECK_STATUS(fixture.create(9999u), Status::no_space);
  CHECK(fixture.table().invariants_hold());
  fixture.table().release_all();
  CHECK_EQ(fixture.table().live(), 0u);
  CHECK_EQ(fixture.host().live_guest_mappings(), 0u); }

TEST(blob_table_size, answers_what_the_pages_hold)
{ Fixture fixture(2u);
  std::uint64_t size = 7u;
  CHECK_STATUS(fixture.table().size(1u, size), Status::not_found);
  CHECK_EQ(size, 7u);
  CHECK_STATUS(fixture.create(1u, 0x1800u, 2u), Status::ok);
  CHECK_STATUS(fixture.table().size(1u, size), Status::ok);
  CHECK_EQ(size, 0x2000u); }

TEST(blob_table_release, frees_one_unpinned_blob_or_all_of_them)
{ Fixture fixture;
  CHECK_STATUS(fixture.table().release(1u), Status::not_found);
  CHECK_STATUS(fixture.create(1u), Status::ok);
  CHECK_STATUS(fixture.create(2u), Status::ok);
  CHECK_STATUS(fixture.table().release(1u), Status::ok);
  CHECK_STATUS(fixture.table().release(1u), Status::not_found);
  CHECK(!fixture.table().contains(1u));
  CHECK(fixture.table().contains(2u));
  CHECK_EQ(fixture.host().live_guest_mappings(), 1u);
  CHECK(fixture.table().invariants_hold());
  fixture.table().release_all();
  CHECK(!fixture.table().contains(2u));
  CHECK_EQ(fixture.host().live_guest_mappings(), 0u);
  CHECK(fixture.table().invariants_hold()); }

TEST(blob_table_pin, keeps_a_blob_large_enough_alive_until_every_pin_is_let_go)
{ Fixture fixture(2u);
  CHECK_STATUS(fixture.create(1u, 0x2000u, 2u), Status::ok);
  CHECK_STATUS(fixture.table().pin(2u, 0u), Status::not_found);
  CHECK_STATUS(fixture.table().pin(1u, 0x2001u), Status::invalid_parameter);
  CHECK_STATUS(fixture.table().release(1u), Status::ok);
  CHECK_STATUS(fixture.create(1u, 0x2000u, 2u), Status::ok);
  CHECK_STATUS(fixture.table().pin(1u, 0x2000u), Status::ok);
  CHECK_STATUS(fixture.table().pin(1u, 32u), Status::ok);
  CHECK_STATUS(fixture.table().release(1u), Status::invalid_state);
  fixture.table().unpin(1u);
  CHECK_STATUS(fixture.table().release(1u), Status::invalid_state);
  fixture.table().unpin(1u);
  fixture.table().unpin(7u);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().release(1u), Status::ok); }

TEST(blob_table_unpin, a_pin_never_taken_leaves_the_blob_releasable)
{ Fixture fixture;
  CHECK_STATUS(fixture.create(1u), Status::ok);
  fixture.table().unpin(1u);
  CHECK_STATUS(fixture.table().release(1u), Status::ok); }

TEST(blob_table_unpin, a_pinned_blob_is_still_released_by_release_all)
{ Fixture fixture;
  CHECK_STATUS(fixture.create(1u), Status::ok);
  CHECK_STATUS(fixture.table().pin(1u, 0x1000u), Status::ok);
  fixture.table().release_all();
  CHECK_EQ(fixture.table().live(), 0u);
  CHECK_EQ(fixture.host().live_guest_mappings(), 0u);
  fixture.table().unpin(1u);
  CHECK(fixture.table().invariants_hold()); }

TEST(blob_table_window, is_handed_out_only_for_a_pinned_blob)
{ Fixture fixture(2u);
  BlobWindow window;
  CHECK_STATUS(fixture.table().window(1u, window), Status::not_found);
  CHECK_STATUS(fixture.create(1u, 0x2000u, 2u), Status::ok);
  CHECK_STATUS(fixture.table().window(1u, window), Status::invalid_state);
  CHECK_EQ(window.length(), 0u);
  CHECK_STATUS(fixture.table().pin(1u, 0x2000u), Status::ok);
  CHECK_STATUS(fixture.table().window(1u, window), Status::ok);
  CHECK_EQ(window.length(), 0x2000u);
  fixture.table().unpin(1u); }

TEST(blob_table_backing, answers_any_live_blob_s_pages)
{ Fixture fixture;
  BlobWindow pages;
  CHECK_STATUS(fixture.table().backing(1u, pages), Status::not_found);
  CHECK_EQ(pages.length(), 0u);
  CHECK_STATUS(fixture.create(1u), Status::ok);
  CHECK_STATUS(fixture.table().backing(1u, pages), Status::ok);
  CHECK_EQ(pages.length(), 0x1000u);
  const std::uint8_t byte = 0x3Cu;
  CHECK_STATUS(pages.write(9u, &byte, 1u), Status::ok);
  CHECK_EQ(fixture.host().guest_ram(ram + 9u)[0], 0x3Cu); }

TEST(blob_table_view, is_one_range_over_a_pinned_blob_kept_until_it_goes)
{ Fixture fixture(2u);
  const std::uint8_t *base = &fixture.host().guest_ram(ram)[0];
  CHECK_STATUS(fixture.table().view(1u, base), Status::not_found);
  CHECK(base == nullptr);
  CHECK_STATUS(fixture.create(1u, 0x2000u, 2u), Status::ok);
  CHECK_STATUS(fixture.table().view(1u, base), Status::invalid_state);
  CHECK(base == nullptr);
  CHECK_STATUS(fixture.table().pin(1u, 0u), Status::ok);
  fixture.host().guest_ram(ram + 0x1004u)[0] = 0x4Du;
  CHECK_STATUS(fixture.table().view(1u, base), Status::ok);
  CHECK(base != nullptr);
  CHECK_EQ(base[0x1004], 0x4Du);
  const std::uint8_t *again = nullptr;
  CHECK_STATUS(fixture.table().view(1u, again), Status::ok);
  CHECK(again == base);
  CHECK_EQ(fixture.host().live_ranges(), 1u);
  fixture.table().unpin(1u);
  CHECK_STATUS(fixture.table().release(1u), Status::ok);
  CHECK_EQ(fixture.host().live_ranges(), 0u); }

TEST(blob_table_view, the_host_refusing_the_range_is_answered)
{ Fixture fixture;
  CHECK_STATUS(fixture.create(1u), Status::ok);
  CHECK_STATUS(fixture.table().pin(1u, 0u), Status::ok);
  fixture.host().fail_reserve_at(1u);
  const std::uint8_t *base = nullptr;
  CHECK(fixture.table().view(1u, base) != Status::ok);
  CHECK(base == nullptr);
  CHECK_EQ(fixture.host().live_ranges(), 0u);
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
                       BlobWindow window;
                       static_cast<void>(c->table->window(5u, window));
                       static_cast<void>(c->table->backing(5u, window));
                       const std::uint8_t *base = nullptr;
                       static_cast<void>(c->table->view(5u, base));
                       static_cast<void>(c->table->contains(5u));
                       std::uint64_t size = 0u;
                       static_cast<void>(c->table->size(5u, size));
                   },
                   &call, thread),
               Status::ok);
  fixture.host().join_thread(thread);
  for (int call_count = 0; call_count < 9; call_count++)
      CHECK(fixture.host().claim(HostViolation::main_loop_required));
  CHECK_STATUS(call.status, Status::not_found); }

TEST(blob_table_invariants, a_duplicate_id_is_detected)
{ Fixture fixture;
  CHECK_STATUS(fixture.create(1u), Status::ok);
  CHECK_STATUS(fixture.create(2u), Status::ok);
  CHECK(fixture.table().invariants_hold());
  av::BlobTableProbe::place(fixture.table(), 1u);
  CHECK(!fixture.table().invariants_hold()); }

TEST(blob_table_invariants, a_blob_without_the_lock_is_detected)
{ MockHost host;
  BlobTable table(host);
  CHECK(table.invariants_hold());
  av::BlobTableProbe::place(table, 1u);
  CHECK(!table.invariants_hold());
  CHECK_STATUS(table.initialize(), Status::ok);
  CHECK(table.invariants_hold());
  table.release_all(); }

TEST(blob_table_invariants, a_retiring_mark_on_an_empty_slot_is_detected)
{ Fixture fixture;
  CHECK_STATUS(fixture.create(1u), Status::ok);
  av::BlobTableProbe::mark_empty_slot_retiring(fixture.table());
  CHECK(!fixture.table().invariants_hold());
  av::BlobTableProbe::clear_marks(fixture.table());
  CHECK(fixture.table().invariants_hold()); }

TEST(blob_table_initialize, nothing_is_created_before_the_lock_exists_and_it_is_made_once)
{ MockHost host;
  host.add_guest_ram(ram, 0x1000u);
  GpuRequestBuilder bytes(0u);
  bytes.u64(ram).u32(0x1000u).u32(0u);
  MockRequest mock(bytes.bytes(), bytes.size());
  ControlRequest request(host, &mock);
  { BlobTable table(host);
    CHECK_STATUS(table.create_guest(1u, 0x1000u, request, entries_at, 1u), Status::invalid_state);
    table.release_all();
    CHECK_EQ(table.live(), 0u);
    CHECK(table.invariants_hold());
    host.fail_mutex_at(1u);
    CHECK_STATUS(table.initialize(), Status::no_memory);
    CHECK_EQ(host.live_mutexes(), 0u);
    CHECK_STATUS(table.initialize(), Status::ok);
    CHECK_STATUS(table.initialize(), Status::invalid_state);
    CHECK_EQ(host.live_mutexes(), 1u);
    CHECK_STATUS(table.create_guest(1u, 0x1000u, request, entries_at, 1u), Status::ok);
    CHECK(table.invariants_hold()); }
  CHECK_EQ(host.live_mutexes(), 0u);
  CHECK_EQ(host.live_guest_mappings(), 0u); }

TEST(blob_table_retire, an_unpinned_blob_goes_at_once_and_an_unknown_one_is_not_found)
{ Fixture fixture;
  CHECK_STATUS(fixture.table().retire(1u), Status::not_found);
  CHECK_STATUS(fixture.create(1u), Status::ok);
  CHECK_STATUS(fixture.create(2u), Status::ok);
  CHECK_STATUS(fixture.table().retire(1u), Status::ok);
  CHECK(!fixture.table().contains(1u));
  CHECK(fixture.table().contains(2u));
  CHECK_EQ(fixture.table().live(), 1u);
  CHECK_EQ(fixture.host().live_guest_mappings(), 1u);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().retire(1u), Status::not_found);
  CHECK_STATUS(fixture.create(1u), Status::ok);
  CHECK(fixture.table().invariants_hold()); }

TEST(blob_table_retire, a_pinned_blob_keeps_its_id_takes_no_new_pin_and_goes_with_the_last_unpin)
{ Fixture fixture;
  CHECK_STATUS(fixture.create(1u), Status::ok);
  CHECK_STATUS(fixture.table().pin(1u, 0x1000u), Status::ok);
  CHECK_STATUS(fixture.table().pin(1u, 0u), Status::ok);
  CHECK_STATUS(fixture.table().retire(1u), Status::ok);
  CHECK(fixture.table().contains(1u));
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.table().pin(1u, 0u), Status::invalid_state);
  CHECK_STATUS(fixture.create(1u), Status::conflict);
  CHECK_STATUS(fixture.table().release(1u), Status::invalid_state);
  CHECK_STATUS(fixture.table().retire(1u), Status::ok);
  BlobWindow window;
  CHECK_STATUS(fixture.table().window(1u, window), Status::ok);
  CHECK_EQ(window.length(), 0x1000u);
  fixture.table().unpin(1u);
  CHECK(fixture.table().contains(1u));
  CHECK_EQ(fixture.host().live_guest_mappings(), 1u);
  CHECK(fixture.table().invariants_hold());
  fixture.table().unpin(1u);
  CHECK(!fixture.table().contains(1u));
  CHECK_EQ(fixture.table().live(), 0u);
  CHECK_EQ(fixture.host().live_guest_mappings(), 0u);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.create(1u), Status::ok);
  CHECK_STATUS(fixture.table().pin(1u, 0u), Status::ok);
  fixture.table().unpin(1u);
  CHECK(fixture.table().contains(1u));
  CHECK(fixture.table().invariants_hold()); }

TEST(blob_table_retire, an_unpinned_blob_that_is_not_retiring_stays)
{ Fixture fixture;
  CHECK_STATUS(fixture.create(1u), Status::ok);
  CHECK_STATUS(fixture.create(2u), Status::ok);
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

TEST(blob_table_retire, reset_releases_a_retiring_blob_and_forgets_it_was_retiring)
{ Fixture fixture;
  CHECK_STATUS(fixture.create(1u), Status::ok);
  CHECK_STATUS(fixture.table().pin(1u, 0u), Status::ok);
  CHECK_STATUS(fixture.table().retire(1u), Status::ok);
  fixture.table().release_all();
  CHECK_EQ(fixture.table().live(), 0u);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.create(1u), Status::ok);
  CHECK_STATUS(fixture.table().pin(1u, 0u), Status::ok);
  fixture.table().unpin(1u);
  CHECK(fixture.table().contains(1u)); }

TEST(blob_table_retire, a_released_blob_s_slot_is_not_left_retiring)
{ Fixture fixture;
  CHECK_STATUS(fixture.create(1u), Status::ok);
  CHECK_STATUS(fixture.create(2u), Status::ok);
  CHECK_STATUS(fixture.table().pin(1u, 0u), Status::ok);
  CHECK_STATUS(fixture.table().retire(1u), Status::ok);
  CHECK_STATUS(fixture.table().release(2u), Status::ok);
  CHECK(fixture.table().invariants_hold());
  fixture.table().unpin(1u);
  CHECK_EQ(fixture.table().live(), 0u);
  CHECK(fixture.table().invariants_hold()); }
