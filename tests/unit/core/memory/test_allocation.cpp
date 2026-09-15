#include "core/ddi/dxgk_allocation_info_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_map_aperture_segment_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_unmap_aperture_segment_record.hpp"
#include "core/memory/aperture_segment.hpp"
#include "core/memory/allocation.hpp"
#include "core/memory/host_visible_segment.hpp"
#include "core/memory/memory_segments.hpp"
#include "core/pci/pci_base_addresses.hpp"
#include "core/virtio/virtio_pci_layout.hpp"
#include "core/virtio/virtio_region.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
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
    /* Opens with nobody holding them: what open() and close() never leave
     * behind, and what the invariant is there to catch. */
    void set_opens(std::uint32_t opens) { allocation.opens_ = opens; }
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

/* What this adapter has: the aperture, and a host-visible segment only where a
 * test says the device offers one. */
class Segments {
public:
    explicit Segments(MockPlatform &platform) : aperture_(platform), host_visible_(platform),
        segments_(platform, aperture_, host_visible_)
    { CHECK_STATUS(aperture_.initialize(0x100000000), av::Status::ok); }

    /* The device offers a region of `bytes`, as one with hostmem does. */
    void offer_host_visible(MockPlatform &platform, std::uint64_t bytes);

    const av::MemorySegments &segments() const { return segments_; }

private:
    av::ApertureSegment     aperture_;
    av::HostVisibleSegment  host_visible_;
    av::MemorySegments      segments_;
};

void Segments::offer_host_visible(MockPlatform &platform, std::uint64_t bytes)
{
    std::uint8_t config[256] = {};
    constexpr std::uint64_t bar_address = 0x4000000000ull;
    constexpr std::uint8_t  bar = 4u;
    const std::uint32_t low = static_cast<std::uint32_t>(bar_address) | 0xCu;
    const std::uint32_t high = static_cast<std::uint32_t>(bar_address >> 32u);
    std::memcpy(config + 0x10u + 4u * bar, &low, 4u);
    std::memcpy(config + 0x14u + 4u * bar, &high, 4u);
    const av::VirtioRegion structure(2u, 0u, 0x1000u);
    const av::VirtioRegion region(bar, 0u, bytes);
    const av::VirtioRegion shared[av::VirtioPciLayout::max_shared_memory_regions] = {region};
    const std::uint8_t ids[av::VirtioPciLayout::max_shared_memory_regions] = {1u};
    const av::VirtioPciLayout layout(structure, structure, structure, structure, 4u, shared, ids, 1u);
    host_visible_.discover(av::PciBaseAddresses(config, sizeof config), layout);
    host_visible_.offer(av::CapsetInfo(av::ProtocolVersion(av::ProtocolVersion::current_major,
                                                           av::ProtocolVersion::current_minor),
                                       1u, 64u, 12u, 24u, av::CapsetInfo::feature_host_visible_memory,
                                       av::GpuIdentity()));
    static_cast<void>(platform);
}

const AllocationDescriptor shadow(AllocationKind::shadow, true, SurfaceLayout(10u, 10u, 21u, 40u), 400u,
                                  ScanoutTarget());

constexpr std::uint64_t page = av::GuestPageList::page_bytes;

std::uint64_t aperture_address(std::uint64_t offset_in_pages)
{
    return static_cast<std::uint64_t>(av::ApertureSegment::base_address) + offset_in_pages * page;
}

}  // namespace

TEST(allocation, remembers_its_descriptor_and_page_rounded_size)
{ MockPlatform platform; const Segments segments(platform); Allocation allocation(platform, segments.segments(), shadow, 0x1000u);
  CHECK(allocation.descriptor().kind() == AllocationKind::shadow);
  CHECK(allocation.size() == 0x1000u);
  CHECK_EQ(allocation.opens(), 0u);
  CHECK(allocation.invariants_hold()); }

TEST(allocation_report_to, tells_vidmm_the_size_segment_and_cpu_visibility)
{ MockPlatform platform; Segments segments(platform); Allocation allocation(platform, segments.segments(), shadow, 0x1000u);
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
  Segments segments(platform); Allocation allocation(platform, segments.segments(), texture, 0x1000u);
  DxgkAllocationInfoRecord info = {};
  allocation.report_to(info);
  CHECK_EQ(info.Flags, 0u); }

TEST(allocation_open, counts_opens_and_refuses_a_close_with_none)
{ MockPlatform platform; Segments segments(platform); Allocation allocation(platform, segments.segments(), shadow, 0x1000u);
  CHECK_STATUS(allocation.close(), Status::invalid_state);
  CHECK_STATUS(allocation.open(&platform), Status::ok);
  CHECK(allocation.invariants_hold());
  CHECK_STATUS(allocation.open(&platform), Status::ok);
  CHECK_EQ(allocation.opens(), 2u);
  CHECK_STATUS(allocation.close(), Status::ok);
  CHECK(allocation.invariants_hold());
  CHECK_EQ(allocation.opens(), 1u); }

TEST(allocation, is_legal_up_to_apc)
{ MockPlatform platform; Segments segments(platform); Allocation allocation(platform, segments.segments(), shadow, 0x1000u); DxgkAllocationInfoRecord info = {};
  platform.set_irql(Irql::dispatch);
  allocation.report_to(info);
  static_cast<void>(allocation.open(&platform));
  static_cast<void>(allocation.close());
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  while (platform.claim(Violation::irql_ceiling)) {} }

TEST(allocation_invariants, a_size_below_the_descriptor_or_off_a_page_is_detected)
{ MockPlatform platform; Segments segments(platform); Allocation allocation(platform, segments.segments(), shadow, 0x1000u);
  AllocationProbe probe(allocation);
  probe.set_size(0x800u);
  CHECK(!allocation.invariants_hold());
  probe.set_size(0x1001u);
  CHECK(!allocation.invariants_hold());
  probe.set_size(0u);
  CHECK(!allocation.invariants_hold());
  const AllocationDescriptor large(AllocationKind::client, false, SurfaceLayout(), 0x1800u, ScanoutTarget());
  Allocation undersized(platform, segments.segments(), large, 0x2000u);
  AllocationProbe(undersized).set_size(0x1000u);
  CHECK(!undersized.invariants_hold()); }

/* An aperture address means nothing to the device; these are the pages behind
 * it, and an address of a different allocation's is not this one's. */
TEST(allocation_pages_at, answers_the_pages_the_aperture_mapping_recorded)
{ MockPlatform platform; Segments segments(platform); Allocation allocation(platform, segments.segments(), shadow, 2u * page);
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
{ MockPlatform platform; Segments segments(platform); Allocation allocation(platform, segments.segments(), shadow, 2u * page);
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
{ MockPlatform platform; Segments segments(platform); Allocation allocation(platform, segments.segments(), shadow, page);
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
  Segments segments(platform);
  Allocation scanned_out(platform, segments.segments(), primary, 64u * 256u);
  DxgkAllocationInfoRecord info = {};
  scanned_out.report_to(info);
  CHECK_EQ(info.PreferredSegment, av::ApertureSegment::segment_id);
  CHECK_EQ(info.SupportedReadSegmentSet, av::ApertureSegment::segment_set);
  CHECK_EQ(info.SupportedWriteSegmentSet, av::ApertureSegment::segment_set);
  Allocation other(platform, segments.segments(), shadow, 0x1000u);
  DxgkAllocationInfoRecord plain = {};
  other.report_to(plain);
  CHECK_EQ(plain.PreferredSegment, 2u);   /* VidMm's own, after the one segment this adapter reports */
  CHECK_EQ(plain.SupportedReadSegmentSet, 2u); }

/* The aperture pages are part of the allocation's own invariant. */
TEST(allocation_invariants, aperture_pages_that_lost_theirs_are_detected)
{ MockPlatform platform; Segments segments(platform); Allocation allocation(platform, segments.segments(), shadow, 0x1000u);
  CHECK(allocation.invariants_hold());
  AllocationProbe(allocation).break_aperture();
  CHECK(!allocation.invariants_hold()); }

/* A primary is read by the display, which has no BAR to read it through: it
 * stays in the aperture whatever the device offers. */
TEST(allocation_report_to, a_primary_stays_in_the_aperture_where_a_region_exists)
{ MockPlatform platform;
  Segments segments(platform);
  segments.offer_host_visible(platform, 64u << 20u);
  const AllocationDescriptor primary(AllocationKind::shared_primary, true,
                                     SurfaceLayout(64u, 64u, 21u, 256u), 64u * 256u, ScanoutTarget(0u, 60u, 1u));
  Allocation scanned_out(platform, segments.segments(), primary, 64u * 256u);
  DxgkAllocationInfoRecord info = {};
  scanned_out.report_to(info);
  CHECK_EQ(info.PreferredSegment, av::ApertureSegment::segment_id);
  CHECK_EQ(info.Flags, 0x8001u);   /* CpuVisible | AccessedPhysically */ }

/* Feature 041, US2: the pages an rvk context runs on belong to the device that
 * made them. A second device asking for the same ring is asking for another
 * process's submissions, and is refused; anything else is shared memory and is
 * not. */
TEST(allocation_open, a_ring_belongs_to_the_device_that_opened_it)
{ MockPlatform platform;
  Segments segments(platform);
  int first_device = 0;
  int second_device = 0;
  const AllocationDescriptor ring(AllocationKind::context_ring, true, SurfaceLayout(), 0x4000u, ScanoutTarget());
  Allocation allocation(platform, segments.segments(), ring, 0x4000u);
  CHECK_STATUS(allocation.open(&first_device), Status::ok);
  CHECK_STATUS(allocation.open(&second_device), Status::invalid_parameter);
  CHECK_STATUS(allocation.open(&first_device), Status::ok);   /* the same device, twice */
  CHECK(allocation.invariants_hold());

  /* Once nobody holds it, it is anybody's again: the handle is gone and what
   * is left is memory. */
  CHECK_STATUS(allocation.close(), Status::ok);
  CHECK_STATUS(allocation.close(), Status::ok);
  CHECK_STATUS(allocation.open(&second_device), Status::ok);
  CHECK(allocation.invariants_hold());
  CHECK_STATUS(allocation.close(), Status::ok); }

TEST(allocation_open, a_fence_page_belongs_to_its_device_and_a_shadow_does_not)
{ MockPlatform platform;
  Segments segments(platform);
  int first_device = 0;
  int second_device = 0;
  const AllocationDescriptor page_of_fences(AllocationKind::fence_page, true, SurfaceLayout(), 0x1000u,
                                            ScanoutTarget());
  Allocation fences(platform, segments.segments(), page_of_fences, 0x1000u);
  CHECK_STATUS(fences.open(&first_device), Status::ok);
  CHECK_STATUS(fences.open(&second_device), Status::invalid_parameter);

  Allocation shared(platform, segments.segments(), shadow, 0x1000u);
  CHECK_STATUS(shared.open(&first_device), Status::ok);
  CHECK_STATUS(shared.open(&second_device), Status::ok);
  CHECK(shared.invariants_hold()); }

/* An allocation that is open with nobody holding it, or held by somebody with
 * no opens, is a handle this driver has lost track of. */
TEST(allocation_invariants, an_open_with_no_holder_is_detected)
{ MockPlatform platform;
  Segments segments(platform);
  int device = 0;
  Allocation allocation(platform, segments.segments(), shadow, 0x1000u);
  AllocationProbe(allocation).set_opens(1u);
  CHECK(!allocation.invariants_hold());
  AllocationProbe(allocation).set_opens(0u);
  CHECK(allocation.invariants_hold());
  CHECK_STATUS(allocation.open(&device), Status::ok);
  AllocationProbe(allocation).set_opens(0u);
  CHECK(!allocation.invariants_hold());
  AllocationProbe(allocation).set_opens(1u);
  CHECK_STATUS(allocation.close(), Status::ok); }

/* Feature 041: the CPU-visible segment is reported, and nothing is asked for in
 * it yet. Three live runs said VidMm refuses every allocation whose
 * PreferredSegment is that segment, with STATUS_INVALID_PARAMETER at
 * CreateAllocation - with and without a system-memory fallback in the supported
 * sets, with and without an eviction segment, and with and without CacheCoherent
 * on the segment - while reporting the segment itself is fine and the desktop
 * runs from the aperture. What VidMm wants of an allocation in a CPU-visible
 * segment that has storage is feature 043's to find out, with a client that
 * asks for the memory. */
TEST(allocation_report_to, nothing_is_asked_for_in_host_memory_yet)
{ MockPlatform platform;
  Segments segments(platform);
  segments.offer_host_visible(platform, 64u << 20u);
  const AllocationDescriptor client(AllocationKind::client, true, SurfaceLayout(), 0x1000u, ScanoutTarget());
  const AllocationDescriptor ring(AllocationKind::context_ring, true, SurfaceLayout(), 0x4000u, ScanoutTarget());
  const AllocationDescriptor asked[] = {client};
  for (const AllocationDescriptor &descriptor : asked) {
      Allocation allocation(platform, segments.segments(), descriptor, descriptor.size());
      DxgkAllocationInfoRecord info = {};
      allocation.report_to(info);
      /* VidMm's own system memory, which is the third segment once this adapter
       * reports two. */
      CHECK_EQ(info.PreferredSegment, 3u);
      CHECK_EQ(info.SupportedReadSegmentSet, 0x4u);
      CHECK_EQ(info.EvictionSegmentSet, 0u);
      CHECK_EQ(info.Flags, 1u);   /* still CPU-visible, and still guest-backed */
  } }

/* Feature 043: a context's ring, fence page and reply slot are published to the
 * device as guest blobs, and the only pages the driver ever learns are the ones
 * VidMm maps into the aperture. So each is pinned there like a primary, and a
 * client's buffer is not. */
TEST(allocation_report_to, the_transport_s_memory_is_pinned_in_the_aperture)
{ MockPlatform platform;
  Segments segments(platform);
  segments.offer_host_visible(platform, 64u << 20u);
  const AllocationDescriptor transport[] = {
      AllocationDescriptor(AllocationKind::context_ring, true, SurfaceLayout(), 0x4000u, ScanoutTarget()),
      AllocationDescriptor(AllocationKind::fence_page, true, SurfaceLayout(), 0x1000u, ScanoutTarget()),
      AllocationDescriptor(AllocationKind::reply_slot, true, SurfaceLayout(), 0x1000u, ScanoutTarget()),
      /* Feature 045: a host-visible chunk is published the same way, and the host GPU imports it. */
      AllocationDescriptor(AllocationKind::host_visible_memory, true, SurfaceLayout(), 0x40000u, ScanoutTarget()),
  };
  for (const AllocationDescriptor &descriptor : transport) {
      Allocation allocation(platform, segments.segments(), descriptor, descriptor.size());
      DxgkAllocationInfoRecord info = {};
      allocation.report_to(info);
      CHECK_EQ(info.PreferredSegment, av::ApertureSegment::segment_id);
      CHECK_EQ(info.SupportedReadSegmentSet, av::ApertureSegment::segment_set);
      CHECK_EQ(info.SupportedWriteSegmentSet, av::ApertureSegment::segment_set);
      CHECK_EQ(info.Flags, 0x8001u);   /* CpuVisible | AccessedPhysically */
  } }

/* A chunk the host GPU imports is one process's memory: another device opening it
 * would be a second process mapping memory the first process's context imported. */
TEST(allocation_open, a_host_visible_chunk_belongs_to_the_device_that_opened_it)
{ MockPlatform platform;
  Segments segments(platform);
  int first_device = 0;
  int second_device = 0;
  const AllocationDescriptor chunk(AllocationKind::host_visible_memory, true, SurfaceLayout(), 0x40000u,
                                   ScanoutTarget());
  Allocation allocation(platform, segments.segments(), chunk, 0x40000u);
  CHECK_STATUS(allocation.open(&first_device), Status::ok);
  CHECK_STATUS(allocation.open(&second_device), Status::invalid_parameter);
  CHECK(allocation.invariants_hold()); }

TEST(allocation_open, a_reply_slot_belongs_to_the_device_that_opened_it)
{ MockPlatform platform;
  Segments segments(platform);
  int first_device = 0;
  int second_device = 0;
  const AllocationDescriptor slot(AllocationKind::reply_slot, true, SurfaceLayout(), 0x1000u, ScanoutTarget());
  Allocation allocation(platform, segments.segments(), slot, 0x1000u);
  CHECK_STATUS(allocation.open(&first_device), Status::ok);
  CHECK_STATUS(allocation.open(&second_device), Status::invalid_parameter);
  CHECK(allocation.invariants_hold()); }

/* The binding publishes the whole allocation, wherever VidMm placed it: every
 * page, in order, or nothing while any page is not mapped. */
TEST(allocation_pages, answers_every_page_only_once_all_are_mapped)
{ MockPlatform platform; Segments segments(platform);
  const AllocationDescriptor ring(AllocationKind::context_ring, true, SurfaceLayout(), 0x4000u, ScanoutTarget());
  Allocation allocation(platform, segments.segments(), ring, 4u * page);
  CHECK_EQ(allocation.pages().count(), 0u);
  const std::uint64_t frames[] = {9u * page, 3u * page, 4u * page, 5u * page};
  av::DxgkBuildPagingBufferMapApertureSegmentRecord head = {};
  head.SegmentId = av::ApertureSegment::segment_id;
  head.OffsetInPages = 10u;
  head.NumberOfPages = 2u;
  head.pMdl = platform.attach_page_frames(frames, 4u);
  CHECK_STATUS(allocation.map_aperture(head), Status::ok);
  CHECK_EQ(allocation.pages().count(), 0u);   /* half of it */
  av::DxgkBuildPagingBufferMapApertureSegmentRecord rest = head;
  rest.OffsetInPages = 12u;
  rest.MdlOffset = 2u;
  CHECK_STATUS(allocation.map_aperture(rest), Status::ok);
  const av::GuestPageList whole = allocation.pages();
  CHECK_EQ(whole.count(), 4u);
  CHECK_EQ(whole.frame(0u), 9u * page);
  CHECK_EQ(whole.frame(3u), 5u * page);
  CHECK_EQ(whole.runs(), 2u);
  av::DxgkBuildPagingBufferUnmapApertureSegmentRecord unmapping = {};
  unmapping.SegmentId = av::ApertureSegment::segment_id;
  unmapping.OffsetInPages = 13u;
  unmapping.NumberOfPages = 1u;
  allocation.unmap_aperture(unmapping);
  CHECK_EQ(allocation.pages().count(), 0u);
  CHECK(allocation.invariants_hold()); }
