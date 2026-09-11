#include "../../src/core/ddi/dxgk_build_paging_buffer_copy_page_table_entries_record.hpp"
#include "../../src/core/ddi/dxgk_build_paging_buffer_copy_range_record.hpp"
#include "../../src/core/ddi/dxgk_build_paging_buffer_fill_virtual_record.hpp"
#include "../../src/core/ddi/dxgk_build_paging_buffer_flush_tlb_record.hpp"
#include "../../src/core/ddi/dxgk_build_paging_buffer_transfer_virtual_record.hpp"
#include "../../src/core/ddi/dxgk_build_paging_buffer_update_page_table_record.hpp"
#include "../../src/core/ddi/dxgk_pte_record.hpp"
#include "../../src/core/ddi/dxgkarg_build_paging_buffer_record.hpp"
#include "../../src/core/paging_request.hpp"
#include "../../src/core/status.hpp"
#include "../harness/mock_platform.hpp"
#include "../harness/test_registry.hpp"
#include "../harness/violation.hpp"

#include <cstdint>
#include <cstring>

using av::DxgkargBuildPagingBufferRecord;
using av::DxgkBuildPagingBufferCopyPageTableEntriesRecord;
using av::DxgkBuildPagingBufferCopyRangeRecord;
using av::DxgkBuildPagingBufferFillVirtualRecord;
using av::DxgkBuildPagingBufferFlushTlbRecord;
using av::DxgkBuildPagingBufferTransferVirtualRecord;
using av::DxgkBuildPagingBufferUpdatePageTableRecord;
using av::DxgkPteRecord;
using av::Irql;
using av::MockPlatform;
using av::PagingRequest;
using av::Status;
using av::Violation;

namespace {

constexpr std::uint8_t  poison = 0xEEu;
constexpr std::int32_t  insufficient_dma_buffer = static_cast<std::int32_t>(0xC01E0001u);
constexpr std::int32_t  invalid_parameter = static_cast<std::int32_t>(0xC000000Du);
enum : std::int32_t { virtual_transfer = 8, virtual_fill = 9, update_page_table = 11, flush_tlb = 12,
                      copy_page_table_entries = 14 };

std::uint64_t u64(const std::uint8_t *bytes, std::size_t offset)
{
    std::uint64_t value = 0u;
    for (unsigned byte = 0u; byte < 8u; byte++) value |= std::uint64_t{bytes[offset + byte]} << (8u * byte);
    return value;
}

std::uint32_t u32(const std::uint8_t *bytes, std::size_t offset)
{
    return static_cast<std::uint32_t>(u64(bytes, offset));
}

/* A paging request over a poisoned DMA buffer, its operation record placed in
 * the union the way dxgkrnl places it. */
class Paging {
public:
    explicit Paging(std::int32_t operation, std::uint32_t dma_size = 128u) : dma_{}, request_{}
    {
        std::memset(dma_, poison, sizeof dma_);
        request_.pDmaBuffer = dma_;
        request_.DmaSize = dma_size;
        request_.Operation = operation;
    }
    template <typename Operation> void place(const Operation &operation)
    {
        std::memcpy(request_.Reserved, &operation, sizeof operation);
    }
    std::int32_t build()
    {
        return PagingRequest(platform_, request_).build().value();
    }
    DxgkargBuildPagingBufferRecord &request() { return request_; }
    const std::uint8_t             *dma() const { return dma_; }
    std::size_t written() const { return static_cast<std::size_t>(static_cast<std::uint8_t *>(request_.pDmaBuffer) - dma_); }
    bool untouched_from(std::size_t first) const
    {
        for (std::size_t byte = first; byte < sizeof dma_; byte++)
            if (dma_[byte] != poison) return false;
        return true;
    }

private:
    MockPlatform                   platform_;
    std::uint8_t                   dma_[160];
    DxgkargBuildPagingBufferRecord request_;
};

DxgkBuildPagingBufferCopyRangeRecord range(std::uint32_t count, std::uint32_t source_start, std::uint32_t destination_start)
{
    return {count, 0u, 0x1000u, 0x2000u, source_start, destination_start};
}

}  // namespace

TEST(paging_request_build, an_aperture_or_residency_operation_needs_no_dma_bytes)
{ const std::int32_t operations[] = {2, 5, 6, 15};   /* DISCARD_CONTENT, MAP/UNMAP_APERTURE_SEGMENT, NOTIFY_RESIDENCY */
  for (std::int32_t operation : operations) {
      Paging paging(operation);
      CHECK_EQ(paging.build(), 0);
      CHECK_EQ(paging.written(), 0u);
  } }

TEST(paging_request_build, every_other_operation_is_refused)
{ const std::int32_t operations[] = {0, 1, 3, 4, 7, 10, 13, 16, 17, 22, 23, -1};
  for (std::int32_t operation : operations) {
      Paging paging(operation);
      CHECK_EQ(paging.build(), invalid_parameter);
      CHECK(paging.untouched_from(0u));
  } }

TEST(paging_request_build, a_page_table_update_writes_the_table_not_the_buffer)
{ std::uint8_t table[4096] = {};
  const DxgkPteRecord entry = {1u, 0x42u};
  DxgkBuildPagingBufferUpdatePageTableRecord update = {};
  update.PageTableAddress = table;
  update.pPageTableEntries = &entry;
  update.StartIndex = 1u;
  update.NumPageTableEntries = 1u;
  Paging paging(update_page_table);
  paging.place(update);
  CHECK_EQ(paging.build(), 0);
  CHECK_EQ(u64(table, 8u), (0x42ull << 12u) | 1u);
  CHECK_EQ(paging.written(), 0u);
  update.StartIndex = 512u;
  paging.place(update);
  CHECK_EQ(paging.build(), invalid_parameter); }

TEST(paging_request_build, a_tlb_flush_is_one_command)
{ DxgkBuildPagingBufferFlushTlbRecord flush = {};
  flush.RootPageTableAddressSegmentId = 2u;
  flush.RootPageTableAddressSegmentOffset = 0x7000u;
  flush.StartVirtualAddress = 0x10000u;
  flush.EndVirtualAddress = 0x30000u;
  Paging paging(flush_tlb);
  paging.place(flush);
  CHECK_EQ(paging.build(), 0);
  CHECK_EQ(paging.written(), 40u);
  CHECK_EQ(u32(paging.dma(), 4u), 1u);
  CHECK_EQ(u64(paging.dma(), 8u), 0x7000u);
  CHECK_EQ(u32(paging.dma(), 16u), 2u);
  CHECK_EQ(u64(paging.dma(), 24u), 0x10000u);
  CHECK_EQ(u64(paging.dma(), 32u), 0x30000u);
  CHECK(paging.untouched_from(40u)); }

TEST(paging_request_build, a_virtual_fill_is_one_command)
{ DxgkBuildPagingBufferFillVirtualRecord fill = {};
  fill.FillSizeInBytes = 0x2000u;
  fill.FillPattern = 0x11223344u;
  fill.DestinationVirtualAddress = 0x80000u;
  Paging paging(virtual_fill);
  paging.place(fill);
  CHECK_EQ(paging.build(), 0);
  CHECK_EQ(paging.written(), 32u);
  CHECK_EQ(u32(paging.dma(), 4u), 2u);
  CHECK_EQ(u64(paging.dma(), 8u), 0x80000u);
  CHECK_EQ(u64(paging.dma(), 16u), 0x2000u);
  CHECK_EQ(u32(paging.dma(), 24u), 0x11223344u); }

TEST(paging_request_build, a_virtual_transfer_is_one_command)
{ DxgkBuildPagingBufferTransferVirtualRecord transfer = {};
  transfer.TransferSizeInBytes = 0x1000u;
  transfer.SourceVirtualAddress = 0x10000u;
  transfer.DestinationVirtualAddress = 0x20000u;
  transfer.SourcePageTable = 0x30000u;
  transfer.DestinationPageTable = 0x40000u;
  transfer.TransferDirection = 1;
  transfer.Flags = 2u;
  Paging paging(virtual_transfer);
  paging.place(transfer);
  CHECK_EQ(paging.build(), 0);
  CHECK_EQ(paging.written(), 56u);
  CHECK_EQ(u32(paging.dma(), 4u), 3u);
  CHECK_EQ(u64(paging.dma(), 8u), 0x10000u);
  CHECK_EQ(u64(paging.dma(), 16u), 0x20000u);
  CHECK_EQ(u64(paging.dma(), 24u), 0x1000u);
  CHECK_EQ(u64(paging.dma(), 32u), 0x30000u);
  CHECK_EQ(u64(paging.dma(), 40u), 0x40000u);
  CHECK_EQ(u32(paging.dma(), 48u), 1u);
  CHECK_EQ(u32(paging.dma(), 52u), 2u); }

TEST(paging_request_build, a_command_that_does_not_fit_asks_for_a_fresh_buffer)
{ const std::int32_t operations[] = {virtual_transfer, virtual_fill, flush_tlb};
  for (std::int32_t operation : operations) {
      Paging paging(operation, 31u);
      CHECK_EQ(paging.build(), insufficient_dma_buffer);
      CHECK_EQ(paging.written(), 0u);
      CHECK(paging.untouched_from(0u));
  } }

TEST(paging_request_build, a_command_without_a_buffer_is_refused)
{ const std::int32_t operations[] = {virtual_transfer, virtual_fill, flush_tlb, copy_page_table_entries};
  for (std::int32_t operation : operations) {
      Paging paging(operation);
      paging.request().pDmaBuffer = nullptr;
      CHECK_EQ(paging.build(), invalid_parameter);
  } }

TEST(paging_request_build, page_table_entries_are_copied_range_by_range)
{ const DxgkBuildPagingBufferCopyRangeRecord ranges[] = {range(3u, 0u, 509u), range(512u, 0u, 0u)};
  const DxgkBuildPagingBufferCopyPageTableEntriesRecord copy = {2u, 0u, ranges};
  Paging paging(copy_page_table_entries);
  paging.place(copy);
  CHECK_EQ(paging.build(), 0);
  CHECK_EQ(paging.written(), 80u);
  CHECK_EQ(u32(paging.dma(), 4u), 4u);
  CHECK_EQ(u32(paging.dma(), 8u), 2u);
  CHECK_EQ(u64(paging.dma(), 16u), 0x1000u);
  CHECK_EQ(u32(paging.dma(), 36u), 509u);
  CHECK_EQ(u32(paging.dma(), 40u), 3u);
  CHECK_EQ(u32(paging.dma(), 72u), 512u);
  CHECK(paging.untouched_from(80u)); }

TEST(paging_request_build, ranges_that_do_not_fit_continue_in_the_next_buffer)
{ const DxgkBuildPagingBufferCopyRangeRecord ranges[] = {range(1u, 1u, 1u), range(2u, 2u, 2u), range(3u, 3u, 3u)};
  const DxgkBuildPagingBufferCopyPageTableEntriesRecord copy = {3u, 0u, ranges};
  Paging paging(copy_page_table_entries, 16u + 2u * 32u + 31u);
  paging.place(copy);
  CHECK_EQ(paging.build(), insufficient_dma_buffer);
  CHECK_EQ(paging.request().MultipassOffset, 2u);
  CHECK_EQ(paging.written(), 80u);
  CHECK_EQ(u32(paging.dma(), 8u), 2u);
  paging.request().pDmaBuffer = const_cast<std::uint8_t *>(paging.dma());
  CHECK_EQ(paging.build(), 0);
  CHECK_EQ(paging.written(), 48u);
  CHECK_EQ(u32(paging.dma(), 8u), 1u);
  CHECK_EQ(u32(paging.dma(), 40u), 3u); }

TEST(paging_request_build, a_buffer_without_room_for_one_range_is_left_untouched)
{ const DxgkBuildPagingBufferCopyRangeRecord ranges[] = {range(1u, 1u, 1u)};
  const DxgkBuildPagingBufferCopyPageTableEntriesRecord copy = {1u, 0u, ranges};
  const std::uint32_t sizes[] = {15u, 47u};
  for (std::uint32_t size : sizes) {
      Paging paging(copy_page_table_entries, size);
      paging.place(copy);
      CHECK_EQ(paging.build(), insufficient_dma_buffer);
      CHECK_EQ(paging.written(), 0u);
      CHECK_EQ(paging.request().MultipassOffset, 0u);
  } }

TEST(paging_request_build, a_hostile_page_table_copy_advances_nothing)
{ const DxgkBuildPagingBufferCopyRangeRecord outside[] = {range(1u, 0u, 0u), range(2u, 511u, 0u)};
  const DxgkBuildPagingBufferCopyRangeRecord wrapping[] = {range(0xFFFFFFFFu, 2u, 0u)};
  const DxgkBuildPagingBufferCopyRangeRecord late[] = {range(1u, 0u, 0xFFFFFFF0u)};
  const DxgkBuildPagingBufferCopyPageTableEntriesRecord copies[] = {
      {2u, 0u, outside}, {1u, 0u, wrapping}, {1u, 0u, late}, {1u, 0u, nullptr}, {0u, 0u, late}};
  for (const DxgkBuildPagingBufferCopyPageTableEntriesRecord &copy : copies) {
      Paging paging(copy_page_table_entries);
      paging.place(copy);
      CHECK_EQ(paging.build(), invalid_parameter);
      CHECK_EQ(paging.written(), 0u);
      CHECK_EQ(paging.request().MultipassOffset, 0u);
  }
  Paging finished(copy_page_table_entries);
  finished.place(copies[0]);
  finished.request().MultipassOffset = 2u;
  CHECK_EQ(finished.build(), invalid_parameter); }

TEST(paging_request, is_passive_only)
{ MockPlatform platform; DxgkargBuildPagingBufferRecord request = {};
  request.Operation = 5;
  platform.set_irql(Irql::dispatch);
  static_cast<void>(PagingRequest(platform, request).build());
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling)); }
