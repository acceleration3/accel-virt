#include "miniport/ddi/d3dddi_patch_location_list_record.hpp"
#include "miniport/ddi/dxgk_allocation_list_record.hpp"
#include "miniport/ddi/dxgkarg_render_record.hpp"
#include "miniport/scheduling/render_request.hpp"
#include "core/support/status.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>
#include <cstring>

using av::D3dddiPatchLocationListRecord;
using av::DxgkAllocationListRecord;
using av::DxgkargRenderRecord;
using av::GuardedBuffer;
using av::Irql;
using av::MockPlatform;
using av::RenderRequest;
using av::Status;
using av::Violation;

namespace {

constexpr std::uint32_t command_bytes = 64u;
constexpr std::uint32_t dma_bytes = 128u;

class Render {
public:
    Render() : dma_(dma_bytes), allocations_{}, in_{}, out_{}, record_{}
    {
        for (std::uint32_t i = 0u; i < command_bytes; i++) command_[i] = static_cast<std::uint8_t>(i + 1u);
        std::memset(dma_.data(), 0xEE, dma_bytes);
        in_[0] = {1u, 0u, 0u, 0x10u, 0u, 0u};
        in_[1] = {0u, 0u, 0u, 0u, command_bytes - 8u, 0u};
        record_.pCommand = command_;
        record_.CommandLength = command_bytes;
        record_.pDmaBuffer = dma_.data();
        record_.DmaSize = dma_bytes;
        record_.pAllocationList = allocations_;
        record_.AllocationListSize = 2u;
        record_.pPatchLocationListIn = in_;
        record_.PatchLocationListInSize = 2u;
        record_.pPatchLocationListOut = out_;
        record_.PatchLocationListOutSize = 4u;
    }

    Status run(MockPlatform &platform) { return RenderRequest(platform, record_).render(); }
    DxgkargRenderRecord           &record() { return record_; }
    GuardedBuffer                 &dma() { return dma_; }
    D3dddiPatchLocationListRecord &in(std::uint32_t i) { return in_[i]; }
    D3dddiPatchLocationListRecord &out(std::uint32_t i) { return out_[i]; }
    const std::uint8_t            *command() const { return command_; }

private:
    std::uint8_t                  command_[command_bytes];
    GuardedBuffer                 dma_;
    DxgkAllocationListRecord      allocations_[2];
    D3dddiPatchLocationListRecord in_[2];
    D3dddiPatchLocationListRecord out_[4];
    DxgkargRenderRecord           record_;
};

}  // namespace

TEST(render_request_render, copies_the_command_and_its_patch_locations_then_advances)
{ MockPlatform platform; Render render;
  const auto *dma_start = static_cast<std::uint8_t *>(render.record().pDmaBuffer);
  const D3dddiPatchLocationListRecord *out_start = render.record().pPatchLocationListOut;
  CHECK_STATUS(render.run(platform), Status::ok);
  CHECK(std::memcmp(dma_start, render.command(), command_bytes) == 0);
  CHECK_EQ(dma_start[command_bytes], 0xEEu);
  CHECK(render.record().pDmaBuffer == dma_start + command_bytes);
  CHECK(render.record().pPatchLocationListOut == out_start + 2);
  CHECK_EQ(render.out(0u).AllocationOffset, 0x10u);
  CHECK_EQ(render.out(1u).PatchOffset, command_bytes - 8u);
  CHECK(render.dma().guards_intact()); }

TEST(render_request_render, an_empty_command_with_no_patches_is_fine)
{ MockPlatform platform; Render render;
  render.record().CommandLength = 0u;
  render.record().PatchLocationListInSize = 0u;
  CHECK_STATUS(render.run(platform), Status::ok);
  CHECK(render.record().pDmaBuffer == render.dma().data()); }

TEST(render_request_render, refuses_anything_that_would_read_or_write_outside_its_buffers)
{ for (int knob = 0; knob < 10; knob++) {
      MockPlatform platform; Render render;
      if (knob == 0) render.record().CommandLength = dma_bytes + 1u;               /* longer than the DMA buffer */
      if (knob == 1) render.record().pCommand = nullptr;
      if (knob == 2) render.record().pDmaBuffer = nullptr;
      if (knob == 3) render.record().PatchLocationListOutSize = 1u;                /* no room for the patches */
      if (knob == 4) render.record().pPatchLocationListIn = nullptr;
      if (knob == 5) render.record().pPatchLocationListOut = nullptr;
      if (knob == 6) render.in(1u).AllocationIndex = 2u;                           /* outside the allocation list */
      if (knob == 7) render.in(1u).PatchOffset = command_bytes - 7u;               /* eight bytes do not fit */
      if (knob == 8) render.in(1u).PatchOffset = 0xFFFFFFFCu;                      /* would wrap */
      if (knob == 9) render.record().pAllocationList = nullptr;
      const std::uint8_t *before = static_cast<std::uint8_t *>(render.record().pDmaBuffer);
      CHECK_STATUS(render.run(platform), Status::invalid_parameter);
      CHECK(render.record().pDmaBuffer == before);
      CHECK_EQ(render.dma().data()[0], 0xEEu);
      CHECK(render.dma().guards_intact());
  } }

TEST(render_request, is_passive_only)
{ MockPlatform platform; Render render;
  platform.set_irql(Irql::dispatch);
  CHECK_STATUS(render.run(platform), Status::ok);
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling)); }
