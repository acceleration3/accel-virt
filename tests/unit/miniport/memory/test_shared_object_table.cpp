#include "miniport/memory/allocation.hpp"
#include "miniport/memory/aperture_segment.hpp"
#include "miniport/memory/memory_segments.hpp"
#include "miniport/memory/shared_object.hpp"
#include "miniport/memory/shared_object_table.hpp"
#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/shared_object_record.hpp"

#include <cstdint>
#include <cstring>

namespace av {
struct SharedObjectProbe {
    static void set_opens(SharedObject &object, std::uint32_t opens) { object.opens_ = opens; }
    static void forget_allocation(SharedObject &object) { object.allocation_ = nullptr; }
    static void set_size(SharedObject &object, std::uint64_t size) { object.size_ = size; }
    static void set_id(SharedObject &object, std::uint64_t id) { object.id_ = id; }
    static void set_kind(SharedObject &object, std::uint32_t kind)
    { object.kind_ = SharedObjectKind(kind); }
    static std::uint32_t ceiling() { return SharedObject::max_opens; }
};

struct SharedObjectTableProbe {
    static void set_next_id(SharedObjectTable &table, std::uint64_t id) { table.next_id_ = id; }
    static std::uint64_t next_id(const SharedObjectTable &table) { return table.next_id_; }
};

}

using av::Allocation;
using av::AllocationDescriptor;
using av::AllocationKind;
using av::MockPlatform;
using av::ScanoutTarget;
using av::SharedObject;
using av::SharedObjectKind;
using av::SharedObjectRecord;
using av::SharedObjectTable;
using av::SharedObjectTableProbe;
using av::StateWriter;
using av::Status;
using av::SurfaceLayout;
using av::Violation;

namespace {

constexpr std::uint64_t page = Allocation::page_bytes;

class Fixture {
public:
    Fixture()
        : aperture_(platform_), segments_(platform_, aperture_),
          table_(platform_)
    {
        CHECK_STATUS(aperture_.initialize(0x100000000), Status::ok);
        CHECK_STATUS(table_.initialize(), Status::ok);
    }

    MockPlatform      &platform() { return platform_; }
    SharedObjectTable &table() { return table_; }
    SharedObject      *sentinel() { return reinterpret_cast<SharedObject *>(&storage_[max_allocations - 1u]); }

    Allocation *allocate(std::uint64_t bytes)
    {
        if (count_ == max_allocations) return nullptr;
        descriptors_[count_] = AllocationDescriptor(AllocationKind::client, false,
                                                    SurfaceLayout(16u, 16u, 64u, 40u), bytes, ScanoutTarget());
        Allocation *const made = new_allocation(bytes);
        return made;
    }

    SharedObjectRecord asking(std::uint64_t bytes,
                              std::uint32_t kind = SharedObjectKind::memory) const
    { return SharedObjectRecord(0u, SharedObjectKind(kind), 0u, bytes); }

    void check()
    { if (!table_.invariants_hold())
          av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "invariants broken"); }

private:
    static constexpr std::uint32_t max_allocations = 8u;

    Allocation *new_allocation(std::uint64_t bytes)
    {
        Allocation *const made = reinterpret_cast<Allocation *>(&storage_[count_]);
        ::new (static_cast<void *>(made)) Allocation(platform_, segments_, descriptors_[count_], bytes);
        count_++;
        return made;
    }

    alignas(Allocation) unsigned char storage_[max_allocations][sizeof(Allocation)] = {};
    MockPlatform            platform_;
    av::ApertureSegment     aperture_;
    av::MemorySegments      segments_;
    AllocationDescriptor    descriptors_[max_allocations];
    SharedObjectTable       table_;
    std::uint32_t           count_ = 0u;
};

}

TEST(shared_object_table_initialize, mints_an_adapter_number_once_and_never_zero)
{ MockPlatform      platform;
  SharedObjectTable table(platform);
  CHECK(table.invariants_hold());
  CHECK_EQ(table.adapter(), 0u);
  CHECK_STATUS(table.initialize(), Status::ok);
  CHECK(table.adapter() != 0u);
  CHECK_STATUS(table.initialize(), Status::invalid_state);
  CHECK(table.invariants_hold()); }

TEST(shared_object_table_mint, refuses_before_the_adapter_has_a_number)
{ MockPlatform            platform;
  av::ApertureSegment     aperture(platform);
  av::MemorySegments      segments(platform, aperture);
  CHECK_STATUS(aperture.initialize(0x100000000), Status::ok);
  const AllocationDescriptor descriptor(AllocationKind::client, false, SurfaceLayout(16u, 16u, 64u, 40u),
                                        page, ScanoutTarget());
  Allocation        allocation(platform, segments, descriptor, page);
  SharedObjectTable table(platform);
  SharedObjectRecord record(0u, SharedObjectKind(SharedObjectKind::memory), 0u, page);
  CHECK_STATUS(table.mint(allocation, record), Status::invalid_state);
  CHECK(!record.assigned());
  CHECK_EQ(table.live(), 0u); }

TEST(shared_object_table_mint, writes_an_id_and_this_adapter_into_the_record)
{ Fixture rig;
  Allocation *const allocation = rig.allocate(2u * page);
  SharedObjectRecord record = rig.asking(2u * page);
  CHECK_STATUS(rig.table().mint(*allocation, record), Status::ok);
  rig.check();
  CHECK(record.assigned());
  CHECK_EQ(record.object_id(), 1u);
  CHECK_EQ(record.adapter_id(), rig.table().adapter());
  CHECK_EQ(record.size(), 2u * page);
  CHECK_EQ(rig.table().live(), 1u); }

TEST(shared_object_table_mint, gives_a_fresh_id_to_every_allocation_and_never_reuses_one)
{ Fixture rig;
  std::uint64_t ids[3] = {};
  for (std::uint32_t index = 0u; index < 3u; index++) {
      Allocation *const allocation = rig.allocate(page);
      SharedObjectRecord record = rig.asking(page);
      CHECK_STATUS(rig.table().mint(*allocation, record), Status::ok);
      rig.check();
      ids[index] = record.object_id();
      if (index > 0u) CHECK(ids[index] != ids[index - 1u]);
  }
  CHECK_EQ(rig.table().live(), 3u);
  Allocation *const second = rig.allocate(page);
  SharedObjectRecord record = rig.asking(page);
  CHECK_STATUS(rig.table().mint(*second, record), Status::ok);
  rig.check();
  CHECK(record.object_id() > ids[2]); }

TEST(shared_object_table_mint, refuses_a_record_that_already_names_one)
{ Fixture rig;
  Allocation *const allocation = rig.allocate(2u * page);
  SharedObjectRecord already(4u, SharedObjectKind(SharedObjectKind::memory), 9u, 2u * page);
  CHECK_STATUS(rig.table().mint(*allocation, already), Status::invalid_parameter);
  rig.check();
  CHECK_EQ(already.object_id(), 4u);
  CHECK_EQ(rig.table().live(), 0u); }

TEST(shared_object_table_mint, a_one_page_carrier_may_name_an_object_of_any_size)
{ Fixture rig;
  Allocation *const carrier = rig.allocate(page);
  SharedObjectRecord bigger = rig.asking(64u * page);
  CHECK_STATUS(rig.table().mint(*carrier, bigger), Status::ok);
  rig.check();
  CHECK(bigger.assigned());
  CHECK_EQ(rig.table().live(), 1u);
  SharedObject *found = nullptr;
  CHECK_STATUS(rig.table().open(bigger, found), Status::ok);
  CHECK(found != nullptr);
  CHECK_STATUS(rig.table().close(*found), Status::ok);
  SharedObjectRecord tampered(bigger.object_id(), SharedObjectKind(SharedObjectKind::memory),
                              bigger.adapter_id(), 2u * page);
  CHECK_STATUS(rig.table().open(tampered, found), Status::invalid_parameter);
  CHECK(found == nullptr);
  rig.check(); }

TEST(shared_object_table_mint, propagates_a_pool_that_cannot_grow)
{ Fixture rig;
  Allocation *const allocation = rig.allocate(page);
  rig.platform().fail_allocation_at(1u);
  SharedObjectRecord record = rig.asking(page);
  CHECK_STATUS(rig.table().mint(*allocation, record), Status::no_memory);
  rig.check();
  CHECK(!record.assigned());
  CHECK_EQ(rig.table().live(), 0u);
  CHECK_STATUS(rig.table().mint(*allocation, record), Status::ok);
  rig.check();
  CHECK_EQ(record.object_id(), 1u); }

TEST(shared_object_table_open, finds_what_a_creator_minted_and_counts_the_device_that_opened_it)
{ Fixture rig;
  Allocation *const allocation = rig.allocate(2u * page);
  SharedObjectRecord record = rig.asking(2u * page);
  CHECK_STATUS(rig.table().mint(*allocation, record), Status::ok);

  SharedObject *first = nullptr;
  CHECK_STATUS(rig.table().open(record, first), Status::ok);
  rig.check();
  CHECK(first != nullptr);
  CHECK_EQ(first->opens(), 1u);
  SharedObject *second = nullptr;
  CHECK_STATUS(rig.table().open(record, second), Status::ok);
  rig.check();
  CHECK(first == second);
  CHECK_EQ(first->opens(), 2u);
  CHECK_EQ(rig.table().live(), 1u);

  CHECK_STATUS(rig.table().close(*first), Status::ok);
  rig.check();
  CHECK_EQ(first->opens(), 1u);
  CHECK_STATUS(rig.table().close(*first), Status::ok);
  rig.check();
  CHECK_EQ(first->opens(), 0u);
  CHECK_STATUS(rig.table().close(*first), Status::invalid_parameter);
  rig.check(); }

TEST(shared_object_table_open, refuses_an_id_this_adapter_never_minted_and_holds_nothing)
{ Fixture rig;
  Allocation *const allocation = rig.allocate(page);
  SharedObjectRecord minted = rig.asking(page);
  CHECK_STATUS(rig.table().mint(*allocation, minted), Status::ok);

  const std::uint64_t forged[] = {minted.object_id() + 1u, 0xFFFFFFFFFFFFFFFFu, 1000u};
  for (const std::uint64_t id : forged) {
      SharedObjectRecord record(id, SharedObjectKind(SharedObjectKind::memory), rig.table().adapter(), page);
      SharedObject      *found = rig.sentinel();
      CHECK_STATUS(rig.table().open(record, found), Status::not_found);
      rig.check();
      CHECK(found == nullptr);
  } }

TEST(shared_object_table_open, refuses_a_record_from_another_adapter)
{ Fixture rig;
  Allocation *const allocation = rig.allocate(page);
  SharedObjectRecord minted = rig.asking(page);
  CHECK_STATUS(rig.table().mint(*allocation, minted), Status::ok);

  SharedObjectRecord elsewhere(minted.object_id(), minted.kind(), rig.table().adapter() ^ 0x10u, page);
  SharedObject      *found = nullptr;
  CHECK_STATUS(rig.table().open(elsewhere, found), Status::not_found);
  rig.check();
  CHECK(found == nullptr);
  SharedObject *ours = nullptr;
  CHECK_STATUS(rig.table().open(minted, ours), Status::ok);
  CHECK_EQ(ours->opens(), 1u);
  CHECK_STATUS(rig.table().close(*ours), Status::ok);
  rig.check(); }

TEST(shared_object_table_open, refuses_a_record_carrying_no_identity_at_all)
{ Fixture rig;
  Allocation *const allocation = rig.allocate(page);
  SharedObjectRecord minted = rig.asking(page);
  CHECK_STATUS(rig.table().mint(*allocation, minted), Status::ok);

  const SharedObjectRecord unassigned = rig.asking(page);
  SharedObject            *found = nullptr;
  CHECK_STATUS(rig.table().open(unassigned, found), Status::invalid_parameter);
  rig.check();
  CHECK(found == nullptr); }

TEST(shared_object_table_open, refuses_a_kind_or_a_size_that_disagrees_with_the_object)
{ Fixture rig;
  Allocation *const allocation = rig.allocate(2u * page);
  SharedObjectRecord minted = rig.asking(2u * page);
  CHECK_STATUS(rig.table().mint(*allocation, minted), Status::ok);

  const SharedObjectRecord wrong[] = {
      SharedObjectRecord(minted.object_id(), SharedObjectKind(SharedObjectKind::monitored_fence),
                         minted.adapter_id(), 0u),
      SharedObjectRecord(minted.object_id(), SharedObjectKind(SharedObjectKind::native_fence),
                         minted.adapter_id(), 0u),
      SharedObjectRecord(minted.object_id(), minted.kind(), minted.adapter_id(), page),
      SharedObjectRecord(minted.object_id(), minted.kind(), minted.adapter_id(), 4u * page),
  };
  for (const SharedObjectRecord &record : wrong) {
      SharedObject *found = nullptr;
      CHECK_STATUS(rig.table().open(record, found), Status::invalid_parameter);
      rig.check();
      CHECK(found == nullptr);
  }
  SharedObject *ours = nullptr;
  CHECK_STATUS(rig.table().open(minted, ours), Status::ok);
  CHECK_EQ(ours->opens(), 1u);
  CHECK_STATUS(rig.table().close(*ours), Status::ok);
  rig.check(); }

TEST(shared_object_table_close, refuses_an_object_this_table_does_not_hold)
{ Fixture rig;
  MockPlatform            other;
  av::ApertureSegment     aperture(other);
  av::MemorySegments      segments(other, aperture);
  CHECK_STATUS(aperture.initialize(0x100000000), Status::ok);
  const AllocationDescriptor descriptor(AllocationKind::client, false, SurfaceLayout(8u, 8u, 32u, 40u),
                                        page, ScanoutTarget());
  Allocation   elsewhere(other, segments, descriptor, page);
  SharedObject stranger(other, 1u, SharedObjectKind(SharedObjectKind::memory), page, elsewhere);
  CHECK_STATUS(rig.table().close(stranger), Status::invalid_parameter);
  rig.check(); }

TEST(shared_object_table_forget, takes_the_identity_with_the_allocation_and_leaves_the_rest)
{ Fixture rig;
  Allocation *const first = rig.allocate(page);
  Allocation *const second = rig.allocate(page);
  SharedObjectRecord one = rig.asking(page);
  SharedObjectRecord two = rig.asking(page);
  CHECK_STATUS(rig.table().mint(*first, one), Status::ok);
  CHECK_STATUS(rig.table().mint(*second, two), Status::ok);
  rig.check();
  CHECK_EQ(rig.table().live(), 2u);

  rig.table().forget(*first);
  rig.check();
  CHECK_EQ(rig.table().live(), 1u);
  SharedObject *found = nullptr;
  CHECK_STATUS(rig.table().open(one, found), Status::not_found);
  CHECK_STATUS(rig.table().open(two, found), Status::ok);
  CHECK_STATUS(rig.table().close(*found), Status::ok);
  rig.check();

  rig.table().forget(*first);
  rig.check();
  CHECK_EQ(rig.table().live(), 1u); }

TEST(shared_object_table_take_released, hands_over_each_forgotten_id_once)
{ Fixture rig;
  Allocation *allocations[3] = {};
  SharedObjectRecord records[3];
  for (std::uint32_t index = 0u; index < 3u; index++) {
      allocations[index] = rig.allocate(page);
      records[index] = rig.asking(page);
      CHECK_STATUS(rig.table().mint(*allocations[index], records[index]), Status::ok);
  }
  std::uint64_t ids[4] = {};
  CHECK_EQ(rig.table().take_released(ids, 4u), 0u);
  rig.table().forget(*allocations[2]);
  rig.table().forget(*allocations[0]);
  rig.table().forget(*allocations[0]);
  rig.check();
  CHECK_EQ(rig.table().live(), 1u);
  av::SharedObject *found = nullptr;
  CHECK_STATUS(rig.table().open(records[0], found), Status::not_found);
  CHECK_EQ(rig.table().take_released(ids, 1u), 1u);
  const std::uint64_t first = ids[0];
  CHECK(first == records[2].object_id() || first == records[0].object_id());
  rig.table().forget(*allocations[1]);
  CHECK_EQ(rig.table().take_released(ids, 4u), 2u);
  std::uint64_t sum = first + ids[0] + ids[1];
  CHECK_EQ(sum, records[0].object_id() + records[1].object_id() + records[2].object_id());
  CHECK(ids[0] != first && ids[1] != first && ids[0] != ids[1]);
  CHECK_EQ(rig.table().take_released(ids, 4u), 0u);
  CHECK_EQ(rig.table().take_released(nullptr, 4u), 0u);
  CHECK_EQ(rig.table().live(), 0u);
  rig.check(); }

TEST(shared_object_table_take_released, more_forgotten_than_any_fixed_queue_held_are_all_handed_over)
{ Fixture rig;
  constexpr std::uint32_t many = 300u;
  Allocation *const allocation = rig.allocate(page);
  for (std::uint32_t index = 0u; index < many; index++) {
      SharedObjectRecord record = rig.asking(page);
      CHECK_STATUS(rig.table().mint(*allocation, record), Status::ok);
      rig.table().forget(*allocation);
  }
  char        text[256] = {};
  StateWriter writer(text, sizeof text);
  rig.table().report(writer);
  CHECK(std::strstr(writer.dump(), "live=0 released=300") != nullptr);
  std::uint64_t ids[64] = {};
  std::uint64_t handed = 0u, sum = 0u;
  for (std::uint32_t taken = rig.table().take_released(ids, 64u); taken != 0u;
       taken = rig.table().take_released(ids, 64u))
      for (std::uint32_t index = 0u; index < taken; index++, handed++) sum += ids[index];
  CHECK_EQ(handed, many);
  CHECK_EQ(sum, std::uint64_t{many} * (many + 1u) / 2u);
  rig.check(); }

TEST(shared_object_table_invariants, refuse_a_forgotten_object_still_held)
{ Fixture rig;
  Allocation *const allocation = rig.allocate(page);
  SharedObjectRecord record = rig.asking(page);
  CHECK_STATUS(rig.table().mint(*allocation, record), Status::ok);
  av::SharedObject *found = nullptr;
  CHECK_STATUS(rig.table().open(record, found), Status::ok);
  av::SharedObjectProbe::forget_allocation(*found);
  CHECK(!rig.table().invariants_hold());
  av::SharedObjectProbe::set_opens(*found, 0u);
  CHECK(rig.table().invariants_hold()); }

TEST(shared_object_table_open, propagates_an_object_at_its_ceiling)
{ Fixture rig;
  Allocation *const allocation = rig.allocate(page);
  SharedObjectRecord record = rig.asking(page);
  CHECK_STATUS(rig.table().mint(*allocation, record), Status::ok);
  SharedObject *found = nullptr;
  CHECK_STATUS(rig.table().open(record, found), Status::ok);
  av::SharedObjectProbe::set_opens(*found, 0xFFFFFFFFu);
  SharedObject *again = rig.sentinel();
  CHECK_STATUS(rig.table().open(record, again), Status::no_space);
  rig.check();
  CHECK(again == nullptr);
  av::SharedObjectProbe::set_opens(*found, 0u); }

TEST(shared_object_table, is_apc_at_most_and_the_mock_says_so)
{ Fixture rig;
  Allocation *const allocation = rig.allocate(page);
  SharedObjectRecord record = rig.asking(page);
  CHECK_STATUS(rig.table().mint(*allocation, record), Status::ok);
  rig.platform().set_irql(av::Irql::dispatch);
  SharedObject *found = nullptr;
  static_cast<void>(rig.table().open(record, found));
  rig.platform().set_irql(av::Irql::passive);
  for (int refusal = 0; refusal < 4; refusal++) CHECK(rig.platform().claim(Violation::irql_ceiling));
  CHECK(rig.platform().claim(Violation::blocking_at_dispatch)); }

TEST(shared_object_table_report, names_the_adapter_and_every_object_it_holds)
{ Fixture rig;
  Allocation *const allocation = rig.allocate(3u * page);
  SharedObjectRecord record = rig.asking(0u, SharedObjectKind::native_fence);
  CHECK_STATUS(rig.table().mint(*allocation, record), Status::ok);
  SharedObject *found = nullptr;
  CHECK_STATUS(rig.table().open(record, found), Status::ok);

  char        text[1024] = {};
  StateWriter writer(text, sizeof text);
  rig.table().report(writer);
  CHECK(std::strstr(writer.dump(), "shared_objects") != nullptr);
  CHECK(std::strstr(writer.dump(), "busy=false") != nullptr);
  CHECK(std::strstr(writer.dump(), "live=1") != nullptr);
  CHECK(std::strstr(writer.dump(), "kind=3") != nullptr);
  CHECK(std::strstr(writer.dump(), "size=0") != nullptr);
  CHECK(std::strstr(writer.dump(), "opens=1") != nullptr);
  CHECK_STATUS(rig.table().close(*found), Status::ok);
  rig.check(); }

TEST(shared_object_table_report, says_busy_and_nothing_more_while_another_thread_holds_it)
{ Fixture rig;
  rig.platform().hold_elsewhere(av::LockRank::shared_objects);
  char        text[512] = {};
  StateWriter writer(text, sizeof text);
  rig.table().report(writer);
  CHECK(std::strstr(writer.dump(), "busy=true") != nullptr);
  CHECK(std::strstr(writer.dump(), "live=") == nullptr);
  CHECK(std::strstr(writer.dump(), "object/") == nullptr); }

TEST(shared_object_table_invariants, refuse_an_object_whose_id_was_never_minted)
{ Fixture rig;
  Allocation *const allocation = rig.allocate(page);
  SharedObjectRecord record = rig.asking(page);
  CHECK_STATUS(rig.table().mint(*allocation, record), Status::ok);
  CHECK(rig.table().invariants_hold());
  SharedObjectTableProbe::set_next_id(rig.table(), record.object_id());
  CHECK(!rig.table().invariants_hold());
  SharedObjectTableProbe::set_next_id(rig.table(), record.object_id() + 1u);
  CHECK(rig.table().invariants_hold());

  SharedObject *found = nullptr;
  CHECK_STATUS(rig.table().open(record, found), Status::ok);
  av::SharedObjectProbe::set_id(*found, 0u);
  CHECK(!rig.table().invariants_hold());
  av::SharedObjectProbe::set_id(*found, record.object_id());
  CHECK_STATUS(rig.table().close(*found), Status::ok); }

TEST(shared_object_table_invariants, refuse_two_objects_that_name_one_id)
{ Fixture rig;
  Allocation *const first = rig.allocate(page);
  Allocation *const second = rig.allocate(page);
  SharedObjectRecord one = rig.asking(page);
  SharedObjectRecord two = rig.asking(page);
  CHECK_STATUS(rig.table().mint(*first, one), Status::ok);
  CHECK_STATUS(rig.table().mint(*second, two), Status::ok);
  CHECK(rig.table().invariants_hold());

  SharedObject *found = nullptr;
  CHECK_STATUS(rig.table().open(two, found), Status::ok);
  av::SharedObjectProbe::set_id(*found, one.object_id());
  CHECK(!rig.table().invariants_hold());
  av::SharedObjectProbe::set_id(*found, two.object_id());
  CHECK_STATUS(rig.table().close(*found), Status::ok);
  CHECK(rig.table().invariants_hold()); }

TEST(shared_object_table_open_minted, opens_by_the_allocations_own_id_and_answers_the_record)
{ Fixture fixture;
  Allocation *const carrier = fixture.allocate(4096u);
  CHECK(carrier != nullptr);
  SharedObjectRecord asking = fixture.asking(8192u);
  CHECK_STATUS(fixture.table().mint(*carrier, asking), Status::ok);
  CHECK(asking.assigned());

  SharedObject      *object = nullptr;
  SharedObjectRecord minted;
  CHECK_STATUS(fixture.table().open_minted(asking.object_id(), object, minted), Status::ok);
  CHECK(object != nullptr);
  CHECK(minted.assigned());
  CHECK_EQ(minted.object_id(), asking.object_id());
  CHECK_EQ(minted.adapter_id(), fixture.table().adapter());
  CHECK_EQ(minted.size(), 8192u);
  CHECK(minted.kind().same_as(asking.kind()));
  CHECK_STATUS(fixture.table().close(*object), Status::ok);
  CHECK(fixture.table().invariants_hold());

  SharedObject *nothing = nullptr;
  CHECK_STATUS(fixture.table().open_minted(asking.object_id() + 0x1000u, nothing, minted), Status::not_found);
  CHECK(nothing == nullptr);
  CHECK_STATUS(fixture.table().open_minted(0u, nothing, minted), Status::invalid_parameter);
  CHECK(nothing == nullptr);
  SharedObject *held = nullptr;
  CHECK_STATUS(fixture.table().open_minted(asking.object_id(), held, minted), Status::ok);
  av::SharedObjectProbe::set_opens(*held, 0xFFFFFFFFu);
  SharedObject *past = fixture.sentinel();
  CHECK_STATUS(fixture.table().open_minted(asking.object_id(), past, minted), Status::no_space);
  CHECK(past == nullptr);
  av::SharedObjectProbe::set_opens(*held, 1u);
  CHECK_STATUS(fixture.table().close(*held), Status::ok);
  CHECK(fixture.table().invariants_hold()); }
