#include "miniport/ddi/dxgk_allocation_info_record.hpp"
#include "miniport/ddi/dxgk_build_paging_buffer_map_aperture_segment_record.hpp"
#include "miniport/ddi/dxgk_build_paging_buffer_unmap_aperture_segment_record.hpp"
#include "miniport/memory/aperture_segment.hpp"
#include "miniport/memory/allocation.hpp"
#include "miniport/memory/resource.hpp"
#include "miniport/memory/memory_segments.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
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
    void set_opens(std::uint32_t opens) { allocation.opens_ = opens; }
    void break_aperture() { AperturePagesProbe::forget_capacity(allocation.aperture_); }
    Allocation &allocation;
};

}

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

class Segments {
public:
    explicit Segments(MockPlatform &platform) : aperture_(platform), segments_(platform, aperture_)
    { CHECK_STATUS(aperture_.initialize(0x100000000), av::Status::ok); }

    const av::MemorySegments &segments() const { return segments_; }
    void offer_local(std::uint64_t bytes)
    {
        const std::uint8_t uuid[av::GpuIdentity::uuid_bytes] = {};
        segments_.offer_local(av::CapsetInfo(
            av::ProtocolVersion(av::ProtocolVersion::current_major, av::ProtocolVersion::current_minor), 1u, 64u,
            12u, 24u, 0u, av::GpuIdentity(1u, 2u, av::GpuIdentity::discrete_gpu, uuid, bytes, 0u, "the host's")));
    }

private:
    av::ApertureSegment     aperture_;
    av::MemorySegments      segments_;
};

const AllocationDescriptor shadow(AllocationKind::shadow, true, SurfaceLayout(10u, 10u, 21u, 40u), 400u,
                                  ScanoutTarget());

constexpr std::uint64_t page = av::GuestPageList::page_bytes;

std::uint64_t aperture_address(std::uint64_t offset_in_pages)
{
    return static_cast<std::uint64_t>(av::ApertureSegment::base_address) + offset_in_pages * page;
}

}

TEST(allocation, remembers_its_descriptor_and_page_rounded_size)
{ MockPlatform platform; const Segments segments(platform); Allocation allocation(platform, segments.segments(), shadow, 0x1000u);
  CHECK(allocation.descriptor().kind() == AllocationKind::shadow);
  CHECK(allocation.size() == 0x1000u);
  CHECK_EQ(allocation.opens(), 0u);
  CHECK(allocation.invariants_hold()); }

TEST(allocation_host_key, is_the_descriptor_s_until_an_identity_is_minted)
{ MockPlatform platform; const Segments segments(platform);
  const AllocationDescriptor keyed(AllocationKind::surface, true, SurfaceLayout(10u, 10u, 21u, 40u), 400u,
                                   ScanoutTarget(), 0x51u);
  Allocation allocation(platform, segments.segments(), keyed, 0x1000u);
  CHECK_EQ(allocation.host_key(), 0x51u);
  allocation.shared_as(7u);
  CHECK_EQ(allocation.host_key(), 7u);
  Allocation plain(platform, segments.segments(), shadow, 0x1000u);
  CHECK_EQ(plain.host_key(), 0u); }

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
  CHECK_EQ(info.PreferredSegment, 2u);
  CHECK_EQ(info.SupportedReadSegmentSet, 2u);
  CHECK_EQ(info.SupportedWriteSegmentSet, 2u);
  CHECK_EQ(info.EvictionSegmentSet, 0u);
  CHECK_EQ(info.MaximumRenamingListLength, 0u);
  CHECK(info.hAllocation == &allocation);
  CHECK_EQ(info.Flags, 1u);
  CHECK_EQ(info.AllocationPriority, 0x28000000u);
  CHECK_EQ(info.Flags2, 0u);
  CHECK(info.pPrivateDriverData == &platform); }

TEST(allocation_report_to, a_gpu_only_allocation_is_not_cpu_visible)
{ MockPlatform platform;
  const AllocationDescriptor texture(AllocationKind::shadow, false, SurfaceLayout(1u, 1u, 21u, 4u), 4u, ScanoutTarget());
  Segments segments(platform); Allocation allocation(platform, segments.segments(), texture, 0x1000u);
  DxgkAllocationInfoRecord info = {};
  allocation.report_to(info);
  CHECK_EQ(info.Flags, 0u); }

TEST(allocation_report_to, a_cross_adapter_texture_is_cached_system_memory_and_an_existing_heap_is_not)
{ MockPlatform platform;
  const AllocationDescriptor lent(AllocationKind::cross_adapter, true, SurfaceLayout(1249u, 805u, 21u, 5120u),
                                  1010u * 4096u, ScanoutTarget());
  Segments segments(platform); Allocation allocation(platform, segments.segments(), lent, 1010u * 4096u);
  DxgkAllocationInfoRecord info = {};
  allocation.report_to(info);
  CHECK_EQ(info.Flags, 5u);
  CHECK_EQ(info.PreferredSegment, 2u);
  const AllocationDescriptor heap(AllocationKind::cross_adapter, true, SurfaceLayout(0x30000u, 1u, 0u, 0x30000u),
                                  0x30000u, ScanoutTarget());
  Allocation existing(platform, segments.segments(), heap, 0x30000u);
  DxgkAllocationInfoRecord heap_info = {};
  existing.report_to(heap_info);
  CHECK_EQ(heap_info.Flags, 1u); }

TEST(allocation_open, counts_opens_and_refuses_a_close_with_none)
{ MockPlatform platform; Segments segments(platform); Allocation allocation(platform, segments.segments(), shadow, 0x1000u);
  CHECK_STATUS(allocation.close(), Status::invalid_state);
  CHECK_STATUS(allocation.open(&platform, nullptr), Status::ok);
  CHECK(allocation.invariants_hold());
  CHECK_STATUS(allocation.open(&platform, nullptr), Status::ok);
  CHECK_EQ(allocation.opens(), 2u);
  CHECK_STATUS(allocation.close(), Status::ok);
  CHECK(allocation.invariants_hold());
  CHECK_EQ(allocation.opens(), 1u); }

TEST(allocation, is_legal_up_to_apc)
{ MockPlatform platform; Segments segments(platform); Allocation allocation(platform, segments.segments(), shadow, 0x1000u); DxgkAllocationInfoRecord info = {};
  platform.set_irql(Irql::dispatch);
  allocation.report_to(info);
  static_cast<void>(allocation.open(&platform, nullptr));
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
  CHECK_EQ(plain.PreferredSegment, 2u);
  CHECK_EQ(plain.SupportedReadSegmentSet, 2u); }

TEST(allocation_report_to, a_staging_surface_is_pinned_in_the_aperture_and_cached)
{ MockPlatform platform;
  Segments segments(platform);
  const AllocationDescriptor staging(AllocationKind::staging, true, SurfaceLayout(64u, 64u, 21u, 256u), 64u * 256u,
                                     ScanoutTarget());
  Allocation redirection(platform, segments.segments(), staging, 64u * 256u);
  DxgkAllocationInfoRecord info = {};
  redirection.report_to(info);
  CHECK_EQ(info.PreferredSegment, av::ApertureSegment::segment_id);
  CHECK_EQ(info.SupportedReadSegmentSet, av::ApertureSegment::segment_set);
  CHECK_EQ(info.Flags, 0x8005u);
  const AllocationDescriptor surface(AllocationKind::surface, true, SurfaceLayout(64u, 64u, 21u, 256u), 64u * 256u,
                                     ScanoutTarget());
  Allocation back_buffer(platform, segments.segments(), surface, 64u * 256u);
  DxgkAllocationInfoRecord drawn = {};
  back_buffer.report_to(drawn);
  CHECK_EQ(drawn.Flags, 0x8001u);    }

TEST(allocation_invariants, aperture_pages_that_lost_theirs_are_detected)
{ MockPlatform platform; Segments segments(platform); Allocation allocation(platform, segments.segments(), shadow, 0x1000u);
  CHECK(allocation.invariants_hold());
  AllocationProbe(allocation).break_aperture();
  CHECK(!allocation.invariants_hold()); }

TEST(allocation_report_to, a_primary_stays_in_the_aperture)
{ MockPlatform platform;
  Segments segments(platform);
  const AllocationDescriptor primary(AllocationKind::shared_primary, true,
                                     SurfaceLayout(64u, 64u, 21u, 256u), 64u * 256u, ScanoutTarget(0u, 60u, 1u));
  Allocation scanned_out(platform, segments.segments(), primary, 64u * 256u);
  DxgkAllocationInfoRecord info = {};
  scanned_out.report_to(info);
  CHECK_EQ(info.PreferredSegment, av::ApertureSegment::segment_id);
  CHECK_EQ(info.Flags, 0x8001u);    }

TEST(allocation_open, a_ring_belongs_to_the_device_that_opened_it)
{ MockPlatform platform;
  Segments segments(platform);
  int first_device = 0;
  int second_device = 0;
  const AllocationDescriptor ring(AllocationKind::context_ring, true, SurfaceLayout(), 0x1000u, ScanoutTarget());
  Allocation allocation(platform, segments.segments(), ring, 0x1000u);
  CHECK_STATUS(allocation.open(&first_device, nullptr), Status::ok);
  CHECK_STATUS(allocation.open(&second_device, nullptr), Status::invalid_parameter);
  CHECK_STATUS(allocation.open(&first_device, nullptr), Status::ok);
  CHECK(allocation.invariants_hold());

  CHECK_STATUS(allocation.close(), Status::ok);
  CHECK_STATUS(allocation.close(), Status::ok);
  CHECK_STATUS(allocation.open(&second_device, nullptr), Status::ok);
  CHECK(allocation.invariants_hold());
  CHECK_STATUS(allocation.close(), Status::ok); }

TEST(allocation_open, rings_are_held_together_only_by_one_device_or_by_none)
{ MockPlatform platform;
  Segments segments(platform);
  int first_device = 0;
  int second_device = 0;
  const AllocationDescriptor ring(AllocationKind::context_ring, true, SurfaceLayout(), 0x1000u, ScanoutTarget());
  Allocation first(platform, segments.segments(), ring, 0x1000u);
  Allocation second(platform, segments.segments(), ring, 0x1000u);
  CHECK(first.held_with(second));
  CHECK_STATUS(first.open(&first_device, nullptr), Status::ok);
  CHECK(!first.held_with(second));
  CHECK(!second.held_with(first));
  CHECK_STATUS(second.open(&second_device, nullptr), Status::ok);
  CHECK(!first.held_with(second));
  CHECK_STATUS(second.close(), Status::ok);
  CHECK_STATUS(second.open(&first_device, nullptr), Status::ok);
  CHECK(first.held_with(second));
  CHECK(first.held_with(first));
  CHECK_STATUS(first.close(), Status::ok);
  CHECK_STATUS(second.close(), Status::ok);
  CHECK(first.held_with(second)); }

TEST(allocation_open, rings_are_held_in_one_process_only_when_both_are_held_there)
{ MockPlatform platform;
  Segments segments(platform);
  int gl_device = 0;
  int cl_device = 0;
  int process = 0;
  int other_process = 0;
  const AllocationDescriptor ring(AllocationKind::context_ring, true, SurfaceLayout(), 0x1000u, ScanoutTarget());
  Allocation gl(platform, segments.segments(), ring, 0x1000u);
  Allocation cl(platform, segments.segments(), ring, 0x1000u);
  CHECK(!gl.held_in_process_of(cl));
  CHECK_STATUS(gl.open(&gl_device, &process), Status::ok);
  CHECK(!gl.held_in_process_of(cl));
  CHECK(!cl.held_in_process_of(gl));
  CHECK_STATUS(cl.open(&cl_device, &process), Status::ok);
  CHECK(gl.held_in_process_of(cl));
  CHECK(cl.held_in_process_of(gl));
  CHECK(!gl.held_with(cl));
  CHECK_STATUS(cl.close(), Status::ok);
  CHECK_STATUS(cl.open(&cl_device, &other_process), Status::ok);
  CHECK(!gl.held_in_process_of(cl));
  CHECK_STATUS(gl.close(), Status::ok);
  CHECK_STATUS(cl.close(), Status::ok);
  CHECK(!gl.held_in_process_of(cl));
  CHECK(gl.invariants_hold()); }

TEST(allocation_open, a_shadow_is_opened_by_any_device)
{ MockPlatform platform;
  Segments segments(platform);
  int first_device = 0;
  int second_device = 0;
  Allocation shared(platform, segments.segments(), shadow, 0x1000u);
  CHECK_STATUS(shared.open(&first_device, nullptr), Status::ok);
  CHECK_STATUS(shared.open(&second_device, nullptr), Status::ok);
  CHECK(shared.invariants_hold()); }

TEST(allocation_invariants, an_open_with_no_holder_is_detected)
{ MockPlatform platform;
  Segments segments(platform);
  int device = 0;
  Allocation allocation(platform, segments.segments(), shadow, 0x1000u);
  AllocationProbe(allocation).set_opens(1u);
  CHECK(!allocation.invariants_hold());
  AllocationProbe(allocation).set_opens(0u);
  CHECK(allocation.invariants_hold());
  CHECK_STATUS(allocation.open(&device, nullptr), Status::ok);
  AllocationProbe(allocation).set_opens(0u);
  CHECK(!allocation.invariants_hold());
  AllocationProbe(allocation).set_opens(1u);
  CHECK_STATUS(allocation.close(), Status::ok); }

TEST(allocation_report_to, a_client_buffer_lives_in_vidmm_s_own_system_memory)
{ MockPlatform platform;
  Segments segments(platform);
  const AllocationDescriptor client(AllocationKind::client, true, SurfaceLayout(), 0x1000u, ScanoutTarget());
  const AllocationDescriptor ring(AllocationKind::context_ring, true, SurfaceLayout(), 0x1000u, ScanoutTarget());
  const AllocationDescriptor asked[] = {client, ring};
  for (const AllocationDescriptor &descriptor : asked) {
      Allocation allocation(platform, segments.segments(), descriptor, descriptor.size());
      DxgkAllocationInfoRecord info = {};
      allocation.report_to(info);
      CHECK_EQ(info.PreferredSegment, 2u);
      CHECK_EQ(info.SupportedReadSegmentSet, 0x2u);
      CHECK_EQ(info.SupportedWriteSegmentSet, 0x2u);
      CHECK_EQ(info.EvictionSegmentSet, 0u);
      CHECK_EQ(info.Flags, 1u);
  } }

TEST(allocation_report_to, a_client_buffer_follows_vidmm_s_system_memory_past_the_local_segment)
{ MockPlatform platform;
  Segments segments(platform);
  segments.offer_local(std::uint64_t{16} << 30u);
  const AllocationDescriptor client(AllocationKind::client, true, SurfaceLayout(), 0x1000u, ScanoutTarget());
  Allocation allocation(platform, segments.segments(), client, client.size());
  DxgkAllocationInfoRecord info = {};
  allocation.report_to(info);
  CHECK_EQ(info.PreferredSegment, 3u);
  CHECK_EQ(info.SupportedReadSegmentSet, 0x4u);
  CHECK_EQ(info.SupportedWriteSegmentSet, 0x4u);

  const AllocationDescriptor ring(AllocationKind::context_ring, true, SurfaceLayout(), 0x1000u, ScanoutTarget());
  Allocation name(platform, segments.segments(), ring, ring.size());
  DxgkAllocationInfoRecord name_info = {};
  name.report_to(name_info);
  CHECK_EQ(name_info.PreferredSegment, 3u);
  CHECK_EQ(name_info.SupportedReadSegmentSet, 0x4u); }

TEST(allocation_report_to, a_context_s_name_is_not_pinned_in_the_aperture)
{ MockPlatform platform;
  Segments segments(platform);
  const AllocationDescriptor ring(AllocationKind::context_ring, true, SurfaceLayout(), 0x1000u, ScanoutTarget());
  Allocation allocation(platform, segments.segments(), ring, ring.size());
  DxgkAllocationInfoRecord info = {};
  allocation.report_to(info);
  CHECK_EQ(info.PreferredSegment, 2u);
  CHECK_EQ(info.SupportedReadSegmentSet, 0x2u);
  CHECK_EQ(info.SupportedWriteSegmentSet, 0x2u);
  CHECK_EQ(info.Flags, 1u);    }

TEST(allocation_pages, answers_every_page_only_once_all_are_mapped)
{ MockPlatform platform; Segments segments(platform);
  const AllocationDescriptor client(AllocationKind::client, true, SurfaceLayout(), 0x4000u, ScanoutTarget());
  Allocation allocation(platform, segments.segments(), client, 4u * page);
  CHECK_EQ(allocation.pages().count(), 0u);
  const std::uint64_t frames[] = {9u * page, 3u * page, 4u * page, 5u * page};
  av::DxgkBuildPagingBufferMapApertureSegmentRecord head = {};
  head.SegmentId = av::ApertureSegment::segment_id;
  head.OffsetInPages = 10u;
  head.NumberOfPages = 2u;
  head.pMdl = platform.attach_page_frames(frames, 4u);
  CHECK_STATUS(allocation.map_aperture(head), Status::ok);
  CHECK_EQ(allocation.pages().count(), 0u);
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

TEST(allocation, carries_no_shared_identity_until_the_adapter_mints_one)
{ MockPlatform platform;
  Segments     segments(platform);
  Allocation   allocation(platform, segments.segments(), shadow, 0x1000u);
  CHECK_EQ(allocation.shared_id(), 0u);
  CHECK(allocation.invariants_hold());
  allocation.shared_as(0x2A00000000000001u);
  CHECK_EQ(allocation.shared_id(), 0x2A00000000000001u);
  CHECK(allocation.invariants_hold()); }

TEST(allocation_open, a_shared_allocation_takes_a_second_device_where_a_ring_would_not)
{ MockPlatform platform;
  Segments     segments(platform);
  Allocation   allocation(platform, segments.segments(), shadow, 0x1000u);
  allocation.shared_as(9u);
  int first = 0;
  int second = 0;
  CHECK_STATUS(allocation.open(&first, nullptr), Status::ok);
  CHECK_STATUS(allocation.open(&second, nullptr), Status::ok);
  CHECK_EQ(allocation.opens(), 2u);
  CHECK(allocation.invariants_hold());
  CHECK_STATUS(allocation.close(), Status::ok);
  CHECK_STATUS(allocation.close(), Status::ok);
  CHECK(allocation.invariants_hold()); }

TEST(allocation, belongs_to_the_resource_it_was_created_into_and_to_none_by_default)
{ MockPlatform platform;
  Segments     segments(platform);
  const AllocationDescriptor surface(AllocationKind::shadow, true, SurfaceLayout(16u, 16u, 21u, 64u), 0x1000u,
                                    ScanoutTarget());
  Allocation   allocation(platform, segments.segments(), surface, 0x1000u);
  CHECK(allocation.resource() == nullptr);
  av::Resource resource(platform);
  allocation.belongs_to(&resource);
  CHECK(allocation.resource() == &resource);
  allocation.belongs_to(nullptr);
  CHECK(allocation.resource() == nullptr); }

TEST(allocation, counts_the_presents_written_into_its_pages_apart_from_every_other_allocation_s)
{ MockPlatform platform;
  Segments     segments(platform);
  Allocation   allocation(platform, segments.segments(), shadow, 0x1000u);
  Allocation   other(platform, segments.segments(), shadow, 0x1000u);
  CHECK_EQ(allocation.writes().sequence(), 0u);
  CHECK_EQ(allocation.writes().ticket(), 0u);
  allocation.writes().opening();
  allocation.writes().written(4u);
  CHECK_EQ(allocation.writes().sequence(), 2u);
  CHECK_EQ(allocation.writes().ticket(), 4u);
  CHECK_EQ(other.writes().sequence(), 0u);
  CHECK(allocation.invariants_hold()); }
