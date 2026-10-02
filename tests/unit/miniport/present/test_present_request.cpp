#include "miniport/ddi/dxgk_present_allocation_info_record.hpp"
#include "miniport/ddi/dxgkarg_present_record.hpp"
#include "miniport/present/present_request.hpp"
#include "core/support/nt_status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "proto/commands/present_command.hpp"

#include <cstdint>
#include <cstring>

using av::D3dddiPatchLocationListRecord;
using av::DxgkargPresentRecord;
using av::Irql;
using av::MockPlatform;
using av::PresentCommand;
using av::PresentRequest;
using av::RectRecord;
using av::Violation;

namespace {

constexpr std::uint32_t insufficient_dma_buffer = 0xC01E0001u;
constexpr std::uint32_t invalid_parameter = 0xC000000Du;
constexpr std::uint32_t blt = 0x1u;
constexpr std::uint32_t color_fill = 0x2u;
constexpr std::uint32_t flip = 0x4u;
constexpr std::uint32_t flip_with_no_wait = 0x8u;
constexpr std::uint8_t  poison = 0xEEu;

std::uint32_t u32(const std::uint8_t *bytes, std::size_t offset)
{
    return std::uint32_t{bytes[offset]} | std::uint32_t{bytes[offset + 1u]} << 8u |
           std::uint32_t{bytes[offset + 2u]} << 16u | std::uint32_t{bytes[offset + 3u]} << 24u;
}

std::uint64_t u64(const std::uint8_t *bytes, std::size_t offset)
{
    return std::uint64_t{u32(bytes, offset)} | std::uint64_t{u32(bytes, offset + 4u)} << 32u;
}

/* One Present as dxgkrnl builds it: a DMA buffer of `dma_size` inside a larger
 * poisoned one, a three-entry patch list, and three sub-rectangles. */
class Present {
public:
    explicit Present(std::uint32_t flags, std::uint32_t dma_size = 512u)
        : dma_{}, patches_{}, rects_{}, allocations_{}, request_{}
    {
        std::memset(dma_, poison, sizeof dma_);
        std::memset(static_cast<void *>(patches_), poison, sizeof patches_);
        for (std::int32_t index = 0; index < 3; index++) rects_[index] = {index, index, index + 10, index + 20};
        request_.pDmaBuffer = dma_;
        request_.DmaSize = dma_size;
        request_.pPatchLocationListOut = patches_;
        request_.PatchLocationListOutSize = 3u;
        request_.Color = 0x00123456u;
        request_.SrcRect = {0, 0, 640, 480};
        request_.DstRect = {10, 20, 650, 500};
        request_.SubRectCnt = 3u;
        request_.pDstSubRects = rects_;
        request_.FlipInterval = 1;
        request_.Flags = flags;
        allocations_[1].AllocationVirtualAddress = 0x0000123400001000ull;
        allocations_[2].AllocationVirtualAddress = 0x0000567800002000ull;
    }

    /* As dxgkrnl presents for a context with GPU virtual addressing. */
    void on_gpu_virtual_addresses()
    {
        request_.pPatchLocationListOut = nullptr;
        request_.PatchLocationListOutSize = 0u;
        request_.pAllocationList = allocations_;
    }

    std::uint32_t run()
    {
        MockPlatform platform;
        return static_cast<std::uint32_t>(PresentRequest(platform, request_).present().value());
    }
    DxgkargPresentRecord                &request() { return request_; }
    const std::uint8_t                  *dma() const { return dma_; }
    const D3dddiPatchLocationListRecord &patch(unsigned index) const { return patches_[index]; }
    RectRecord                          &rect(unsigned index) { return rects_[index]; }
    std::size_t written() const { return static_cast<std::size_t>(static_cast<std::uint8_t *>(request_.pDmaBuffer) - dma_); }
    std::size_t patched() const { return static_cast<std::size_t>(request_.pPatchLocationListOut - patches_); }
    bool untouched_from(std::size_t offset) const
    {
        for (std::size_t byte = offset; byte < sizeof dma_; byte++)
            if (dma_[byte] != poison) return false;
        return true;
    }
    bool patches_untouched_from(unsigned index) const
    {
        const auto *bytes = reinterpret_cast<const std::uint8_t *>(patches_);
        for (std::size_t byte = index * sizeof(D3dddiPatchLocationListRecord); byte < sizeof patches_; byte++)
            if (bytes[byte] != poison) return false;
        return true;
    }

private:
    std::uint8_t                  dma_[1024];
    D3dddiPatchLocationListRecord patches_[4];
    RectRecord                    rects_[3];
    av::DxgkPresentAllocationInfoRecord allocations_[3];
    DxgkargPresentRecord          request_;
};

}  // namespace

TEST(present_request_present, a_blt_writes_one_command_with_both_patches)
{ Present present(blt);
  CHECK_EQ(present.run(), 0u);
  const std::size_t size = PresentCommand::header_size + 3u * PresentCommand::sub_rectangle_size;
  CHECK_EQ(present.written(), size);
  CHECK_EQ(u32(present.dma(), 0u), 0x00505641u);
  CHECK_EQ(u32(present.dma(), 4u), 1u);
  CHECK_EQ(u32(present.dma(), 8u), 0u);
  CHECK_EQ(u32(present.dma(), 12u), 3u);
  CHECK_EQ(u32(present.dma(), 24u), 640u);
  CHECK_EQ(u32(present.dma(), 32u), 10u);
  CHECK_EQ(u32(present.dma(), 64u), 0u);
  CHECK_EQ(u32(present.dma(), PresentCommand::header_size + 16u), 1u);
  CHECK_EQ(u32(present.dma(), PresentCommand::header_size + 44u), 22u);
  CHECK(present.untouched_from(size));
  CHECK_EQ(present.patched(), 2u);
  CHECK_EQ(present.patch(0u).AllocationIndex, 1u);
  CHECK_EQ(present.patch(0u).PatchOffset, PresentCommand::source_address_offset);
  CHECK_EQ(present.patch(0u).SlotId, 0u);
  CHECK_EQ(present.patch(0u).AllocationOffset, 0u);
  CHECK_EQ(present.patch(1u).AllocationIndex, 2u);
  CHECK_EQ(present.patch(1u).PatchOffset, PresentCommand::destination_address_offset);
  CHECK(present.patches_untouched_from(2u)); }

TEST(present_request_present, a_colour_fill_patches_only_the_destination)
{ Present present(color_fill);
  present.request().SubRectCnt = 0u;
  CHECK_EQ(present.run(), 0u);
  CHECK_EQ(present.written(), PresentCommand::header_size);
  CHECK_EQ(u32(present.dma(), 4u), 2u);
  CHECK_EQ(u32(present.dma(), 8u), 0x00123456u);
  CHECK_EQ(present.patched(), 1u);
  CHECK_EQ(present.patch(0u).AllocationIndex, 2u);
  CHECK_EQ(present.patch(0u).PatchOffset, PresentCommand::destination_address_offset);
  CHECK(present.patches_untouched_from(1u)); }

TEST(present_request_present, a_flip_patches_only_the_source)
{ const std::uint32_t flips[] = {flip, flip | flip_with_no_wait};
  for (std::uint32_t flags : flips) {
      Present present(flags);
      CHECK_EQ(present.run(), 0u);
      CHECK_EQ(u32(present.dma(), 4u), 3u);
      CHECK_EQ(u32(present.dma(), 64u), 1u);
      CHECK_EQ(present.patched(), 1u);
      CHECK_EQ(present.patch(0u).AllocationIndex, 1u);
      CHECK_EQ(present.patch(0u).PatchOffset, PresentCommand::source_address_offset);
  } }

TEST(present_request_present, sub_rectangles_that_do_not_fit_go_in_the_next_pass)
{ Present first(blt, PresentCommand::header_size + PresentCommand::sub_rectangle_size + 15u);
  CHECK_EQ(first.run(), insufficient_dma_buffer);
  CHECK_EQ(first.request().MultipassOffset, 1u);
  CHECK_EQ(u32(first.dma(), 12u), 1u);
  CHECK_EQ(first.written(), PresentCommand::header_size + PresentCommand::sub_rectangle_size);
  CHECK_EQ(first.patched(), 2u);
  Present second(blt);
  second.request().MultipassOffset = 1u;
  CHECK_EQ(second.run(), 0u);
  CHECK_EQ(u32(second.dma(), 12u), 2u);
  CHECK_EQ(u32(second.dma(), PresentCommand::header_size), 1u);
  CHECK_EQ(u32(second.dma(), PresentCommand::header_size + 16u), 2u);
  CHECK_EQ(second.request().MultipassOffset, 1u); }

TEST(present_request_present, a_buffer_without_room_for_progress_writes_nothing)
{ Present no_header(color_fill, PresentCommand::header_size - 1u);
  no_header.request().SubRectCnt = 0u;
  CHECK_EQ(no_header.run(), insufficient_dma_buffer);
  CHECK(no_header.untouched_from(0u));
  Present no_rectangle(blt, PresentCommand::header_size + PresentCommand::sub_rectangle_size - 1u);
  CHECK_EQ(no_rectangle.run(), insufficient_dma_buffer);
  CHECK(no_rectangle.untouched_from(0u));
  CHECK_EQ(no_rectangle.patched(), 0u);
  CHECK_EQ(no_rectangle.request().MultipassOffset, 0u); }

TEST(present_request_present, flags_must_name_exactly_one_supported_operation)
{ const std::uint32_t refused[] = {0u, blt | color_fill, blt | flip, blt | 0x10u, blt | flip_with_no_wait,
                                   flip | 0x80u, color_fill | 0x2000u};
  for (std::uint32_t flags : refused) {
      Present present(flags);
      CHECK_EQ(present.run(), invalid_parameter);
      CHECK(present.untouched_from(0u));
  } }

TEST(present_request_present, hostile_buffers_and_rectangles_are_refused_untouched)
{ Present no_dma(blt);
  no_dma.request().pDmaBuffer = nullptr;
  CHECK_EQ(no_dma.run(), invalid_parameter);
  Present no_rectangles(blt);
  no_rectangles.request().pDstSubRects = nullptr;
  CHECK_EQ(no_rectangles.run(), invalid_parameter);
  Present past_the_end(blt);
  past_the_end.request().MultipassOffset = 4u;
  CHECK_EQ(past_the_end.run(), invalid_parameter);
  Present inverted_destination(blt);
  inverted_destination.request().DstRect = {10, 0, 9, 1};
  CHECK_EQ(inverted_destination.run(), invalid_parameter);
  Present inverted_source(blt);
  inverted_source.request().SrcRect = {0, 10, 1, 9};
  CHECK_EQ(inverted_source.run(), invalid_parameter);
  Present inverted_sub_rectangle(blt);
  inverted_sub_rectangle.rect(2u) = {5, 5, 4, 6};
  CHECK_EQ(inverted_sub_rectangle.run(), invalid_parameter);
  CHECK(inverted_sub_rectangle.untouched_from(0u));
  Present no_patch_list(blt);
  no_patch_list.request().pPatchLocationListOut = nullptr;
  CHECK_EQ(no_patch_list.run(), invalid_parameter);
  Present short_patch_list(blt);
  short_patch_list.request().PatchLocationListOutSize = 1u;
  CHECK_EQ(short_patch_list.run(), invalid_parameter);
  CHECK(short_patch_list.untouched_from(0u));
  Present fill_one_patch(color_fill);
  fill_one_patch.request().PatchLocationListOutSize = 1u;
  CHECK_EQ(fill_one_patch.run(), 0u); }

TEST(present_request_present, a_gpu_va_blt_carries_both_virtual_addresses_and_no_patches)
{ Present present(blt);
  present.on_gpu_virtual_addresses();
  CHECK_EQ(present.run(), 0u);
  CHECK_EQ(present.written(), 72u + 3u * 16u);
  CHECK_EQ(u64(present.dma(), PresentCommand::source_address_offset), 0x0000123400001000ull);
  CHECK_EQ(u64(present.dma(), PresentCommand::destination_address_offset), 0x0000567800002000ull);
  CHECK_EQ(u32(present.dma(), 68u), 1u);
  CHECK(present.request().pPatchLocationListOut == nullptr);
  CHECK(present.patches_untouched_from(0u)); }

TEST(present_request_present, a_gpu_va_fill_or_flip_writes_only_its_allocation)
{ Present fill(color_fill);
  fill.on_gpu_virtual_addresses();
  CHECK_EQ(fill.run(), 0u);
  CHECK_EQ(u64(fill.dma(), PresentCommand::source_address_offset), 0u);
  CHECK_EQ(u64(fill.dma(), PresentCommand::destination_address_offset), 0x0000567800002000ull);
  Present shown(flip);
  shown.on_gpu_virtual_addresses();
  CHECK_EQ(shown.run(), 0u);
  CHECK_EQ(u64(shown.dma(), PresentCommand::source_address_offset), 0x0000123400001000ull);
  CHECK_EQ(u64(shown.dma(), PresentCommand::destination_address_offset), 0u); }

TEST(present_request_present, a_gpu_va_present_without_allocations_is_refused_untouched)
{ Present present(blt);
  present.on_gpu_virtual_addresses();
  present.request().pAllocationList = nullptr;
  CHECK_EQ(present.run(), invalid_parameter);
  CHECK(present.untouched_from(0u)); }

TEST(present_request_present, an_inverted_rectangle_already_presented_is_not_read_again)
{ Present present(blt);
  present.rect(0u) = {5, 5, 4, 6};
  present.request().MultipassOffset = 1u;
  CHECK_EQ(present.run(), 0u); }

TEST(present_request, is_passive_only)
{ MockPlatform platform;
  DxgkargPresentRecord request = {};
  PresentRequest present(platform, request);
  platform.set_irql(Irql::dispatch);
  static_cast<void>(present.present());
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling)); }
