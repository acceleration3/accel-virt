#include "../../src/core/ddi/d3dddi_patch_location_list_record.hpp"
#include "../../src/core/ddi/dxgk_allocation_list_record.hpp"
#include "../../src/core/ddi/dxgkarg_patch_record.hpp"
#include "../../src/core/patch_request.hpp"
#include "../../src/core/status.hpp"
#include "../harness/byte_layout.hpp"
#include "../harness/guarded_buffer.hpp"
#include "../harness/mock_platform.hpp"
#include "../harness/test_registry.hpp"
#include "../harness/violation.hpp"

#include <cstdint>
#include <cstring>

using av::ByteLayout;
using av::D3dddiPatchLocationListRecord;
using av::DxgkAllocationListRecord;
using av::DxgkargPatchRecord;
using av::GuardedBuffer;
using av::Irql;
using av::MockPlatform;
using av::PatchRequest;
using av::Status;
using av::Violation;

namespace {

constexpr std::uint32_t dma_bytes = 64u;

class Patch {
public:
    Patch() : dma_(dma_bytes), allocations_{}, patches_{}, record_{}
    {
        std::memset(dma_.data(), 0xEE, dma_bytes);
        allocations_[0].PhysicalAddress = 0xC0001000;
        allocations_[1].PhysicalAddress = 0xC0080000;
        patches_[0] = {0u, 0u, 0u, 0u, 0u, 0u};       /* not in the submission */
        patches_[1] = {1u, 0u, 0u, 0x20u, 16u, 0u};
        patches_[2] = {0u, 0u, 0u, 0x4u, 56u, 0u};
        record_.pDmaBuffer = dma_.data();
        record_.DmaBufferSize = dma_bytes;
        record_.DmaBufferSubmissionStartOffset = 16u;
        record_.DmaBufferSubmissionEndOffset = dma_bytes;
        record_.pAllocationList = allocations_;
        record_.AllocationListSize = 2u;
        record_.pPatchLocationList = patches_;
        record_.PatchLocationListSize = 3u;
        record_.PatchLocationListSubmissionStart = 1u;
        record_.PatchLocationListSubmissionLength = 2u;
    }

    Status run(MockPlatform &platform) { return PatchRequest(platform, record_).patch(); }
    DxgkargPatchRecord            &record() { return record_; }
    GuardedBuffer                 &dma() { return dma_; }
    D3dddiPatchLocationListRecord &location(std::uint32_t i) { return patches_[i]; }

private:
    GuardedBuffer                 dma_;
    DxgkAllocationListRecord      allocations_[2];
    D3dddiPatchLocationListRecord patches_[3];
    DxgkargPatchRecord            record_;
};

}  // namespace

TEST(patch_request_patch, writes_each_address_plus_offset_little_endian)
{ MockPlatform platform; Patch patch;
  CHECK_STATUS(patch.run(platform), Status::ok);
  const ByteLayout layout(patch.dma().data(), dma_bytes);
  CHECK(layout.at(16u, 8u) == 0xC0080020u);
  CHECK(layout.at(56u, 8u) == 0xC0001004u);
  CHECK_EQ(patch.dma().data()[0], 0xEEu);
  CHECK_EQ(patch.dma().data()[24], 0xEEu);
  CHECK(patch.dma().guards_intact()); }

TEST(patch_request_patch, an_empty_submission_writes_nothing)
{ MockPlatform platform; Patch patch;
  patch.record().PatchLocationListSubmissionLength = 0u;
  patch.record().pPatchLocationList = nullptr;
  CHECK_STATUS(patch.run(platform), Status::ok);
  CHECK_EQ(patch.dma().data()[16], 0xEEu); }

TEST(patch_request_patch, a_submission_with_locations_needs_both_lists)
{ MockPlatform platform; Patch patch;
  patch.record().pPatchLocationList = nullptr;
  CHECK_STATUS(patch.run(platform), Status::invalid_parameter);
  Patch other;
  other.record().pAllocationList = nullptr;
  CHECK_STATUS(other.run(platform), Status::invalid_parameter);
  Patch backwards;
  backwards.record().DmaBufferSubmissionStartOffset = 32u;
  backwards.record().DmaBufferSubmissionEndOffset = 24u;
  CHECK_STATUS(backwards.run(platform), Status::invalid_parameter); }

TEST(patch_request_patch, refuses_the_whole_patch_before_writing_anything)
{ for (int knob = 0; knob < 11; knob++) {
      MockPlatform platform; Patch patch;
      if (knob == 0) patch.record().pDmaBuffer = nullptr;
      if (knob == 1) patch.record().DmaBufferSubmissionEndOffset = dma_bytes + 1u;
      if (knob == 2) patch.record().DmaBufferSubmissionStartOffset = dma_bytes;          /* start after end */
      if (knob == 3) patch.record().PatchLocationListSubmissionLength = 3u;              /* past the list */
      if (knob == 4) patch.record().PatchLocationListSubmissionStart = 0xFFFFFFFFu;      /* wraps */
      if (knob == 5) patch.record().pPatchLocationList = nullptr;
      if (knob == 6) patch.location(2u).AllocationIndex = 2u;
      if (knob == 7) patch.record().pAllocationList = nullptr;
      if (knob == 8) patch.location(2u).PatchOffset = 57u;                               /* crosses the end */
      if (knob == 9) patch.location(2u).PatchOffset = 8u;                                /* before the start */
      if (knob == 10) patch.location(2u).PatchOffset = 0xFFFFFFFCu;
      CHECK_STATUS(patch.run(platform), Status::invalid_parameter);
      CHECK_EQ(patch.dma().data()[16], 0xEEu);
      CHECK(patch.dma().guards_intact());
  } }

TEST(patch_request, allocates_nothing_and_is_passive_only)
{ MockPlatform platform; Patch patch;
  const std::uint32_t allocations = platform.allocations_made();
  platform.set_irql(Irql::dispatch);
  CHECK_STATUS(patch.run(platform), Status::ok);
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  CHECK_EQ(platform.allocations_made(), allocations); }
