#include "miniport/ddi/dxgk_allocation_info_record.hpp"
#include "miniport/ddi/dxgk_build_paging_buffer_map_aperture_segment_record.hpp"
#include "miniport/ddi/dxgk_build_paging_buffer_unmap_aperture_segment_record.hpp"
#include "miniport/ddi/dxgkarg_create_allocation_record.hpp"
#include "miniport/ddi/dxgkarg_describe_allocation_record.hpp"
#include "miniport/ddi/dxgkarg_destroy_allocation_record.hpp"
#include "miniport/display/display_sources.hpp"
#include "miniport/memory/allocation.hpp"
#include "miniport/memory/allocation_table.hpp"
#include "miniport/memory/shared_object_table.hpp"
#include "miniport/memory/memory_segments.hpp"
#include "miniport/memory/aperture_segment.hpp"
#include "core/support/status.hpp"
#include "core/platform/lock_rank.hpp"
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/guest_page_list.hpp"
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
    static void corrupt_resources(AllocationTable &table)
    { ObjectPoolProbe::corrupt_live_count(table.resources_); }
};

struct AllocationProbe {
    explicit AllocationProbe(Allocation &target) : allocation(target) {}
    void set_size(std::uint64_t size) { allocation.size_ = size; }
    Allocation &allocation;
};

}

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
    Fixture() : segment_(platform_), segments_(platform_, segment_),
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

    void claim_an_identity(std::uint32_t index, std::uint64_t size, std::uint64_t object_id)
    {
        av::SharedObjectRecord forged(0u, av::SharedObjectKind(av::SharedObjectKind::memory), 0u, size);
        forged.assign(object_id, 1u);
        CHECK_STATUS(forged.encode(descriptors_[index] + AllocationDescriptor::encoded_size,
                                   av::SharedObjectRecord::encoded_size),
                     Status::ok);
        infos_[index].PrivateDriverDataSize =
            AllocationDescriptor::encoded_size + av::SharedObjectRecord::encoded_size;
    }

    av::SharedObjectRecord minted(std::uint32_t index) const
    {
        av::SharedObjectRecord record;
        CHECK_STATUS(record.decode(descriptors_[index] + AllocationDescriptor::encoded_size,
                                   av::SharedObjectRecord::encoded_size),
                     Status::ok);
        return record;
    }

    Status destroy_resource(std::uint32_t count, void *resource)
    {
        void *handles[max_infos] = {};
        for (std::uint32_t i = 0u; i < count; i++) handles[i] = infos_[i].hAllocation;
        return destroy_list(handles, count, resource);
    }

    Status destroy_list(void *const *handles, std::uint32_t count, void *resource)
    {
        DxgkargDestroyAllocationRecord request = {};
        request.NumAllocations = count;
        request.pAllocationList = handles;
        request.hResource = resource;
        request.Flags = 1u;
        return table_.destroy(request);
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
    av::MemorySegments            segments_;
    DisplaySources                sources_;
    av::SharedObjectTable         shared_;
    AllocationTable               table_;
    DxgkAllocationInfoRecord      infos_[max_infos];
    std::uint8_t                  descriptors_[max_infos][AllocationDescriptor::encoded_size +
                                                          av::SharedObjectRecord::encoded_size];
    DxgkargCreateAllocationRecord request_;
};

}

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
      if (knob == 3) fixture.request().PrivateDriverDataSize = 4u;
      if (knob == 4) fixture.info(3u).pPrivateDriverData = nullptr;
      if (knob == 5) fixture.info(3u).PrivateDriverDataSize = AllocationDescriptor::encoded_size - 1u;
      if (knob == 6) fixture.descriptor(3u)[0] = 0u;
      if (knob == 7) {
          const AllocationDescriptor huge(AllocationKind::client, false, SurfaceLayout(),
                                          static_cast<std::uint64_t>(segment_bytes) + 1u, ScanoutTarget());
          CHECK_STATUS(huge.encode(fixture.descriptor(3u), AllocationDescriptor::encoded_size), Status::ok);
      }
      if (knob == 8) {
          const AllocationDescriptor wrapping(AllocationKind::client, false, SurfaceLayout(), ~0ull, ScanoutTarget());
          CHECK_STATUS(wrapping.encode(fixture.descriptor(3u), AllocationDescriptor::encoded_size), Status::ok);
      }
      CHECK_STATUS(fixture.create(), knob == 5 ? Status::not_supported : Status::invalid_parameter);
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
  handles[3] = handles[0];
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
  CHECK_STATUS(fixture.table().open(*allocation, &fixture, nullptr), Status::ok);
  CHECK_STATUS(fixture.destroy(1u), Status::invalid_state);
  CHECK_STATUS(fixture.table().close(*allocation), Status::ok);
  CHECK_STATUS(fixture.table().close(*allocation), Status::invalid_state);
  CHECK_STATUS(fixture.destroy(1u), Status::ok); }

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
  CHECK_EQ(request.Rotation, 1);   }

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

namespace {

constexpr std::uint64_t aperture_page = av::GuestPageList::page_bytes;

av::DxgkBuildPagingBufferMapApertureSegmentRecord placed(void *allocation, void *mdl, std::uint64_t offset)
{
    av::DxgkBuildPagingBufferMapApertureSegmentRecord request = {};
    request.hAllocation = allocation;
    request.SegmentId = ApertureSegment::segment_id;
    request.OffsetInPages = offset;
    request.NumberOfPages = 1u;
    request.pMdl = mdl;
    return request;
}

av::DxgkBuildPagingBufferUnmapApertureSegmentRecord taken(void *allocation, std::uint64_t offset)
{
    av::DxgkBuildPagingBufferUnmapApertureSegmentRecord request = {};
    request.hAllocation = allocation;
    request.SegmentId = ApertureSegment::segment_id;
    request.OffsetInPages = offset;
    request.NumberOfPages = 1u;
    return request;
}

std::uint64_t aperture_at(std::uint64_t offset)
{
    return static_cast<std::uint64_t>(ApertureSegment::base_address) + offset * aperture_page;
}

}

TEST(allocation_table_map_aperture, records_where_vidmm_placed_an_allocation_and_unmap_forgets_it)
{ Fixture fixture;
  CHECK_STATUS(fixture.create(), Status::ok);
  void *const handle = fixture.info(0u).hAllocation;
  const std::uint64_t frames[] = {9u * aperture_page};
  void *mdl = fixture.platform().attach_page_frames(frames, 1u);
  CHECK_STATUS(fixture.table().map_aperture(placed(handle, mdl, 0x30u)), Status::ok);
  const Allocation *allocation = fixture.table().find(handle);
  CHECK_EQ(allocation->pages_at(aperture_at(0x30u), 1u).frame(0u), 9u * aperture_page);
  CHECK_STATUS(fixture.table().unmap_aperture(taken(handle, 0x30u)), Status::ok);
  CHECK_EQ(allocation->pages_at(aperture_at(0x30u), 1u).count(), 0u);
  CHECK_EQ(fixture.platform().locks_held(), 0u);
  CHECK_STATUS(fixture.destroy(1u), Status::ok); }

TEST(allocation_table_map_aperture, a_handle_the_table_never_made_is_not_found)
{ Fixture fixture;
  std::uint64_t stranger = 0u;
  const std::uint64_t frames[] = {9u * aperture_page};
  void *mdl = fixture.platform().attach_page_frames(frames, 1u);
  CHECK_STATUS(fixture.table().map_aperture(placed(&stranger, mdl, 0u)), Status::not_found);
  CHECK_STATUS(fixture.table().unmap_aperture(taken(&stranger, 0u)), Status::not_found); }

TEST(allocation_table_map_aperture, runs_inside_the_tables_guard)
{ Fixture fixture;
  CHECK_STATUS(fixture.create(), Status::ok);
  void *const handle = fixture.info(0u).hAllocation;
  const std::uint64_t frames[] = {9u * aperture_page};
  void *mdl = fixture.platform().attach_page_frames(frames, 1u);
  AllocationTableProbe::corrupt_pool(fixture.table());
  static_cast<void>(fixture.table().map_aperture(placed(handle, mdl, 0u)));
  CHECK(fixture.platform().claim(Violation::broken_invariant));
  while (fixture.platform().claim(Violation::broken_invariant)) {}
  static_cast<void>(fixture.table().unmap_aperture(taken(handle, 0u)));
  CHECK(fixture.platform().claim(Violation::broken_invariant));
  while (fixture.platform().claim(Violation::broken_invariant)) {} }

TEST(allocation_table_map_aperture, is_passive_only)
{ Fixture fixture;
  std::uint64_t stranger = 0u;
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.table().map_aperture(placed(&stranger, nullptr, 0u)));
  static_cast<void>(fixture.table().unmap_aperture(taken(&stranger, 0u)));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {}
  while (fixture.platform().claim(Violation::blocking_at_dispatch)) {} }

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

TEST(allocation_table_create, mints_an_identity_for_a_creator_that_asked_for_one)
{ Fixture fixture;
  fixture.set_count(1u, shadow);
  fixture.ask_to_share(0u, 4096u);
  CHECK_STATUS(fixture.create(), Status::ok);
  CHECK_EQ(fixture.shared().live(), 1u);
  const av::SharedObjectRecord record = fixture.minted(0u);
  CHECK(!record.assigned());
  const auto *const allocation = static_cast<const av::Allocation *>(fixture.info(0u).hAllocation);
  CHECK(allocation != nullptr);
  if (allocation == nullptr) return;
  CHECK(allocation->shared_id() != 0u);
  const av::SharedObjectRecord names(allocation->shared_id(), av::SharedObjectKind(av::SharedObjectKind::memory),
                                     fixture.shared().adapter(), 4096u);
  av::SharedObject *object = nullptr;
  CHECK_STATUS(fixture.shared().open(names, object), Status::ok);
  CHECK(object != nullptr);
  CHECK_STATUS(fixture.shared().close(*object), Status::ok);
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
                                   AllocationDescriptor::encoded_size + av::SharedObjectRecord::encoded_size + 1u,
                                   48u + av::SharedObjectRecord::encoded_size};
  for (const std::uint32_t length : lengths) {
      Fixture fixture;
      fixture.ask_to_share(0u, 4096u);
      fixture.info(0u).PrivateDriverDataSize = length;
      CHECK_STATUS(fixture.create(), Status::not_supported);
      CHECK_EQ(fixture.table().live(), 0u);
      CHECK_EQ(fixture.shared().live(), 0u);
  }
  Fixture missing;
  missing.ask_to_share(0u, 4096u);
  missing.info(0u).pPrivateDriverData = nullptr;
  CHECK_STATUS(missing.create(), Status::invalid_parameter); }

TEST(allocation_table_create, refuses_a_record_that_does_not_decode_and_leaves_no_allocation_behind)
{ Fixture fixture;
  fixture.ask_to_share(0u, 4096u);
  fixture.descriptor(0u)[AllocationDescriptor::encoded_size] ^= 0xFFu;
  CHECK_STATUS(fixture.create(), Status::invalid_parameter);
  CHECK_EQ(fixture.table().live(), 0u);
  CHECK_EQ(fixture.shared().live(), 0u);
  CHECK(fixture.table().invariants_hold()); }

TEST(allocation_table_create, a_record_may_name_an_object_bigger_than_the_allocation_carrying_it)
{ Fixture fixture;
  fixture.ask_to_share(0u, 8192u);
  CHECK_STATUS(fixture.create(), Status::ok);
  CHECK_EQ(fixture.table().live(), 1u);
  CHECK_EQ(fixture.shared().live(), 1u);
  const auto *const allocation = static_cast<const av::Allocation *>(fixture.info(0u).hAllocation);
  CHECK(allocation != nullptr);
  if (allocation == nullptr) return;
  const av::SharedObjectRecord names(allocation->shared_id(), av::SharedObjectKind(av::SharedObjectKind::memory),
                                     fixture.shared().adapter(), 8192u);
  av::SharedObject *object = nullptr;
  CHECK_STATUS(fixture.shared().open(names, object), Status::ok);
  CHECK_STATUS(fixture.shared().close(*object), Status::ok);
  const av::SharedObjectRecord smaller(allocation->shared_id(), av::SharedObjectKind(av::SharedObjectKind::memory),
                                       fixture.shared().adapter(), 4096u);
  CHECK_STATUS(fixture.shared().open(smaller, object), Status::invalid_parameter);
  CHECK(fixture.table().invariants_hold()); }

TEST(allocation_table_create, unwinds_the_identity_of_an_earlier_allocation_when_a_later_one_fails)
{ Fixture fixture;
  fixture.set_count(2u, shadow);
  fixture.ask_to_share(0u, 4096u);
  fixture.ask_to_share(1u, 4096u);
  fixture.info(1u).PrivateDriverDataSize = 7u;
  CHECK_STATUS(fixture.create(), Status::not_supported);
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

TEST(allocation_table_create, a_record_that_already_names_an_object_takes_its_allocation_with_it)
{ Fixture fixture;
  fixture.claim_an_identity(0u, 4096u, 77u);
  CHECK_STATUS(fixture.create(), Status::invalid_parameter);
  CHECK_EQ(fixture.table().live(), 0u);
  CHECK_EQ(fixture.shared().live(), 0u);
  CHECK(fixture.info(0u).hAllocation == nullptr);
  CHECK(fixture.table().invariants_hold());
  CHECK(fixture.shared().invariants_hold()); }

TEST(allocation_table_create, a_create_that_asks_for_a_resource_is_answered_one)
{ Fixture fixture;
  fixture.request().Flags = 1u;
  CHECK_STATUS(fixture.create(), Status::ok);
  CHECK(fixture.request().hResource != nullptr);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.destroy(1u), Status::ok);
  CHECK_EQ(fixture.table().live(), 0u);
  CHECK(fixture.table().invariants_hold()); }

TEST(allocation_table_create, a_resource_create_may_carry_the_resource_s_own_private_data)
{ Fixture fixture;
  fixture.request().Flags = 1u;
  fixture.request().PrivateDriverDataSize = AllocationTable::resource_private_bytes;
  CHECK_STATUS(fixture.create(), Status::ok);
  CHECK(fixture.request().hResource != nullptr);
  CHECK(fixture.table().invariants_hold());
  CHECK_STATUS(fixture.destroy(1u), Status::ok);
  CHECK(fixture.table().invariants_hold()); }

TEST(allocation_table_create, a_second_create_into_a_resource_answers_the_same_one)
{ Fixture fixture;
  fixture.request().Flags = 1u;
  CHECK_STATUS(fixture.create(), Status::ok);
  void *const first = fixture.request().hResource;
  CHECK(first != nullptr);
  void *const kept = fixture.info(0u).hAllocation;

  fixture.set_count(1u, shadow);
  fixture.request().Flags = 1u;
  fixture.request().hResource = first;
  CHECK_STATUS(fixture.create(), Status::ok);
  CHECK(fixture.request().hResource == first);
  CHECK_EQ(fixture.table().live(), 2u);
  CHECK(fixture.table().invariants_hold());

  CHECK_STATUS(fixture.destroy_resource(1u, first), Status::invalid_state);
  void *const both[] = {kept, fixture.info(0u).hAllocation};
  CHECK_STATUS(fixture.destroy_list(both, 2u, first), Status::ok);
  CHECK_EQ(fixture.table().live(), 0u);
  CHECK(fixture.table().invariants_hold()); }

TEST(allocation_table_create, a_resource_the_host_cannot_hold_and_a_destroy_that_names_none)
{ { Fixture fixture;
    fixture.platform().fail_allocation_at(1u);
    fixture.request().Flags = 1u;
    CHECK_STATUS(fixture.create(), Status::no_memory);
    CHECK_EQ(fixture.table().live(), 0u);
    CHECK(fixture.table().invariants_hold()); }
  {
    Fixture fixture;
    CHECK_STATUS(fixture.create(), Status::ok);
    CHECK_STATUS(fixture.destroy_resource(1u, nullptr), Status::ok);
    CHECK_EQ(fixture.table().live(), 0u);
    CHECK(fixture.table().invariants_hold()); } }

TEST(allocation_table_invariants, a_broken_resource_pool_is_a_broken_table)
{ Fixture fixture;
  fixture.request().Flags = 1u;
  CHECK_STATUS(fixture.create(), Status::ok);
  CHECK(fixture.table().invariants_hold());
  AllocationTableProbe::corrupt_resources(fixture.table());
  CHECK(!fixture.table().invariants_hold()); }

TEST(allocation_table_create, a_resource_create_that_fails_part_way_leaves_no_resource)
{ Fixture fixture;
  fixture.set_count(3u, shadow);
  fixture.request().Flags = 1u;
  fixture.info(2u).PrivateDriverDataSize = AllocationDescriptor::encoded_size + 1u;
  CHECK_STATUS(fixture.create(), Status::not_supported);
  CHECK_EQ(fixture.table().live(), 0u);
  CHECK(fixture.table().invariants_hold());
  Fixture again;
  again.request().Flags = 1u;
  CHECK_STATUS(again.create(), Status::ok);
  CHECK(again.request().hResource != nullptr);
  CHECK(again.table().invariants_hold()); }

TEST(allocation_table_create, a_host_heap_may_be_bigger_than_the_aperture_because_the_host_owns_its_memory)
{ Fixture fixture;
  const AllocationDescriptor heap(AllocationKind::host_heap, false, SurfaceLayout(), std::uint64_t{8} << 30u, ScanoutTarget(), 3u);
  fixture.set_count(1u, heap);
  CHECK_STATUS(fixture.create(), Status::ok);
  CHECK(fixture.info(0u).Size == (std::uint64_t{8} << 30u));
  CHECK(fixture.table().invariants_hold());
  fixture.set_count(1u, shadow);
  const AllocationDescriptor huge(AllocationKind::shadow, true, SurfaceLayout(10u, 10u, 21u, 40u), std::uint64_t{8} << 30u, ScanoutTarget());
  fixture.set_count(1u, huge);
  CHECK_STATUS(fixture.create(), Status::invalid_parameter); }

TEST(allocation_table_create, a_shareable_host_heap_mints_an_identity)
{ Fixture fixture;
  const AllocationDescriptor heap(AllocationKind::host_heap, false, SurfaceLayout(), std::uint64_t{1} << 20u, ScanoutTarget(), 3u);
  fixture.set_count(1u, heap);
  fixture.ask_to_share(0u, std::uint64_t{1} << 20u);
  CHECK_STATUS(fixture.create(), Status::ok);
  const auto *const allocation = static_cast<const av::Allocation *>(fixture.info(0u).hAllocation);
  CHECK(allocation != nullptr);
  if (allocation == nullptr) return;
  CHECK(allocation->shared_id() != 0u);
  CHECK_EQ(allocation->host_key(), allocation->shared_id()); }
