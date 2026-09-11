#include "../../src/core/allocation.hpp"
#include "../../src/core/ddi/dxgk_allocation_info_record.hpp"
#include "../../src/core/status.hpp"
#include "../../src/proto/allocation_descriptor.hpp"
#include "../harness/mock_platform.hpp"
#include "../harness/test_registry.hpp"
#include "../harness/violation.hpp"

#include <cstdint>
#include <cstring>

namespace av {

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
using av::DxgkAllocationInfoRecord;
using av::Irql;
using av::MockPlatform;
using av::ScanoutTarget;
using av::Status;
using av::SurfaceLayout;
using av::Violation;

namespace {

const AllocationDescriptor shadow(AllocationKind::shadow, true, SurfaceLayout(10u, 10u, 21u, 40u), 400u,
                                  ScanoutTarget());

}  // namespace

TEST(allocation, remembers_its_descriptor_and_page_rounded_size)
{ MockPlatform platform; const Allocation allocation(platform, shadow, 0x1000u);
  CHECK(allocation.descriptor().kind() == AllocationKind::shadow);
  CHECK(allocation.size() == 0x1000u);
  CHECK_EQ(allocation.opens(), 0u);
  CHECK(allocation.invariants_hold()); }

TEST(allocation_report_to, tells_vidmm_the_size_segment_and_cpu_visibility)
{ MockPlatform platform; Allocation allocation(platform, shadow, 0x1000u);
  DxgkAllocationInfoRecord info;
  std::memset(&info, 0xEE, sizeof info);
  info.pPrivateDriverData = &platform;
  allocation.report_to(info);
  CHECK(info.Size == 0x1000u);
  CHECK_EQ(info.Alignment, 0u);
  CHECK(info.PitchAlignedSize == 0u);
  CHECK_EQ(info.HintedBank, 0u);
  CHECK_EQ(info.PreferredSegment, 1u);
  CHECK_EQ(info.SupportedReadSegmentSet, 1u);
  CHECK_EQ(info.SupportedWriteSegmentSet, 1u);
  CHECK_EQ(info.EvictionSegmentSet, 0u);
  CHECK_EQ(info.MaximumRenamingListLength, 0u);
  CHECK(info.hAllocation == &allocation);
  CHECK_EQ(info.Flags, 1u);                         /* CpuVisible */
  CHECK_EQ(info.AllocationPriority, 0x28000000u);   /* D3DDDI_ALLOCATIONPRIORITY_NORMAL */
  CHECK_EQ(info.Flags2, 0u);
  CHECK(info.pPrivateDriverData == &platform); }

TEST(allocation_report_to, a_gpu_only_allocation_is_not_cpu_visible)
{ MockPlatform platform;
  const AllocationDescriptor texture(AllocationKind::gdi, false, SurfaceLayout(1u, 1u, 21u, 4u), 4u, ScanoutTarget());
  Allocation allocation(platform, texture, 0x1000u);
  DxgkAllocationInfoRecord info = {};
  allocation.report_to(info);
  CHECK_EQ(info.Flags, 0u); }

TEST(allocation_open, counts_opens_and_refuses_a_close_with_none)
{ MockPlatform platform; Allocation allocation(platform, shadow, 0x1000u);
  CHECK_STATUS(allocation.close(), Status::invalid_state);
  CHECK_STATUS(allocation.open(), Status::ok);
  CHECK(allocation.invariants_hold());
  CHECK_STATUS(allocation.open(), Status::ok);
  CHECK_EQ(allocation.opens(), 2u);
  CHECK_STATUS(allocation.close(), Status::ok);
  CHECK(allocation.invariants_hold());
  CHECK_EQ(allocation.opens(), 1u); }

TEST(allocation, is_legal_up_to_apc)
{ MockPlatform platform; Allocation allocation(platform, shadow, 0x1000u); DxgkAllocationInfoRecord info = {};
  platform.set_irql(Irql::dispatch);
  allocation.report_to(info);
  static_cast<void>(allocation.open());
  static_cast<void>(allocation.close());
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  while (platform.claim(Violation::irql_ceiling)) {} }

TEST(allocation_invariants, a_size_below_the_descriptor_or_off_a_page_is_detected)
{ MockPlatform platform; Allocation allocation(platform, shadow, 0x1000u);
  AllocationProbe probe(allocation);
  probe.set_size(0x800u);
  CHECK(!allocation.invariants_hold());
  probe.set_size(0x1001u);
  CHECK(!allocation.invariants_hold());
  probe.set_size(0u);
  CHECK(!allocation.invariants_hold());
  const AllocationDescriptor large(AllocationKind::client, false, SurfaceLayout(), 0x1800u, ScanoutTarget());
  Allocation undersized(platform, large, 0x2000u);
  AllocationProbe(undersized).set_size(0x1000u);
  CHECK(!undersized.invariants_hold()); }
