#include "core/ddi/dxgk_allocation_info_record.hpp"
#include "core/ddi/dxgkarg_create_allocation_record.hpp"
#include "core/ddi/dxgkarg_describe_allocation_record.hpp"
#include "core/ddi/dxgkarg_destroy_allocation_record.hpp"
#include "core/display/display_sources.hpp"
#include "core/memory/allocation.hpp"
#include "core/memory/allocation_table.hpp"
#include "core/memory/shared_object_table.hpp"
#include "core/memory/host_visible_segment.hpp"
#include "core/memory/memory_segments.hpp"
#include "core/memory/aperture_segment.hpp"
#include "core/support/status.hpp"
#include "core/platform/lock_rank.hpp"
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/shared_object_record.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace av {

struct ObjectPoolProbe {
    template <typename T>
    static void corrupt_live_count(ObjectPool<T> &pool) { pool.live_ += 1u; }
};

struct AllocationTableProbe {
    static void corrupt_pool(AllocationTable &table) { ObjectPoolProbe::corrupt_live_count(table.allocations_); }
};

struct AllocationProbe {
    explicit AllocationProbe(Allocation &target) : allocation(target) {}
    void set_size(std::uint64_t size) { allocation.size_ = size; }
    Allocation &allocation;
};

}  // namespace av

using av::Allocation;
using av::AllocationDescriptor;
using av::AllocationKind;
using av::AllocationProbe;
using av::AllocationTableProbe;
using av::AllocationTable;
using av::ApertureSegment;
using av::DisplaySources;
using av::DxgkAllocationInfoRecord;
using av::DxgkargCreateAllocationRecord;
using av::DxgkargDescribeAllocationRecord;
using av::DxgkargDestroyAllocationRecord;
using av::Irql;
using av::MockPlatform;
using av::ScanoutTarget;
using av::Status;
using av::SurfaceLayout;
using av::Violation;

namespace {

constexpr std::uint32_t max_infos = 8u;
constexpr std::int64_t  segment_bytes = std::int64_t{1} << 30u;

const AllocationDescriptor primary(AllocationKind::shared_primary, true, SurfaceLayout(1024u, 768u, 21u, 4096u),
                                   4096u * 768u, ScanoutTarget(0u, 60u, 1u));
const AllocationDescriptor shadow(AllocationKind::shadow, true, SurfaceLayout(10u, 10u, 21u, 40u), 400u,
                                  ScanoutTarget());

class Fixture {
public:
    Fixture() : segment_(platform_), host_visible_(platform_), segments_(platform_, segment_, host_visible_),
        sources_(platform_), shared_(platform_), table_(platform_, segment_, segments_, shared_),
        infos_{}, descriptors_{}, request_{}
    {
        if (shared_.initialize() != Status::ok)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "shared objects");
        if (segment_.initialize(segment_bytes) != Status::ok)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "segment");
        set_count(1u, shadow);
    }

    void set_count(std::uint32_t count, const AllocationDescriptor &descriptor)
    {
        for (std::uint32_t i = 0u; i < max_infos; i++) {
            if (descriptor.encode(descriptors_[i], AllocationDescriptor::encoded_size) != Status::ok)
                av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "encode");
            std::memset(&infos_[i], 0, sizeof infos_[i]);
            infos_[i].pPrivateDriverData = descriptors_[i];
            infos_[i].PrivateDriverDataSize = AllocationDescriptor::encoded_size;
        }
        std::memset(&request_, 0, sizeof request_);
        request_.NumAllocations = count;
        request_.pAllocationInfo = infos_;
    }

    Status create() { return table_.create(request_); }

    /* What a creator that wants an identity hands the miniport: the descriptor
     * and, after it, a record naming a kind and a size and nothing else. */
    void ask_to_share(std::uint32_t index, std::uint64_t size,
                      std::uint32_t kind = av::SharedObjectKind::memory)
    {
        const av::SharedObjectRecord asking(0u, av::SharedObjectKind(kind), 0u, size);
        CHECK_STATUS(asking.encode(descriptors_[index] + AllocationDescriptor::encoded_size,
                                   av::SharedObjectRecord::encoded_size),
                     Status::ok);
        infos_[index].PrivateDriverDataSize =
            AllocationDescriptor::encoded_size + av::SharedObjectRecord::encoded_size;
    }

    /* The record the miniport wrote back, as the opening process reads it. */
    av::SharedObjectRecord minted(std::uint32_t index) const
    {
        av::SharedObjectRecord record;
        CHECK_STATUS(record.decode(descriptors_[index] + AllocationDescriptor::encoded_size,
                                   av::SharedObjectRecord::encoded_size),
                     Status::ok);
        return record;
    }

    Status destroy(std::uint32_t count)
    {
        void *handles[max_infos] = {};
        for (std::uint32_t i = 0u; i < count; i++) handles[i] = infos_[i].hAllocation;
        DxgkargDestroyAllocationRecord request = {};
        request.NumAllocations = count;
        request.pAllocationList = handles;
        return table_.destroy(request);
    }

    MockPlatform                  &platform() { return platform_; }
    AllocationTable               &table() { return table_; }
    av::SharedObjectTable         &shared() { return shared_; }
    DisplaySources                &sources() { return sources_; }
    DxgkAllocationInfoRecord      &info(std::uint32_t i) { return infos_[i]; }
    std::uint8_t                  *descriptor(std::uint32_t i) { return descriptors_[i]; }
    DxgkargCreateAllocationRecord &request() { return request_; }

private:
    MockPlatform                  platform_;
    ApertureSegment               segment_;
    av::HostVisibleSegment        host_visible_;
    av::MemorySegments            segments_;
    DisplaySources                sources_;
    av::SharedObjectTable         shared_;
    AllocationTable               table_;
    DxgkAllocationInfoRecord      infos_[max_infos];
    /* Room for a descriptor and, where a test asks to share, the record that
     * follows it (feature 053). */
    std::uint8_t                  descriptors_[max_infos][AllocationDescriptor::encoded_size +
                                                          av::SharedObjectRecord::encoded_size];
    DxgkargCreateAllocationRecord request_;
};

}  // namespace

/* ---- create ------------------------------------------------------------------ */

TEST(allocation_table_create, gives_each_allocation_a_live_handle_and_a_page_rounded_size)
{ Fixture fixture;
  fixture.set_count(3u, shadow);
  fixture.request().hResource = &fixture;
  CHECK_STATUS(fixture.create(), Status::ok);
  CHECK(fixture.table().invariants_hold());
  CHECK_EQ(fixture.table().live(), 3u);
  CHECK(fixture.request().hResource == nullptr);
  for (std::uint32_t i = 0u; i < 3u; i++) {
      Allocation *allocation = fixture.table().find(fixture.info(i).hAllocation);
      CHECK(allocation != nullptr);
      CHECK(fixture.info(i).Size == 0x1000u);
      CHECK_EQ(fixture.info(i).SupportedReadSegmentSet, 2u);
  }
  CHECK(fixture.info(0u).hAllocation != fixture.info(1u).hAllocation); }

TEST(allocation_table_create, refuses_a_malformed_request_and_keeps_nothing)
{ for (int knob = 0; knob < 9; knob++) {
      Fixture fixture;
      fixture.set_count(4u, shadow);
      if (knob == 0) fixture.request().NumAllocations = 0u;
      if (knob == 1) fixture.request().NumAllocations = AllocationTable::max_per_call + 1u;
      if (knob == 2) fixture.request().pAllocationInfo = nullptr;
      if (knob == 3) fixture.request().PrivateDriverDataSize = 4u;          /* resource data: none defined */
      if (knob == 4) fixture.info(3u).pPrivateDriverData = nullptr;
      if (knob == 5) fixture.info(3u).PrivateDriverDataSize = AllocationDescriptor::encoded_size - 1u;
      if (knob == 6) fixture.descriptor(3u)[0] = 0u;                        /* bad magic */
      if (knob == 7) {                                                       /* larger than the segment */
          const AllocationDescriptor huge(AllocationKind::client, false, SurfaceLayout(),
                                          static_cast<std::uint64_t>(segment_bytes) + 1u, ScanoutTarget());
          CHECK_STATUS(huge.encode(fixture.descriptor(3u), AllocationDescriptor::encoded_size), Status::ok);
      }
      if (knob == 8) {                                                       /* would wrap when page-rounded */
          const AllocationDescriptor wrapping(AllocationKind::client, false, SurfaceLayout(), ~0ull, ScanoutTarget());
          CHECK_STATUS(wrapping.encode(fixture.descriptor(3u), AllocationDescriptor::encoded_size), Status::ok);
      }
      CHECK_STATUS(fixture.create(), Status::invalid_parameter);
      CHECK_EQ(fixture.table().live(), 0u);
      CHECK_EQ(fixture.platform().live_allocations(), 0u);
      CHECK(fixture.table().invariants_hold());
  } }

TEST(allocation_table_create, each_allocation_failing_in_turn_leaves_nothing)
{ for (std::uint32_t nth = 1u; nth <= 3u; nth++) {
      Fixture fixture;
      fixture.set_count(max_infos, shadow);
      fixture.platform().fail_allocation_at(nth);
      const Status status = fixture.create();
      fixture.platform().fail_allocation_at(0u);
      CHECK(status == Status::ok || status == Status::no_memory);
      if (status == Status::no_memory) {
          CHECK_EQ(fixture.table().live(), 0u);
          CHECK_EQ(fixture.platform().live_allocations(), 0u);
      } else {
          CHECK_STATUS(fixture.destroy(max_infos), Status::ok);
      }
  } }

TEST(allocation_table_create, a_chunk_that_fills_mid_call_takes_its_allocation_failure_cleanly)
{ Fixture fixture;
  for (std::uint32_t round = 0u; round < 8u; round++) {
      fixture.set_count(max_infos, shadow);
      CHECK_STATUS(fixture.create(), Status::ok);
  }
  fixture.set_count(max_infos, shadow);
  fixture.platform().fail_allocation_at(1u);
  CHECK_STATUS(fixture.create(), Status::no_memory);
  fixture.platform().fail_allocation_at(0u);
  CHECK_EQ(fixture.table().live(), 64u);
  CHECK(fixture.table().invariants_hold()); }

/* ---- destroy ----------------------------------------------------------------- */

TEST(allocation_table_destroy, releases_every_listed_allocation)
{ Fixture fixture;
  fixture.set_count(3u, shadow);
  CHECK_STATUS(fixture.create(), Status::ok);
  CHECK_STATUS(fixture.destroy(3u), Status::ok);
  CHECK(fixture.table().invariants_hold());
  CHECK_EQ(fixture.table().live(), 0u);
  CHECK(fixture.table().find(fixture.info(0u).hAllocation) == nullptr);
  CHECK_EQ(fixture.platform().live_allocations(), 0u); }

TEST(allocation_table_destroy, refuses_the_whole_list_for_one_bad_handle)
{ Fixture fixture;
  fixture.set_count(3u, shadow);
  CHECK_STATUS(fixture.create(), Status::ok);
  void *handles[4] = {fixture.info(0u).hAllocation, fixture.info(1u).hAllocation, fixture.info(2u).hAllocation, &fixture};
  DxgkargDestroyAllocationRecord request = {};
  request.NumAllocations = 4u;
  request.pAllocationList = handles;
  CHECK_STATUS(fixture.table().destroy(request), Status::invalid_parameter);
  handles[3] = handles[0];   /* listed twice */
  CHECK_STATUS(fixture.table().destroy(request), Status::invalid_parameter);
  request.pAllocationList = nullptr;
  CHECK_STATUS(fixture.table().destroy(request), Status::invalid_parameter);
  CHECK_EQ(fixture.table().live(), 3u);
  request.NumAllocations = 0u;
  CHECK_STATUS(fixture.table().destroy(request), Status::ok);
  CHECK_EQ(fixture.table().live(), 3u); }

TEST(allocation_table_destroy, an_allocation_still_open_is_refused)
{ Fixture fixture;
  CHECK_STATUS(fixture.create(), Status::ok);
  Allocation *allocation = fixture.table().find(fixture.info(0u).hAllocation);
  CHECK_STATUS(fixture.table().open(*allocation, &fixture), Status::ok);
  CHECK_STATUS(fixture.destroy(1u), Status::invalid_state);
  CHECK_STATUS(fixture.table().close(*allocation), Status::ok);
  CHECK_STATUS(fixture.table().close(*allocation), Status::invalid_state);
  CHECK_STATUS(fixture.destroy(1u), Status::ok); }

/* ---- describe ---------------------------------------------------------------- */

TEST(allocation_table_describe, answers_the_primary_layout_refresh_rate_and_source_format)
{ Fixture fixture;
  fixture.set_count(1u, primary);
  CHECK_STATUS(fixture.create(), Status::ok);
  CHECK_STATUS(fixture.sources().set_private_format(0u, 0x77u), Status::ok);
  DxgkargDescribeAllocationRecord request;
  std::memset(&request, 0xEE, sizeof request);
  request.hAllocation = fixture.info(0u).hAllocation;
  CHECK_STATUS(fixture.table().describe(request, fixture.sources()), Status::ok);
  CHECK_EQ(request.Width, 1024u);
  CHECK_EQ(request.Height, 768u);
  CHECK_EQ(request.Format, 21);
  CHECK_EQ(request.MultisampleMethodType, 0u);
  CHECK_EQ(request.MultisampleMethodQuality, 0u);
  CHECK_EQ(request.RefreshRateNumerator, 60u);
  CHECK_EQ(request.RefreshRateDenominator, 1u);
  CHECK_EQ(request.PrivateDriverFormatAttribute, 0x77u);
  CHECK_EQ(request.Rotation, 1);   /* D3DDDI_ROTATION_IDENTITY */ }

TEST(allocation_table_describe, another_kind_has_no_private_format_and_a_stranger_is_invalid)
{ Fixture fixture;
  CHECK_STATUS(fixture.create(), Status::ok);
  CHECK_STATUS(fixture.sources().set_private_format(0u, 0x77u), Status::ok);
  DxgkargDescribeAllocationRecord request = {};
  request.hAllocation = fixture.info(0u).hAllocation;
  CHECK_STATUS(fixture.table().describe(request, fixture.sources()), Status::ok);
  CHECK_EQ(request.PrivateDriverFormatAttribute, 0u);
  request.hAllocation = &request;
  CHECK_STATUS(fixture.table().describe(request, fixture.sources()), Status::invalid_parameter); }

TEST(allocation_table, is_passive_only)
{ Fixture fixture;
  CHECK_STATUS(fixture.create(), Status::ok);
  DxgkargDescribeAllocationRecord request = {};
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.table().find(nullptr));
  static_cast<void>(fixture.table().describe(request, fixture.sources()));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {}
  while (fixture.platform().claim(Violation::blocking_at_dispatch)) {}
  CHECK_STATUS(fixture.destroy(1u), Status::ok); }

TEST(allocation_table_invariants, a_broken_allocation_breaks_the_table)
{ Fixture fixture;
  CHECK_STATUS(fixture.create(), Status::ok);
  Allocation *allocation = fixture.table().find(fixture.info(0u).hAllocation);
  AllocationProbe(*allocation).set_size(0x1001u);
  CHECK(!fixture.table().invariants_hold());
  AllocationProbe(*allocation).set_size(0x1000u);
  CHECK(fixture.table().invariants_hold());
  AllocationTableProbe::corrupt_pool(fixture.table());
  CHECK(!fixture.table().invariants_hold()); }

/* ---- report -------------------------------------------------------------------- */

TEST(allocation_table_report, writes_the_table_then_each_allocation)
{ Fixture fixture;
  fixture.set_count(2u, shadow);
  CHECK_STATUS(fixture.create(), Status::ok);
  char bytes[512];
  av::StateWriter writer(bytes, sizeof bytes);
  fixture.table().report(writer);
  char expected[512];
  std::snprintf(expected, sizeof expected,
                "allocation_table/0 busy=false live=2\n"
                "allocation_table/0/allocation/0 handle=0x%016llX kind=2 size=4096 opens=0 cpu_visible=true\n"
                "allocation_table/0/allocation/1 handle=0x%016llX kind=2 size=4096 opens=0 cpu_visible=true",
                static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(fixture.info(0u).hAllocation)),
                static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(fixture.info(1u).hAllocation)));
  CHECK(std::strcmp(writer.dump(), expected) == 0);
  CHECK(fixture.table().invariants_hold());
  CHECK_EQ(fixture.platform().locks_held(), 0u);
  CHECK_STATUS(fixture.destroy(2u), Status::ok); }

TEST(allocation_table_report, a_table_another_thread_holds_is_only_busy)
{ Fixture fixture;
  CHECK_STATUS(fixture.create(), Status::ok);
  fixture.platform().hold_elsewhere(av::LockRank::allocation_table);
  char bytes[128];
  av::StateWriter writer(bytes, sizeof bytes);
  fixture.table().report(writer);
  CHECK(std::strcmp(writer.dump(), "allocation_table/0 busy=true") == 0);
  fixture.platform().released_elsewhere();
  CHECK_STATUS(fixture.destroy(1u), Status::ok); }

TEST(allocation_table_report, is_apc_at_most)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  char bytes[128];
  av::StateWriter writer(bytes, sizeof bytes);
  fixture.platform().hold_elsewhere(av::LockRank::allocation_table);
  fixture.table().report(writer);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  fixture.platform().set_irql(Irql::passive); }

/* Feature 053: an allocation whose private data asks for an identity gets one,
 * written back into the creator's own bytes - the only thing that reaches the
 * process that opens it (docs/kb/04-memory.md). */
TEST(allocation_table_create, mints_an_identity_for_a_creator_that_asked_for_one)
{ Fixture fixture;
  fixture.set_count(1u, shadow);
  fixture.ask_to_share(0u, 4096u);
  CHECK_STATUS(fixture.create(), Status::ok);
  CHECK_EQ(fixture.shared().live(), 1u);
  const av::SharedObjectRecord record = fixture.minted(0u);
  CHECK(record.assigned());
  CHECK_EQ(record.adapter_id(), fixture.shared().adapter());
  CHECK_EQ(record.size(), 4096u);
  const auto *const allocation = static_cast<const av::Allocation *>(fixture.info(0u).hAllocation);
  CHECK_EQ(allocation->shared_id(), record.object_id());
  CHECK(fixture.table().invariants_hold());
  CHECK(fixture.shared().invariants_hold()); }

TEST(allocation_table_create, leaves_an_allocation_that_asked_for_nothing_without_one)
{ Fixture fixture;
  CHECK_STATUS(fixture.create(), Status::ok);
  CHECK_EQ(fixture.shared().live(), 0u);
  const auto *const allocation = static_cast<const av::Allocation *>(fixture.info(0u).hAllocation);
  CHECK_EQ(allocation->shared_id(), 0u); }

TEST(allocation_table_create, refuses_private_data_of_any_other_length_than_the_two_it_knows)
{ const std::uint32_t lengths[] = {0u, 1u, AllocationDescriptor::encoded_size - 1u,
                                   AllocationDescriptor::encoded_size + 1u,
                                   AllocationDescriptor::encoded_size + av::SharedObjectRecord::encoded_size - 1u,
                                   AllocationDescriptor::encoded_size + av::SharedObjectRecord::encoded_size + 1u};
  for (const std::uint32_t length : lengths) {
      Fixture fixture;
      fixture.ask_to_share(0u, 4096u);
      fixture.info(0u).PrivateDriverDataSize = length;
      CHECK_STATUS(fixture.create(), Status::invalid_parameter);
      CHECK_EQ(fixture.table().live(), 0u);
      CHECK_EQ(fixture.shared().live(), 0u);
  } }

TEST(allocation_table_create, refuses_a_record_that_does_not_decode_and_leaves_no_allocation_behind)
{ Fixture fixture;
  fixture.ask_to_share(0u, 4096u);
  fixture.descriptor(0u)[AllocationDescriptor::encoded_size] ^= 0xFFu;   /* the magic */
  CHECK_STATUS(fixture.create(), Status::invalid_parameter);
  CHECK_EQ(fixture.table().live(), 0u);
  CHECK_EQ(fixture.shared().live(), 0u);
  CHECK(fixture.table().invariants_hold()); }

TEST(allocation_table_create, refuses_a_record_whose_size_is_not_the_allocations)
{ Fixture fixture;
  fixture.ask_to_share(0u, 8192u);   /* the descriptor rounds to one page */
  CHECK_STATUS(fixture.create(), Status::invalid_parameter);
  CHECK_EQ(fixture.table().live(), 0u);
  CHECK_EQ(fixture.shared().live(), 0u); }

TEST(allocation_table_create, unwinds_the_identity_of_an_earlier_allocation_when_a_later_one_fails)
{ Fixture fixture;
  fixture.set_count(2u, shadow);
  fixture.ask_to_share(0u, 4096u);
  fixture.ask_to_share(1u, 4096u);
  fixture.info(1u).PrivateDriverDataSize = 7u;
  CHECK_STATUS(fixture.create(), Status::invalid_parameter);
  CHECK_EQ(fixture.table().live(), 0u);
  CHECK_EQ(fixture.shared().live(), 0u);
  CHECK(fixture.shared().invariants_hold()); }

TEST(allocation_table_destroy, takes_the_identity_with_the_allocation)
{ Fixture fixture;
  fixture.ask_to_share(0u, 4096u);
  CHECK_STATUS(fixture.create(), Status::ok);
  CHECK_EQ(fixture.shared().live(), 1u);
  CHECK_STATUS(fixture.destroy(1u), Status::ok);
  CHECK_EQ(fixture.shared().live(), 0u);
  CHECK(fixture.shared().invariants_hold()); }
