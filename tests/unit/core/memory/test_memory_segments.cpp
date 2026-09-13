/* What the adapter tells VidMm it has: the aperture over guest system memory,
 * and - where the device offers it - the CPU-visible segment over the hostmem
 * region (feature 041, US1). */
#include "core/ddi/dxgk_query_segment_out4_record.hpp"
#include "core/ddi/dxgk_segment_descriptor4_record.hpp"
#include "core/ddi/dxgkarg_query_adapter_info_record.hpp"
#include "core/memory/aperture_segment.hpp"
#include "core/memory/host_visible_segment.hpp"
#include "core/memory/memory_segments.hpp"
#include "core/miniport/adapter_info_query.hpp"
#include "core/pci/pci_base_addresses.hpp"
#include "core/support/status.hpp"
#include "core/virtio/virtio_pci_layout.hpp"
#include "core/virtio/virtio_region.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"

#include <cstdint>
#include <cstring>

using av::AdapterInfoQuery;
using av::ApertureSegment;
using av::CapsetInfo;
using av::DxgkQuerySegmentOut4Record;
using av::DxgkSegmentDescriptor4Record;
using av::DxgkargQueryAdapterInfoRecord;
using av::GpuIdentity;
using av::GuardedBuffer;
using av::HostVisibleSegment;
using av::Irql;
using av::MemorySegments;
using av::MockPlatform;
using av::PciBaseAddresses;
using av::ProtocolVersion;
using av::Status;
using av::VirtioPciLayout;
using av::VirtioRegion;
using av::Violation;

namespace {

constexpr std::uint64_t gib = std::uint64_t{1} << 30u;
constexpr std::uint64_t bar_address = 0x4000000000ull;
constexpr std::uint64_t region_bytes = 256u << 20u;
constexpr std::uint8_t  region_bar = 4u;
constexpr std::uint32_t vidmm_stride = 0x68u;   /* CreatePhysicalAdapterSegments */
constexpr std::uint8_t  uuid[GpuIdentity::uuid_bytes] = {9};

/* An adapter whose device offers the region, and one whose device does not. */
class Fixture {
public:
    explicit Fixture(bool with_region) : aperture_(platform_), host_visible_(platform_),
        segments_(platform_, aperture_, host_visible_)
    {
        CHECK_STATUS(aperture_.initialize(static_cast<std::int64_t>(4u * gib)), Status::ok);
        std::memset(config_, 0, sizeof config_);
        const std::uint32_t low = static_cast<std::uint32_t>(bar_address) | 0xCu;
        const std::uint32_t high = static_cast<std::uint32_t>(bar_address >> 32u);
        std::memcpy(config_ + 0x10u + 4u * region_bar, &low, 4u);
        std::memcpy(config_ + 0x14u + 4u * region_bar, &high, 4u);
        const VirtioRegion structure(2u, 0u, 0x1000u);
        const VirtioRegion region = with_region ? VirtioRegion(region_bar, 0u, region_bytes) : VirtioRegion();
        const VirtioRegion shared[VirtioPciLayout::max_shared_memory_regions] = {region};
        const std::uint8_t ids[VirtioPciLayout::max_shared_memory_regions] = {1u};
        const VirtioPciLayout layout(structure, structure, structure, structure, 4u, shared, ids,
                                     with_region ? 1u : 0u);
        host_visible_.discover(PciBaseAddresses(config_, sizeof config_), layout);
        host_visible_.offer(CapsetInfo(ProtocolVersion(ProtocolVersion::current_major,
                                                       ProtocolVersion::current_minor),
                                       1u, 64u, 12u, 24u, CapsetInfo::feature_host_visible_memory,
                                       GpuIdentity(1u, 2u, GpuIdentity::discrete_gpu, uuid, 0u, 0u, "host")));
    }

    MockPlatform   &platform() { return platform_; }
    MemorySegments &segments() { return segments_; }

private:
    MockPlatform       platform_;
    ApertureSegment    aperture_;
    HostVisibleSegment host_visible_;
    MemorySegments     segments_;
    std::uint8_t       config_[256];
};

class SegmentQuery {
public:
    SegmentQuery() : index_(0u), out_{}, record_{}
    {
        record_.Type = 11;   /* DXGKQAITYPE_QUERYSEGMENT4 */
        record_.pInputData = &index_;
        record_.InputDataSize = sizeof index_;
        record_.pOutputData = &out_;
        record_.OutputDataSize = sizeof out_;
    }

    Status ask(MockPlatform &platform, const MemorySegments &segments)
    { return segments.report(AdapterInfoQuery(platform, record_)); }

    std::uint32_t                 &index() { return index_; }
    DxgkQuerySegmentOut4Record    &out() { return out_; }
    DxgkargQueryAdapterInfoRecord &record() { return record_; }

private:
    std::uint32_t                 index_;
    DxgkQuerySegmentOut4Record    out_;
    DxgkargQueryAdapterInfoRecord record_;
};

}  // namespace

TEST(memory_segments_count, is_one_without_the_region_and_two_with_it)
{ Fixture bare(false);
  CHECK_EQ(bare.segments().count(), 1u);
  Fixture offered(true);
  CHECK_EQ(offered.segments().count(), 2u); }

/* VidMm appends its own system memory segment after the last one reported, so
 * the number a non-primary allocation is sent to follows the count - it was
 * only ever 2 because there was only ever one segment. */
TEST(memory_segments_system_memory_segment_id, follows_the_count)
{ Fixture bare(false);
  CHECK_EQ(bare.segments().system_memory_segment_id(), 2u);
  Fixture offered(true);
  CHECK_EQ(offered.segments().system_memory_segment_id(), 3u); }

TEST(memory_segments_report, the_first_pass_counts_what_the_adapter_has)
{ Fixture offered(true);
  SegmentQuery query;
  CHECK_STATUS(query.ask(offered.platform(), offered.segments()), Status::ok);
  CHECK_EQ(query.out().NbSegment, 2u);
  CHECK_EQ(query.out().PagingBufferSegmentId, 0u); }

TEST(memory_segments_report, describes_the_aperture_then_the_host_visible_segment)
{ Fixture offered(true);
  GuardedBuffer descriptors(2u * vidmm_stride);
  SegmentQuery query;
  query.out().pSegmentDescriptor = descriptors.data();
  query.out().NbSegment = 2u;
  query.out().SegmentDescriptorStride = vidmm_stride;
  CHECK_STATUS(query.ask(offered.platform(), offered.segments()), Status::ok);

  DxgkSegmentDescriptor4Record aperture = {};
  std::memcpy(&aperture, descriptors.data(), sizeof aperture);
  CHECK_EQ(aperture.Flags, 0x15u);              /* Aperture | CpuVisible | CacheCoherent */
  CHECK(aperture.BaseAddress == ApertureSegment::base_address);
  CHECK_EQ(aperture.Size, 4u * gib);

  DxgkSegmentDescriptor4Record host_visible = {};
  std::memcpy(&host_visible, descriptors.data() + vidmm_stride, sizeof host_visible);
  CHECK_EQ(host_visible.Flags, 0x14u);          /* CpuVisible | CacheCoherent, no Aperture */
  CHECK_EQ(host_visible.Size, region_bytes);
  CHECK(host_visible.CpuTranslatedAddress == static_cast<std::int64_t>(bar_address));

  /* The paging buffer is the aperture's, whatever else is reported. */
  CHECK_EQ(query.out().PagingBufferSegmentId, ApertureSegment::segment_id);
  CHECK_EQ(query.out().PagingBufferSize, 0x1000u);
  CHECK_EQ(query.out().PagingBufferPrivateDataSize, 0u);
  CHECK_EQ(query.out().NbSegment, 2u);
  CHECK(descriptors.guards_intact()); }

/* An adapter with no region describes one segment and writes nothing past it. */
TEST(memory_segments_report, without_the_region_only_the_aperture_is_described)
{ Fixture bare(false);
  GuardedBuffer descriptors(2u * vidmm_stride);
  SegmentQuery query;
  query.out().pSegmentDescriptor = descriptors.data();
  query.out().NbSegment = 2u;
  query.out().SegmentDescriptorStride = vidmm_stride;
  CHECK_STATUS(query.ask(bare.platform(), bare.segments()), Status::ok);
  CHECK_EQ(query.out().NbSegment, 1u);
  for (std::size_t byte = vidmm_stride; byte < 2u * vidmm_stride; byte++)
      CHECK_EQ(descriptors.data()[byte], 0u);
  CHECK(descriptors.guards_intact()); }

/* VidMm asking for fewer segments than the adapter has, or at a stride that
 * cannot hold a descriptor, is refused rather than half-answered. */
TEST(memory_segments_report, refuses_a_request_that_cannot_hold_the_answer)
{ Fixture offered(true);
  GuardedBuffer descriptors(2u * vidmm_stride);
  SegmentQuery query;
  query.out().pSegmentDescriptor = descriptors.data();
  query.out().NbSegment = 1u;
  query.out().SegmentDescriptorStride = vidmm_stride;
  CHECK_STATUS(query.ask(offered.platform(), offered.segments()), Status::invalid_parameter);

  query.out().NbSegment = 2u;
  query.out().SegmentDescriptorStride = sizeof(DxgkSegmentDescriptor4Record) - 1u;
  CHECK_STATUS(query.ask(offered.platform(), offered.segments()), Status::invalid_parameter);

  query.out().SegmentDescriptorStride = vidmm_stride;
  query.index() = 1u;   /* a physical adapter this driver does not have */
  CHECK_STATUS(query.ask(offered.platform(), offered.segments()), Status::invalid_parameter);
  CHECK(descriptors.guards_intact()); }

TEST(memory_segments_report, is_passive_only)
{ Fixture offered(true);
  SegmentQuery query;
  offered.platform().set_irql(Irql::dispatch);
  static_cast<void>(query.ask(offered.platform(), offered.segments()));
  offered.platform().set_irql(Irql::passive);
  CHECK(offered.platform().claim(Violation::irql_ceiling));
  while (offered.platform().claim(Violation::irql_ceiling)) {} }

/* Before StartDevice the adapter has no memory to describe. */
TEST(memory_segments_report, before_the_aperture_is_sized_there_is_nothing_to_report)
{ MockPlatform platform;
  ApertureSegment aperture(platform);
  HostVisibleSegment host_visible(platform);
  MemorySegments segments(platform, aperture, host_visible);
  SegmentQuery query;
  CHECK_STATUS(query.ask(platform, segments), Status::invalid_parameter); }

/* What an allocation asks before it chooses a segment: whether this adapter has
 * host memory a process can map, and how much of it (feature 041, FR-001). */
TEST(memory_segments_offers_host_visible, is_what_the_device_offered)
{ Fixture bare(false);
  CHECK(!bare.segments().offers_host_visible());
  CHECK_EQ(bare.segments().host_visible_bytes(), 0u);
  Fixture offered(true);
  CHECK(offered.segments().offers_host_visible());
  CHECK_EQ(offered.segments().host_visible_bytes(), region_bytes); }
