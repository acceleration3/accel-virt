/* The harness's VidMm segment model, proven against crafted answers, then
 * against the driver's own through the registered table. */
#include "miniport/ddi/dxgk_query_segment_out4_record.hpp"
#include "miniport/ddi/dxgk_segment_descriptor4_record.hpp"
#include "miniport/ddi/dxgkarg_query_adapter_info_record.hpp"
#include "miniport/driver/ddi_slot.hpp"
#include "miniport/driver/ddi_table.hpp"
#include "core/support/status.hpp"
#include "harness/dxgkrnl/fake_dxgadapter.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/vidmm/fake_vidmm.hpp"
#include "harness/vidmm/segment_rule.hpp"

#include <cstdint>
#include <cstring>

using av::DdiRoutine;
using av::DdiSlot;
using av::DdiTable;
using av::DxgkQuerySegmentOut4Record;
using av::DxgkSegmentDescriptor4Record;
using av::DxgkargQueryAdapterInfoRecord;
using av::FakeDxgadapter;
using av::FakeVidMm;
using av::MockPlatform;
using av::SegmentRule;
using av::Status;

namespace {

constexpr std::int32_t no_memory = static_cast<std::int32_t>(0xC0000017u);
constexpr std::int32_t invalid_parameter = static_cast<std::int32_t>(0xC000000Du);
constexpr std::int32_t unsuccessful = static_cast<std::int32_t>(0xC0000001u);

class CraftedSegments {
public:
    CraftedSegments() : count_(1u), paging_segment_(1u), fail_pass_(0), descriptors_{}
    {
        descriptors_[0].Flags = 0x15u;
        descriptors_[0].Size = 0x100000u;
        if (table_.set(DdiSlot::DxgkDdiQueryAdapterInfo, reinterpret_cast<DdiRoutine>(&CraftedSegments::answer)) !=
            Status::ok)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "set failed");
    }

    static std::int32_t answer(void *context, const DxgkargQueryAdapterInfoRecord *query)
    { return static_cast<CraftedSegments *>(context)->respond(*query); }

    const DdiTable &table() const { return table_; }
    DxgkSegmentDescriptor4Record &descriptor(std::uint32_t index) { return descriptors_[index]; }
    void set_count(std::uint32_t count) { count_ = count; }
    void set_paging_segment(std::uint32_t segment) { paging_segment_ = segment; }
    void fail_pass(int pass) { fail_pass_ = pass; }

private:
    std::int32_t respond(const DxgkargQueryAdapterInfoRecord &query)
    {
        auto *out = static_cast<DxgkQuerySegmentOut4Record *>(query.pOutputData);
        const int pass = out->pSegmentDescriptor == nullptr ? 1 : 2;
        if (pass == fail_pass_) return no_memory;
        if (pass == 2) {
            for (std::uint32_t i = 0u; i < count_ && i < 4u; i++)
                std::memcpy(out->pSegmentDescriptor + i * out->SegmentDescriptorStride, &descriptors_[i],
                            sizeof descriptors_[i]);
            out->PagingBufferSegmentId = paging_segment_;
        }
        out->NbSegment = count_;
        return 0;
    }

    DdiTable                     table_;
    std::uint32_t                count_;
    std::uint32_t                paging_segment_;
    int                          fail_pass_;
    DxgkSegmentDescriptor4Record descriptors_[4];
};

}  // namespace

TEST(fake_vidmm_create_segments, accepts_one_aperture_segment)
{ MockPlatform platform; CraftedSegments miniport;
  FakeDxgadapter adapter(platform, miniport.table());
  FakeVidMm vidmm(adapter);
  CHECK_EQ(vidmm.create_segments(&miniport), 0);
  CHECK(vidmm.failed_rule() == SegmentRule::none);
  CHECK_EQ(vidmm.answer().NbSegment, 1u);
  CHECK_EQ(vidmm.segment(0u).Flags, 0x15u);
  CHECK_EQ(vidmm.segment(FakeVidMm::max_segments).Flags, 0u); }

TEST(fake_vidmm_create_segments, refuses_a_count_it_cannot_hold_or_a_failed_pass)
{ struct Case { int knob; std::int32_t status; SegmentRule rule; };
  const Case cases[] = {
      {0, unsuccessful, SegmentRule::no_segment},
      {1, unsuccessful, SegmentRule::too_many_segments},
      {2, no_memory, SegmentRule::count_query_failed},
      {3, no_memory, SegmentRule::descriptor_query_failed},
  };
  for (const Case &c : cases) {
      MockPlatform platform; CraftedSegments miniport;
      if (c.knob == 0) miniport.set_count(0u);
      if (c.knob == 1) miniport.set_count(33u);
      if (c.knob == 2) miniport.fail_pass(1);
      if (c.knob == 3) miniport.fail_pass(2);
      FakeDxgadapter adapter(platform, miniport.table());
      FakeVidMm vidmm(adapter);
      CHECK_EQ(vidmm.create_segments(&miniport), c.status);
      CHECK(vidmm.failed_rule() == c.rule);
  } }

TEST(fake_vidmm_validate, refuses_each_descriptor_validate_segment_descriptors_refuses)
{ using Mutate = void (*)(CraftedSegments &);
  struct Case { Mutate mutate; SegmentRule rule; };
  const Case cases[] = {
      {[](CraftedSegments &m) { m.descriptor(0).NumUEFIFrameBufferRanges = 2u; }, SegmentRule::too_many_uefi_ranges},
      {[](CraftedSegments &m) { m.set_count(2u); m.descriptor(1).Size = 0x1000u;
                                m.descriptor(0).NumUEFIFrameBufferRanges = 1u; m.descriptor(1).NumUEFIFrameBufferRanges = 1u; },
       SegmentRule::uefi_ranges_in_two_segments},
      {[](CraftedSegments &m) { m.descriptor(0).Size = 0x1001u; }, SegmentRule::size_not_page_aligned},
      {[](CraftedSegments &m) { m.descriptor(0).Flags = 0x400000u; }, SegmentRule::invalid_flags},
      {[](CraftedSegments &m) { m.descriptor(0).Flags = 0x2u; }, SegmentRule::invalid_flags},
      {[](CraftedSegments &m) { m.descriptor(0).Flags = 0x100u; }, SegmentRule::invalid_flags},
      {[](CraftedSegments &m) { m.descriptor(0).Flags = 0x380u; }, SegmentRule::invalid_flags},
      {[](CraftedSegments &m) { m.descriptor(0).Flags = 0x2004u; }, SegmentRule::cpu_host_aperture_with_aperture},
      {[](CraftedSegments &m) { m.set_paging_segment(2u); }, SegmentRule::paging_buffer_segment_out_of_range},
      /* Zero is not a segment: identifiers are one-based. */
      {[](CraftedSegments &m) { m.set_paging_segment(0u); }, SegmentRule::paging_buffer_segment_out_of_range},
  };
  for (const Case &c : cases) {
      MockPlatform platform; CraftedSegments miniport;
      c.mutate(miniport);
      FakeDxgadapter adapter(platform, miniport.table());
      FakeVidMm vidmm(adapter);
      CHECK_EQ(vidmm.create_segments(&miniport), invalid_parameter);
      CHECK(vidmm.failed_rule() == c.rule);
  } }

TEST(fake_vidmm_validate, accepts_standby_with_one_hibernate_preservation_flag_and_a_host_aperture_alone)
{ MockPlatform platform; CraftedSegments miniport;
  FakeDxgadapter adapter(platform, miniport.table());
  FakeVidMm vidmm(adapter);
  std::uint8_t descriptors[2u * FakeVidMm::stride] = {};
  DxgkSegmentDescriptor4Record descriptor = {};
  descriptor.Flags = 0x180u;
  std::memcpy(descriptors, &descriptor, sizeof descriptor);
  descriptor.Flags = 0x2000u;
  descriptor.NumUEFIFrameBufferRanges = 1u;
  std::memcpy(descriptors + FakeVidMm::stride, &descriptor, sizeof descriptor);
  CHECK(vidmm.validate(descriptors, 2u, 2u) == SegmentRule::none); }

/* What dxdiag and DXGI report: a plain memory segment is dedicated video
 * memory, one populated from system memory is dedicated system memory, and
 * neither an aperture nor reserved system memory is dedicated at all. Each
 * counts no more than its commit limit. */
TEST(fake_vidmm_totals, classify_each_segment_as_get_total_segment_size_does)
{ MockPlatform platform; CraftedSegments miniport;
  miniport.set_count(4u);
  miniport.descriptor(0).CommitLimit = 0x100000u;
  miniport.descriptor(1).Size = 0x1000000u;
  miniport.descriptor(1).CommitLimit = 0x800000u;
  miniport.descriptor(2).Flags = 0x40u;
  miniport.descriptor(2).Size = 0x200000u;
  miniport.descriptor(2).CommitLimit = 0x400000u;
  miniport.descriptor(3).Flags = 0x1000u;
  miniport.descriptor(3).Size = 0x400000u;
  miniport.descriptor(3).CommitLimit = 0x400000u;
  FakeDxgadapter adapter(platform, miniport.table());
  FakeVidMm vidmm(adapter);
  CHECK_EQ(vidmm.create_segments(&miniport), 0);
  CHECK_EQ(vidmm.dedicated_video_memory(), 0x800000u);
  CHECK_EQ(vidmm.dedicated_system_memory(), 0x200000u);
  CHECK_EQ(vidmm.aperture_memory(), 0x100000u); }

TEST(fake_vidmm_totals, an_aperture_alone_is_no_dedicated_memory)
{ MockPlatform platform; CraftedSegments miniport;
  miniport.descriptor(0).CommitLimit = 0x100000u;
  FakeDxgadapter adapter(platform, miniport.table());
  FakeVidMm vidmm(adapter);
  CHECK_EQ(vidmm.create_segments(&miniport), 0);
  CHECK_EQ(vidmm.dedicated_video_memory(), 0u);
  CHECK_EQ(vidmm.dedicated_system_memory(), 0u);
  CHECK_EQ(vidmm.aperture_memory(), 0x100000u); }
