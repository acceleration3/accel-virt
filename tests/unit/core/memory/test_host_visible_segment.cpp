/* The CPU-visible segment over the device's hostmem region: what makes it
 * offered at all, and what it tells VidMm when it is (feature 041, US1). */
#include "core/ddi/dxgk_segment_descriptor4_record.hpp"
#include "core/memory/host_visible_segment.hpp"
#include "core/pci/pci_base_addresses.hpp"
#include "core/support/status.hpp"
#include "core/virtio/virtio_pci_layout.hpp"
#include "core/virtio/virtio_region.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"

#include <cstdint>
#include <cstring>

using av::CapsetInfo;
using av::DxgkSegmentDescriptor4Record;
using av::GpuIdentity;
using av::HostVisibleSegment;
using av::Irql;
using av::MockPlatform;
using av::PciBaseAddresses;
using av::ProtocolVersion;
using av::Status;
using av::VirtioPciLayout;
using av::VirtioRegion;
using av::Violation;

namespace av {

struct HostVisibleSegmentProbe {
    /* A segment offered over a region that is not there, or not whole pages:
     * what discover() never builds, and what the invariant is there to catch. */
    static void offer_without_a_region(HostVisibleSegment &segment) { segment.offered_ = true; }
    static void offer_a_partial_page(HostVisibleSegment &segment, std::uint64_t size, std::uint64_t base)
    {
        segment.offered_ = true;
        segment.size_ = size;
        segment.base_ = base;
    }
};

}  // namespace av

namespace {

constexpr std::uint64_t bar_address = 0x4000000000ull;
constexpr std::uint64_t region_bytes = 256u << 20u;
constexpr std::uint8_t  region_bar = 4u;
constexpr std::uint8_t  uuid[GpuIdentity::uuid_bytes] = {9};

CapsetInfo capset(std::uint64_t features)
{
    return CapsetInfo(ProtocolVersion(ProtocolVersion::current_major, ProtocolVersion::current_minor),
                      1u, 64u, 12u, 24u, features,
                      GpuIdentity(1u, 2u, GpuIdentity::discrete_gpu, uuid, 0u, 0u, "the host's"));
}

/* A configuration space whose BAR 4 is where the region lives, as firmware
 * assigned it: 64-bit, prefetchable. */
class Function {
public:
    Function()
    {
        std::memset(config_, 0, sizeof config_);
        const std::uint32_t low = static_cast<std::uint32_t>(bar_address) | 0xCu;
        const std::uint32_t high = static_cast<std::uint32_t>(bar_address >> 32u);
        std::memcpy(config_ + 0x10u + 4u * region_bar, &low, 4u);
        std::memcpy(config_ + 0x14u + 4u * region_bar, &high, 4u);
    }

    PciBaseAddresses bars() const { return PciBaseAddresses(config_, sizeof config_); }

private:
    std::uint8_t config_[256];
};

VirtioPciLayout layout_with(const VirtioRegion &host_visible)
{
    const VirtioRegion structure(2u, 0u, 0x1000u);
    const VirtioRegion shared[VirtioPciLayout::max_shared_memory_regions] = {host_visible};
    const std::uint8_t ids[VirtioPciLayout::max_shared_memory_regions] = {1u};
    return VirtioPciLayout(structure, structure, structure, structure, 4u, shared, ids,
                           host_visible.present() ? 1u : 0u);
}

}  // namespace

TEST(host_visible_segment, offers_nothing_until_it_has_looked)
{ MockPlatform platform;
  HostVisibleSegment segment(platform);
  CHECK(!segment.offered());
  CHECK_EQ(segment.size(), 0u);
  CHECK(segment.invariants_hold()); }

/* The region and the capset both have to say yes: a device with the region but
 * a capset that does not claim it is a device this driver has not proved
 * anything about. */
TEST(host_visible_segment, is_offered_only_where_the_region_and_the_capset_agree)
{ MockPlatform platform;
  Function function;
  HostVisibleSegment segment(platform);
  segment.discover(function.bars(), layout_with(VirtioRegion(region_bar, 0u, region_bytes)));
  CHECK(!segment.offered());                    /* nothing has been claimed yet */
  segment.offer(capset(0u));
  CHECK(!segment.offered());
  segment.offer(capset(CapsetInfo::feature_host_visible_memory));
  CHECK(segment.offered());
  CHECK_EQ(segment.size(), region_bytes);
  CHECK_EQ(segment.base(), bar_address);
  CHECK(segment.invariants_hold()); }

TEST(host_visible_segment, a_device_with_no_region_offers_none)
{ MockPlatform platform;
  Function function;
  HostVisibleSegment segment(platform);
  segment.discover(function.bars(), layout_with(VirtioRegion()));
  segment.offer(capset(CapsetInfo::feature_host_visible_memory));
  CHECK(!segment.offered());
  CHECK_EQ(segment.size(), 0u); }

/* A region whose length is not a whole number of pages, or whose BAR firmware
 * never assigned, is not a segment: VidMm would place an allocation in the part
 * that is not there. */
TEST(host_visible_segment, a_region_that_does_not_fit_a_page_is_refused)
{ MockPlatform platform;
  Function function;
  HostVisibleSegment truncated(platform);
  truncated.discover(function.bars(), layout_with(VirtioRegion(region_bar, 0u, 0x800u)));
  truncated.offer(capset(CapsetInfo::feature_host_visible_memory));
  CHECK(!truncated.offered());

  HostVisibleSegment unassigned(platform);
  std::uint8_t empty[256] = {};
  unassigned.discover(PciBaseAddresses(empty, sizeof empty),
                      layout_with(VirtioRegion(region_bar, 0u, region_bytes)));
  unassigned.offer(capset(CapsetInfo::feature_host_visible_memory));
  CHECK(!unassigned.offered()); }

/* What VidMm is told: storage of its own, addressable by the CPU at the BAR,
 * and never an aperture - an aperture has no storage and VidMm would ask for
 * guest pages to be mapped into it. */
TEST(host_visible_segment, describes_a_cpu_visible_segment_at_the_bar)
{ MockPlatform platform;
  Function function;
  HostVisibleSegment segment(platform);
  segment.discover(function.bars(), layout_with(VirtioRegion(region_bar, 0x1000u, region_bytes)));
  segment.offer(capset(CapsetInfo::feature_host_visible_memory));
  DxgkSegmentDescriptor4Record descriptor = {};
  segment.describe(descriptor);
  CHECK_EQ(descriptor.Flags, 0x4u);             /* CpuVisible alone: a BAR is not coherent RAM */
  CHECK(descriptor.BaseAddress == 0);
  CHECK_EQ(descriptor.Size, region_bytes);
  CHECK_EQ(descriptor.CommitLimit, region_bytes);
  CHECK(descriptor.CpuTranslatedAddress == static_cast<std::int64_t>(bar_address + 0x1000u));
  CHECK_EQ(descriptor.CpuHostApertureSizeInPages, 0u);
  CHECK_EQ(descriptor.SystemMemoryEndAddress, 0u); }

/* An unoffered segment describes a segment of nothing; what keeps it out of the
 * answer VidMm gets is MemorySegments, which does not count it. */
TEST(host_visible_segment, an_unoffered_segment_describes_nothing)
{ MockPlatform platform;
  HostVisibleSegment segment(platform);
  DxgkSegmentDescriptor4Record descriptor = {};
  descriptor.Flags = 0xAAu;
  segment.describe(descriptor);
  CHECK_EQ(descriptor.Size, 0u);
  CHECK(descriptor.CpuTranslatedAddress == 0); }

/* StopDevice: the next start looks again, because the device it looks at may
 * be another one (feature 040, FR-005). */
TEST(host_visible_segment, forgetting_leaves_nothing_offered)
{ MockPlatform platform;
  Function function;
  HostVisibleSegment segment(platform);
  segment.discover(function.bars(), layout_with(VirtioRegion(region_bar, 0u, region_bytes)));
  segment.offer(capset(CapsetInfo::feature_host_visible_memory));
  segment.forget();
  CHECK(!segment.offered());
  CHECK_EQ(segment.base(), 0u);
  CHECK_EQ(segment.size(), 0u);
  CHECK(segment.invariants_hold()); }

TEST(host_visible_segment, every_call_keeps_its_irql_ceiling)
{ MockPlatform platform;
  Function function;
  HostVisibleSegment segment(platform);
  platform.set_irql(Irql::dispatch);
  segment.discover(function.bars(), layout_with(VirtioRegion(region_bar, 0u, region_bytes)));
  CHECK(platform.claim(Violation::irql_ceiling));
  segment.offer(capset(CapsetInfo::feature_host_visible_memory));
  CHECK(platform.claim(Violation::irql_ceiling));
  platform.set_irql(Irql::passive); }

/* A segment offered over a region that is not whole pages, or over none at all,
 * would have VidMm place allocations in memory that is not there. */
TEST(host_visible_segment_invariants, an_offer_without_a_whole_region_is_caught)
{ MockPlatform platform;
  HostVisibleSegment nothing(platform);
  av::HostVisibleSegmentProbe::offer_without_a_region(nothing);
  CHECK(!nothing.invariants_hold());

  HostVisibleSegment partial(platform);
  av::HostVisibleSegmentProbe::offer_a_partial_page(partial, region_bytes + 1u, bar_address);
  CHECK(!partial.invariants_hold());

  HostVisibleSegment unaligned(platform);
  av::HostVisibleSegmentProbe::offer_a_partial_page(unaligned, region_bytes, bar_address + 1u);
  CHECK(!unaligned.invariants_hold());

  HostVisibleSegment whole(platform);
  av::HostVisibleSegmentProbe::offer_a_partial_page(whole, region_bytes, bar_address);
  CHECK(whole.invariants_hold()); }
