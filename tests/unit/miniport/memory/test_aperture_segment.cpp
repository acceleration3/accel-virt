#include "miniport/ddi/dxgk_segment_descriptor4_record.hpp"
#include "miniport/memory/aperture_segment.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>
#include <cstring>

namespace av {

struct ApertureSegmentProbe {
    explicit ApertureSegmentProbe(ApertureSegment &target) : segment(target) {}
    void set_size(std::uint64_t size) { segment.size_ = size; }
    ApertureSegment &segment;
};

}  // namespace av

using av::ApertureSegment;
using av::ApertureSegmentProbe;
using av::DxgkSegmentDescriptor4Record;
using av::Irql;
using av::MockPlatform;
using av::Status;
using av::Violation;

namespace {

constexpr std::uint64_t gib = std::uint64_t{1} << 30u;

}  // namespace

TEST(aperture_segment_initialize, rounds_system_memory_down_to_a_page)
{ MockPlatform platform; ApertureSegment segment(platform);
  CHECK_STATUS(segment.initialize(static_cast<std::int64_t>(gib + 123u)), Status::ok);
  CHECK(segment.invariants_hold());
  CHECK(segment.size() == gib); }

TEST(aperture_segment_initialize, refuses_less_than_a_page)
{ MockPlatform platform; ApertureSegment segment(platform);
  CHECK_STATUS(segment.initialize(4095), Status::invalid_parameter);
  CHECK_STATUS(segment.initialize(-4096), Status::invalid_parameter);
  CHECK(segment.size() == 0u);
  CHECK(segment.invariants_hold()); }

/* The descriptor VidMm is given for this segment; what answers
 * DXGKQAITYPE_QUERYSEGMENT4 with it is MemorySegments, which knows how many
 * segments the adapter has. */
TEST(aperture_segment_describe, is_guest_system_memory_as_an_aperture)
{ MockPlatform platform; ApertureSegment segment(platform);
  CHECK_STATUS(segment.initialize(static_cast<std::int64_t>(8u * gib)), Status::ok);
  DxgkSegmentDescriptor4Record descriptor = {};
  segment.describe(descriptor);
  CHECK_EQ(descriptor.Flags, 0x415u);   /* Aperture | CpuVisible | CacheCoherent | DirectFlip */
  CHECK(descriptor.BaseAddress == 0xC0000000);
  CHECK(descriptor.Size == 8u * gib);
  CHECK(descriptor.CommitLimit == 8u * gib);
  CHECK(descriptor.CpuTranslatedAddress == 0);
  CHECK_EQ(descriptor.SystemMemoryEndAddress, 0u); }

/* Before initialize the segment has no size, and MemorySegments is what refuses
 * to report one: what this describes is a segment of nothing. */
TEST(aperture_segment_describe, before_initialize_it_describes_an_empty_segment)
{ MockPlatform platform; ApertureSegment segment(platform);
  DxgkSegmentDescriptor4Record descriptor = {};
  descriptor.Flags = 0xAAu;
  segment.describe(descriptor);
  CHECK_EQ(descriptor.Size, 0u);
  CHECK_EQ(descriptor.CommitLimit, 0u); }

TEST(aperture_segment, is_passive_only)
{ MockPlatform platform; ApertureSegment segment(platform);
  platform.set_irql(Irql::dispatch);
  static_cast<void>(segment.initialize(static_cast<std::int64_t>(gib)));
  DxgkSegmentDescriptor4Record descriptor = {};
  segment.describe(descriptor);
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  while (platform.claim(Violation::irql_ceiling)) {} }

TEST(aperture_segment_invariants, a_size_that_is_not_a_page_multiple_is_detected)
{ MockPlatform platform; ApertureSegment segment(platform);
  ApertureSegmentProbe(segment).set_size(4097u);
  CHECK(!segment.invariants_hold()); }
