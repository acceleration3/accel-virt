/* What the adapter tells VidMm it has: the aperture over guest system memory,
 * and nothing else. The device's hostmem region is the driver's own host
 * window, which VidMm is never told of (docs/host-window.md). */
#include "core/ddi/dxgk_query_segment_out4_record.hpp"
#include "core/ddi/dxgk_segment_descriptor4_record.hpp"
#include "core/ddi/dxgkarg_query_adapter_info_record.hpp"
#include "core/memory/aperture_segment.hpp"
#include "core/memory/memory_segments.hpp"
#include "core/miniport/adapter_info_query.hpp"
#include "core/support/status.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>
#include <cstring>

using av::AdapterInfoQuery;
using av::ApertureSegment;
using av::DxgkQuerySegmentOut4Record;
using av::DxgkSegmentDescriptor4Record;
using av::DxgkargQueryAdapterInfoRecord;
using av::GuardedBuffer;
using av::Irql;
using av::MemorySegments;
using av::MockPlatform;
using av::Status;
using av::Violation;

namespace av {

struct ApertureSegmentProbe {
    static void set_size(ApertureSegment &segment, std::uint64_t size) { segment.size_ = size; }
};

}  // namespace av

namespace {

constexpr std::uint64_t gib = std::uint64_t{1} << 30u;
constexpr std::uint32_t vidmm_stride = 0x68u;   /* CreatePhysicalAdapterSegments */

class Fixture {
public:
    Fixture() : aperture_(platform_), segments_(platform_, aperture_)
    { CHECK_STATUS(aperture_.initialize(static_cast<std::int64_t>(4u * gib)), Status::ok); }

    /* The segment itself, for a test that has to break it. */
    ApertureSegment &aperture() { return aperture_; }
    MockPlatform    &platform() { return platform_; }
    MemorySegments  &segments() { return segments_; }

private:
    MockPlatform    platform_;
    ApertureSegment aperture_;
    MemorySegments  segments_;
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

TEST(memory_segments_count, is_the_aperture_alone)
{ Fixture fixture;
  CHECK_EQ(fixture.segments().count(), 1u); }

/* VidMm appends its own system memory segment after the last one reported, so
 * a non-primary allocation is sent to the one after the aperture. */
TEST(memory_segments_system_memory_segment_id, is_the_one_after_the_aperture)
{ Fixture fixture;
  CHECK_EQ(fixture.segments().system_memory_segment_id(), ApertureSegment::system_memory_segment_id);
  CHECK_EQ(fixture.segments().system_memory_segment_id(), 2u); }

TEST(memory_segments_report, the_first_pass_counts_what_the_adapter_has)
{ Fixture fixture;
  SegmentQuery query;
  CHECK_STATUS(query.ask(fixture.platform(), fixture.segments()), Status::ok);
  CHECK_EQ(query.out().NbSegment, 1u);
  CHECK_EQ(query.out().PagingBufferSegmentId, 0u); }

/* One descriptor, the aperture's, and nothing written past it however much room
 * VidMm offered. */
TEST(memory_segments_report, describes_the_aperture_and_nothing_past_it)
{ Fixture fixture;
  GuardedBuffer descriptors(2u * vidmm_stride);
  SegmentQuery query;
  query.out().pSegmentDescriptor = descriptors.data();
  query.out().NbSegment = 2u;
  query.out().SegmentDescriptorStride = vidmm_stride;
  CHECK_STATUS(query.ask(fixture.platform(), fixture.segments()), Status::ok);

  DxgkSegmentDescriptor4Record aperture = {};
  std::memcpy(&aperture, descriptors.data(), sizeof aperture);
  CHECK_EQ(aperture.Flags, 0x15u);              /* Aperture | CpuVisible | CacheCoherent */
  CHECK(aperture.BaseAddress == ApertureSegment::base_address);
  CHECK_EQ(aperture.Size, 4u * gib);
  for (std::size_t byte = vidmm_stride; byte < 2u * vidmm_stride; byte++)
      CHECK_EQ(descriptors.data()[byte], 0u);

  CHECK_EQ(query.out().PagingBufferSegmentId, ApertureSegment::segment_id);
  CHECK_EQ(query.out().PagingBufferSize, 0x1000u);
  CHECK_EQ(query.out().PagingBufferPrivateDataSize, 0u);
  CHECK_EQ(query.out().NbSegment, 1u);
  CHECK(descriptors.guards_intact()); }

/* VidMm asking for no segment at all, or at a stride that cannot hold a
 * descriptor, is refused rather than half-answered. */
TEST(memory_segments_report, refuses_a_request_that_cannot_hold_the_answer)
{ Fixture fixture;
  GuardedBuffer descriptors(vidmm_stride);
  SegmentQuery query;
  query.out().pSegmentDescriptor = descriptors.data();
  query.out().NbSegment = 0u;
  query.out().SegmentDescriptorStride = vidmm_stride;
  CHECK_STATUS(query.ask(fixture.platform(), fixture.segments()), Status::invalid_parameter);

  query.out().NbSegment = 1u;
  query.out().SegmentDescriptorStride = sizeof(DxgkSegmentDescriptor4Record) - 1u;
  CHECK_STATUS(query.ask(fixture.platform(), fixture.segments()), Status::invalid_parameter);

  query.out().SegmentDescriptorStride = vidmm_stride;
  query.index() = 1u;   /* a physical adapter this driver does not have */
  CHECK_STATUS(query.ask(fixture.platform(), fixture.segments()), Status::invalid_parameter);
  CHECK(descriptors.guards_intact()); }

TEST(memory_segments_report, is_passive_only)
{ Fixture fixture;
  SegmentQuery query;
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(query.ask(fixture.platform(), fixture.segments()));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {} }

/* Before StartDevice the adapter has no memory to describe. */
TEST(memory_segments_report, before_the_aperture_is_sized_there_is_nothing_to_report)
{ MockPlatform platform;
  ApertureSegment aperture(platform);
  MemorySegments segments(platform, aperture);
  SegmentQuery query;
  CHECK_STATUS(query.ask(platform, segments), Status::invalid_parameter); }

/* The list is only as sound as the segment in it: a broken aperture is the
 * adapter telling VidMm about memory it does not have. */
TEST(memory_segments_invariants, hold_only_while_the_aperture_does)
{ Fixture fixture;
  CHECK(fixture.segments().invariants_hold());
  av::ApertureSegmentProbe::set_size(fixture.aperture(), 4097u);
  CHECK(!fixture.segments().invariants_hold());
  av::ApertureSegmentProbe::set_size(fixture.aperture(), 4u * gib);
  CHECK(fixture.segments().invariants_hold()); }
