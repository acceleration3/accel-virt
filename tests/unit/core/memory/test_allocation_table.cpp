#include "core/ddi/dxgk_allocation_info_record.hpp"
#include "core/ddi/dxgkarg_create_allocation_record.hpp"
#include "core/ddi/dxgkarg_describe_allocation_record.hpp"
#include "core/ddi/dxgkarg_destroy_allocation_record.hpp"
#include "core/display/display_sources.hpp"
#include "core/memory/allocation.hpp"
#include "core/memory/allocation_table.hpp"
#include "core/memory/aperture_segment.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "proto/resources/allocation_descriptor.hpp"

#include <cstdint>
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
    Fixture() : segment_(platform_), sources_(platform_), table_(platform_, segment_), infos_{}, descriptors_{}, request_{}
    {
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
    DisplaySources                &sources() { return sources_; }
    DxgkAllocationInfoRecord      &info(std::uint32_t i) { return infos_[i]; }
    std::uint8_t                  *descriptor(std::uint32_t i) { return descriptors_[i]; }
    DxgkargCreateAllocationRecord &request() { return request_; }

private:
    MockPlatform                  platform_;
    ApertureSegment               segment_;
    DisplaySources                sources_;
    AllocationTable               table_;
    DxgkAllocationInfoRecord      infos_[max_infos];
    std::uint8_t                  descriptors_[max_infos][AllocationDescriptor::encoded_size];
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
  CHECK_STATUS(fixture.table().open(*allocation), Status::ok);
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
