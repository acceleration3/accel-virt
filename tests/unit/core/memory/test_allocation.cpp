#include "core/ddi/dxgk_allocation_info_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_map_aperture_segment_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_unmap_aperture_segment_record.hpp"
#include "core/memory/aperture_segment.hpp"
#include "core/memory/allocation.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/guest_page_list.hpp"

#include <cstdint>
#include <cstring>

namespace av {

struct AperturePagesProbe {
    static void forget_capacity(AperturePages &pages) { pages.capacity_ = 0u; }
};

struct AllocationProbe {
    explicit AllocationProbe(Allocation &target) : allocation(target) {}
    void set_size(std::uint64_t size) { allocation.size_ = size; }
    /* An allocation whose aperture pages lost their own invariant. */
    void break_aperture() { AperturePagesProbe::forget_capacity(allocation.aperture_); }
    Allocation &allocation;
};

}  // namespace av

using av::Allocation;
using av::AllocationDescriptor;
using av::AllocationKind;
using av::AllocationProbe;
using av::AperturePages;
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

constexpr std::uint64_t page = av::GuestPageList::page_bytes;

std::uint64_t aperture_address(std::uint64_t offset_in_pages)
{
    return static_cast<std::uint64_t>(av::ApertureSegment::base_address) + offset_in_pages * page;
}

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
  CHECK_EQ(info.PreferredSegment, 2u);   /* VidMm's system memory segment */
  CHECK_EQ(info.SupportedReadSegmentSet, 2u);
  CHECK_EQ(info.SupportedWriteSegmentSet, 2u);
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

/* An aperture address means nothing to the device; these are the pages behind
 * it, and an address of a different allocation's is not this one's. */
TEST(allocation_pages_at, answers_the_pages_the_aperture_mapping_recorded)
{ MockPlatform platform; Allocation allocation(platform, shadow, 2u * page);
  const std::uint64_t frames[] = {6u * page, 7u * page};
  av::DxgkBuildPagingBufferMapApertureSegmentRecord mapping = {};
  mapping.SegmentId = av::ApertureSegment::segment_id;
  mapping.OffsetInPages = 5u;
  mapping.NumberOfPages = 2u;
  mapping.pMdl = platform.attach_page_frames(frames, 2u);
  CHECK_STATUS(allocation.map_aperture(mapping), Status::ok);
  const av::GuestPageList pages = allocation.pages_at(aperture_address(5u), 2u);
  CHECK_EQ(pages.count(), 2u);
  CHECK_EQ(pages.frame(0u), 6u * page);
  CHECK_EQ(pages.runs(), 1u);
  CHECK_EQ(allocation.pages_at(aperture_address(5u) + 1u, 2u).count(), 0u);
  CHECK_EQ(allocation.pages_at(0x1000u, 2u).count(), 0u);
  CHECK(allocation.invariants_hold()); }

TEST(allocation_unmap_aperture, leaves_no_pages_behind)
{ MockPlatform platform; Allocation allocation(platform, shadow, 2u * page);
  const std::uint64_t frames[] = {6u * page, 7u * page};
  av::DxgkBuildPagingBufferMapApertureSegmentRecord mapping = {};
  mapping.SegmentId = av::ApertureSegment::segment_id;
  mapping.NumberOfPages = 2u;
  mapping.pMdl = platform.attach_page_frames(frames, 2u);
  CHECK_STATUS(allocation.map_aperture(mapping), Status::ok);
  CHECK_EQ(allocation.pages_at(aperture_address(0u), 2u).count(), 2u);
  av::DxgkBuildPagingBufferUnmapApertureSegmentRecord unmapping = {};
  unmapping.SegmentId = av::ApertureSegment::segment_id;
  unmapping.NumberOfPages = 2u;
  allocation.unmap_aperture(unmapping);
  CHECK_EQ(allocation.pages_at(aperture_address(0u), 2u).count(), 0u);
  CHECK(allocation.invariants_hold()); }

TEST(allocation_map_aperture, is_passive_only)
{ MockPlatform platform; Allocation allocation(platform, shadow, page);
  av::DxgkBuildPagingBufferUnmapApertureSegmentRecord unmapping = {};
  platform.set_irql(Irql::dispatch);
  static_cast<void>(allocation.map_aperture(av::DxgkBuildPagingBufferMapApertureSegmentRecord{}));
  allocation.unmap_aperture(unmapping);
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  while (platform.claim(Violation::irql_ceiling)) {} }

/* Only the aperture segment is one the host can read a frame out of: a primary
 * left in VidMm's implicit system memory segment is never mapped, and
 * SetVidPnSourceAddress then answers segment 0, address 0. */
TEST(allocation_report_to, a_primary_is_preferred_in_the_aperture_and_anything_else_in_system_memory)
{ MockPlatform platform;
  const AllocationDescriptor primary(AllocationKind::shared_primary, true,
                                     SurfaceLayout(64u, 64u, 21u, 256u), 64u * 256u, ScanoutTarget(0u, 60u, 1u));
  Allocation scanned_out(platform, primary, 64u * 256u);
  DxgkAllocationInfoRecord info = {};
  scanned_out.report_to(info);
  CHECK_EQ(info.PreferredSegment, av::ApertureSegment::segment_id);
  CHECK_EQ(info.SupportedReadSegmentSet, av::ApertureSegment::segment_set);
  CHECK_EQ(info.SupportedWriteSegmentSet, av::ApertureSegment::segment_set);
  Allocation other(platform, shadow, 0x1000u);
  DxgkAllocationInfoRecord plain = {};
  other.report_to(plain);
  CHECK_EQ(plain.PreferredSegment, av::ApertureSegment::system_memory_segment_id);
  CHECK_EQ(plain.SupportedReadSegmentSet, 1u << (av::ApertureSegment::system_memory_segment_id - 1u)); }

/* The aperture pages are part of the allocation's own invariant. */
TEST(allocation_invariants, aperture_pages_that_lost_theirs_are_detected)
{ MockPlatform platform; Allocation allocation(platform, shadow, 0x1000u);
  CHECK(allocation.invariants_hold());
  AllocationProbe(allocation).break_aperture();
  CHECK(!allocation.invariants_hold()); }
