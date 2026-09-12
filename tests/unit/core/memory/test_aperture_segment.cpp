#include "core/ddi/dxgk_query_segment_out4_record.hpp"
#include "core/ddi/dxgk_segment_descriptor4_record.hpp"
#include "core/ddi/dxgkarg_query_adapter_info_record.hpp"
#include "core/memory/aperture_segment.hpp"
#include "core/miniport/adapter_info_query.hpp"
#include "core/support/status.hpp"
#include "harness/framework/guarded_buffer.hpp"
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

using av::AdapterInfoQuery;
using av::ApertureSegment;
using av::ApertureSegmentProbe;
using av::DxgkQuerySegmentOut4Record;
using av::DxgkSegmentDescriptor4Record;
using av::DxgkargQueryAdapterInfoRecord;
using av::GuardedBuffer;
using av::Irql;
using av::MockPlatform;
using av::Status;
using av::Violation;

namespace {

constexpr std::uint64_t gib = std::uint64_t{1} << 30u;
constexpr std::uint32_t vidmm_stride = 0x68u;   /* CreatePhysicalAdapterSegments */

class SegmentQuery {
public:
    SegmentQuery() : index_(0u), out_{}, record_{}
    {
        record_.Type = 11;
        record_.pInputData = &index_;
        record_.InputDataSize = sizeof index_;
        record_.pOutputData = &out_;
        record_.OutputDataSize = sizeof out_;
    }

    Status ask(MockPlatform &platform, const ApertureSegment &segment)
    { return segment.report(AdapterInfoQuery(platform, record_)); }

    std::uint32_t                 &index() { return index_; }
    DxgkQuerySegmentOut4Record    &out() { return out_; }
    DxgkargQueryAdapterInfoRecord &record() { return record_; }

private:
    std::uint32_t                 index_;
    DxgkQuerySegmentOut4Record    out_;
    DxgkargQueryAdapterInfoRecord record_;
};

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

TEST(aperture_segment_report, the_first_pass_counts_one_segment)
{ MockPlatform platform; ApertureSegment segment(platform); SegmentQuery query;
  CHECK_STATUS(segment.initialize(static_cast<std::int64_t>(8u * gib)), Status::ok);
  CHECK_STATUS(query.ask(platform, segment), Status::ok);
  CHECK_EQ(query.out().NbSegment, 1u); }

TEST(aperture_segment_report, the_second_pass_describes_guest_system_memory_as_an_aperture)
{ MockPlatform platform; ApertureSegment segment(platform); SegmentQuery query;
  CHECK_STATUS(segment.initialize(static_cast<std::int64_t>(8u * gib)), Status::ok);
  GuardedBuffer descriptors(vidmm_stride);
  std::memset(descriptors.data(), 0, vidmm_stride);
  query.out().NbSegment = 1u;
  query.out().pSegmentDescriptor = descriptors.data();
  query.out().SegmentDescriptorStride = vidmm_stride;
  CHECK_STATUS(query.ask(platform, segment), Status::ok);
  DxgkSegmentDescriptor4Record descriptor = {};
  std::memcpy(&descriptor, descriptors.data(), sizeof descriptor);
  CHECK_EQ(descriptor.Flags, 0x15u);   /* Aperture | CpuVisible | CacheCoherent */
  CHECK(descriptor.BaseAddress == 0xC0000000);
  CHECK(descriptor.Size == 8u * gib);
  CHECK(descriptor.CommitLimit == 8u * gib);
  CHECK(descriptor.CpuTranslatedAddress == 0);
  CHECK_EQ(query.out().NbSegment, 1u);
  CHECK_EQ(query.out().PagingBufferSegmentId, av::ApertureSegment::segment_id);
  CHECK_EQ(query.out().PagingBufferSize, 0x1000u);
  CHECK_EQ(query.out().PagingBufferPrivateDataSize, 0u);
  CHECK(descriptors.guards_intact()); }

TEST(aperture_segment_report, refuses_a_malformed_request_and_writes_nothing)
{ MockPlatform platform; ApertureSegment segment(platform);
  CHECK_STATUS(segment.initialize(static_cast<std::int64_t>(gib)), Status::ok);
  std::uint8_t descriptors[vidmm_stride];
  struct Case { int knob; };
  for (int knob = 0; knob < 5; knob++) {
      SegmentQuery query;
      std::memset(descriptors, 0xEE, sizeof descriptors);
      query.out().NbSegment = 1u;
      query.out().pSegmentDescriptor = descriptors;
      query.out().SegmentDescriptorStride = vidmm_stride;
      if (knob == 0) query.index() = 1u;                                  /* another physical adapter */
      if (knob == 1) query.out().SegmentDescriptorStride = 0x5Fu;         /* narrower than a descriptor */
      if (knob == 2) query.out().NbSegment = 0u;                          /* room for none */
      if (knob == 3) query.record().InputDataSize = 3u;                   /* no index */
      if (knob == 4) query.record().OutputDataSize = sizeof(DxgkQuerySegmentOut4Record) - 1u;
      CHECK_STATUS(query.ask(platform, segment), Status::invalid_parameter);
      CHECK_EQ(descriptors[0], 0xEEu);
  } }

TEST(aperture_segment_report, before_initialize_there_is_no_segment)
{ MockPlatform platform; ApertureSegment segment(platform); SegmentQuery query;
  CHECK_STATUS(query.ask(platform, segment), Status::invalid_parameter); }

TEST(aperture_segment, is_passive_only)
{ MockPlatform platform; ApertureSegment segment(platform); SegmentQuery query;
  platform.set_irql(Irql::dispatch);
  static_cast<void>(segment.initialize(static_cast<std::int64_t>(gib)));
  static_cast<void>(query.ask(platform, segment));
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  while (platform.claim(Violation::irql_ceiling)) {} }

TEST(aperture_segment_invariants, a_size_that_is_not_a_page_multiple_is_detected)
{ MockPlatform platform; ApertureSegment segment(platform);
  ApertureSegmentProbe(segment).set_size(4097u);
  CHECK(!segment.invariants_hold()); }
