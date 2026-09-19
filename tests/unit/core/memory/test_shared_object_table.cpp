/* The adapter's shared objects: where an identity is minted, and where every
 * claim to one is judged.
 *
 * The table believes nothing it did not write. Everything a second process can
 * put in front of it - an id nobody minted, an id from another adapter, a kind
 * or a size that disagrees with the object, a record with no id at all - is
 * refused here, with nothing held and nothing on the host touched. That is
 * US1's fourth scenario, and it is the only defence the kernel has: dxgkrnl
 * copies private data without reading it. */
#include "core/memory/allocation.hpp"
#include "core/memory/aperture_segment.hpp"
#include "core/memory/host_visible_segment.hpp"
#include "core/memory/memory_segments.hpp"
#include "core/memory/shared_object.hpp"
#include "core/memory/shared_object_table.hpp"
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

/* The same probe the object's own test defines, so a table test can put an
 * object into a state no table ever leaves one in. */
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

}  // namespace av

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

/* An adapter's table and the allocations it names. */
class Fixture {
public:
    Fixture()
        : aperture_(platform_), host_visible_(platform_), segments_(platform_, aperture_, host_visible_),
          table_(platform_)
    {
        CHECK_STATUS(aperture_.initialize(0x100000000), Status::ok);
        CHECK_STATUS(table_.initialize(), Status::ok);
    }

    MockPlatform      &platform() { return platform_; }
    SharedObjectTable &table() { return table_; }
    /* A non-null value a refusal has to clear, so "answered nothing" is
     * something a test can tell from "was never written". */
    SharedObject      *sentinel() { return reinterpret_cast<SharedObject *>(&storage_[max_allocations - 1u]); }

    /* An allocation of `bytes`, which the caller keeps for as long as it names
     * anything: a table forgets an object only when told to. */
    Allocation *allocate(std::uint64_t bytes)
    {
        if (count_ == max_allocations) return nullptr;
        descriptors_[count_] = AllocationDescriptor(AllocationKind::client, false,
                                                    SurfaceLayout(16u, 16u, 64u, 40u), bytes, ScanoutTarget());
        Allocation *const made = new_allocation(bytes);
        return made;
    }

    /* What a creator hands the miniport: a kind, a size, and no identity. */
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
        /* Storage the fixture owns, so an allocation outlives whatever names
         * it without a pool of its own. */
        Allocation *const made = reinterpret_cast<Allocation *>(&storage_[count_]);
        ::new (static_cast<void *>(made)) Allocation(platform_, segments_, descriptors_[count_], bytes);
        count_++;
        return made;
    }

    alignas(Allocation) unsigned char storage_[max_allocations][sizeof(Allocation)] = {};
    MockPlatform            platform_;
    av::ApertureSegment     aperture_;
    av::HostVisibleSegment  host_visible_;
    av::MemorySegments      segments_;
    AllocationDescriptor    descriptors_[max_allocations];
    SharedObjectTable       table_;
    std::uint32_t           count_ = 0u;
};

}  // namespace

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
  av::HostVisibleSegment  host_visible(platform);
  av::MemorySegments      segments(platform, aperture, host_visible);
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
  /* The middle one goes; its id is never handed out again, so a process that
   * kept the old record finds nothing rather than someone else's object. */
  Allocation *const second = rig.allocate(page);
  SharedObjectRecord record = rig.asking(page);
  CHECK_STATUS(rig.table().mint(*second, record), Status::ok);
  rig.check();
  CHECK(record.object_id() > ids[2]); }

TEST(shared_object_table_mint, refuses_a_record_that_already_names_one_and_one_whose_size_is_not_the_allocations)
{ Fixture rig;
  Allocation *const allocation = rig.allocate(2u * page);
  SharedObjectRecord already(4u, SharedObjectKind(SharedObjectKind::memory), 9u, 2u * page);
  CHECK_STATUS(rig.table().mint(*allocation, already), Status::invalid_parameter);
  rig.check();
  CHECK_EQ(already.object_id(), 4u);
  CHECK_EQ(rig.table().live(), 0u);

  SharedObjectRecord wrong = rig.asking(page);
  CHECK_STATUS(rig.table().mint(*allocation, wrong), Status::invalid_parameter);
  rig.check();
  CHECK(!wrong.assigned());
  SharedObjectRecord bigger = rig.asking(4u * page);
  CHECK_STATUS(rig.table().mint(*allocation, bigger), Status::invalid_parameter);
  rig.check();
  CHECK_EQ(rig.table().live(), 0u); }

TEST(shared_object_table_mint, propagates_a_pool_that_cannot_grow)
{ Fixture rig;
  Allocation *const allocation = rig.allocate(page);
  rig.platform().fail_allocation_at(1u);
  SharedObjectRecord record = rig.asking(page);
  CHECK_STATUS(rig.table().mint(*allocation, record), Status::no_memory);
  rig.check();
  CHECK(!record.assigned());
  CHECK_EQ(rig.table().live(), 0u);
  /* The id the refused mint would have taken is still the next one: a failed
   * mint must not burn one, or a table would run out under a hostile guest. */
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
  /* A second process opening the same record reaches the same object. */
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
  /* The object itself is untouched and still opens from its own adapter. */
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
                         minted.adapter_id(), 2u * page),
      SharedObjectRecord(minted.object_id(), SharedObjectKind(SharedObjectKind::native_fence),
                         minted.adapter_id(), 2u * page),
      SharedObjectRecord(minted.object_id(), minted.kind(), minted.adapter_id(), page),
      SharedObjectRecord(minted.object_id(), minted.kind(), minted.adapter_id(), 4u * page),
  };
  for (const SharedObjectRecord &record : wrong) {
      SharedObject *found = nullptr;
      CHECK_STATUS(rig.table().open(record, found), Status::invalid_parameter);
      rig.check();
      CHECK(found == nullptr);
  }
  /* Nothing was counted by any of them. */
  SharedObject *ours = nullptr;
  CHECK_STATUS(rig.table().open(minted, ours), Status::ok);
  CHECK_EQ(ours->opens(), 1u);
  CHECK_STATUS(rig.table().close(*ours), Status::ok);
  rig.check(); }

TEST(shared_object_table_close, refuses_an_object_this_table_does_not_hold)
{ Fixture rig;
  MockPlatform            other;
  av::ApertureSegment     aperture(other);
  av::HostVisibleSegment  host_visible(other);
  av::MemorySegments      segments(other, aperture, host_visible);
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

  /* Forgetting an allocation that never named one changes nothing. */
  rig.table().forget(*first);
  rig.check();
  CHECK_EQ(rig.table().live(), 1u); }

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
  /* The ceiling the table declares, the mutex it takes and releases above
   * that ceiling, and the object's own, which is reached under it. */
  for (int refusal = 0; refusal < 4; refusal++) CHECK(rig.platform().claim(Violation::irql_ceiling));
  CHECK(rig.platform().claim(Violation::blocking_at_dispatch)); }

TEST(shared_object_table_report, names_the_adapter_and_every_object_it_holds)
{ Fixture rig;
  Allocation *const allocation = rig.allocate(3u * page);
  SharedObjectRecord record = rig.asking(3u * page, SharedObjectKind::native_fence);
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
  CHECK(std::strstr(writer.dump(), "size=12288") != nullptr);
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
  /* An id at or above the next one to be minted is one the table cannot have
   * handed out. */
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
