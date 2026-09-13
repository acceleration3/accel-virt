#include "core/ddi/dxgk_build_paging_buffer_copy_page_table_entries_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_copy_range_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_fill_virtual_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_flush_tlb_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_transfer_virtual_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_update_page_table_record.hpp"
#include "core/ddi/dxgk_pte_record.hpp"
#include "core/ddi/dxgkarg_build_paging_buffer_record.hpp"
#include "core/ddi/dxgk_allocation_info_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_map_aperture_segment_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_unmap_aperture_segment_record.hpp"
#include "core/ddi/dxgkarg_create_allocation_record.hpp"
#include "core/memory/allocation.hpp"
#include "core/memory/allocation_table.hpp"
#include "core/memory/host_visible_segment.hpp"
#include "core/memory/memory_segments.hpp"
#include "core/memory/aperture_segment.hpp"
#include "core/memory/paging_request.hpp"
#include "core/platform/log_budget.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>
#include <cstring>

using av::Allocation;
using av::AllocationDescriptor;
using av::AllocationKind;
using av::AllocationTable;
using av::ApertureSegment;
using av::DxgkAllocationInfoRecord;
using av::DxgkargBuildPagingBufferRecord;
using av::DxgkargCreateAllocationRecord;
using av::DxgkBuildPagingBufferMapApertureSegmentRecord;
using av::DxgkBuildPagingBufferUnmapApertureSegmentRecord;
using av::GuestPageList;
using av::LogBudget;
using av::ScanoutTarget;
using av::SurfaceLayout;
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
constexpr std::int64_t segment_bytes = std::int64_t{1} << 30u;
constexpr std::uint64_t page = GuestPageList::page_bytes;

/* A four-page allocation: enough for a mapping to describe every page of it. */
const AllocationDescriptor four_pages(AllocationKind::shadow, true, SurfaceLayout(16u, 16u, 21u, 1024u),
                                      4u * page, ScanoutTarget());

class Paging {
public:
    explicit Paging(std::int32_t operation, std::uint32_t dma_size = 128u)
        : segment_(platform_), host_visible_(platform_), segments_(platform_, segment_, host_visible_),
          allocations_(platform_, segment_, segments_), budget_(platform_), dma_{}, request_{}
    {
        if (segment_.initialize(segment_bytes) != Status::ok)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "segment");
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
        return PagingRequest(platform_, allocations_, budget_, request_).build().value();
    }

    /* One allocation of four pages, as CreateAllocation would leave it. */
    Allocation *allocate()
    {
        std::uint8_t bytes[AllocationDescriptor::encoded_size] = {};
        if (four_pages.encode(bytes, sizeof bytes) != Status::ok)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "encode");
        info_ = {};
        info_.pPrivateDriverData = bytes;
        info_.PrivateDriverDataSize = AllocationDescriptor::encoded_size;
        DxgkargCreateAllocationRecord create = {};
        create.NumAllocations = 1u;
        create.pAllocationInfo = &info_;
        if (allocations_.create(create) != Status::ok)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "create");
        return static_cast<Allocation *>(info_.hAllocation);
    }

    MockPlatform &platform() { return platform_; }
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
    ApertureSegment                segment_;
    av::HostVisibleSegment         host_visible_;
    av::MemorySegments             segments_;
    AllocationTable                allocations_;
    LogBudget                      budget_;
    DxgkAllocationInfoRecord       info_ = {};
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
  ApertureSegment segment(platform);
  av::HostVisibleSegment host_visible(platform);
  const av::MemorySegments segments(platform, segment, host_visible);
  AllocationTable allocations(platform, segment, segments);
  LogBudget budget(platform);
  static_cast<void>(PagingRequest(platform, allocations, budget, request).build());
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling)); }

namespace {

/* The address the aperture segment gives an allocation mapped at `offset`. */
std::uint64_t aperture_address(std::uint64_t offset_in_pages)
{
    return static_cast<std::uint64_t>(ApertureSegment::base_address) + offset_in_pages * page;
}

DxgkBuildPagingBufferMapApertureSegmentRecord aperture_map(void *allocation, void *mdl, std::uint64_t offset,
                                                           std::uint64_t pages)
{
    DxgkBuildPagingBufferMapApertureSegmentRecord request = {};
    request.hAllocation = allocation;
    request.SegmentId = ApertureSegment::segment_id;
    request.OffsetInPages = offset;
    request.NumberOfPages = pages;
    request.pMdl = mdl;
    return request;
}

}  // namespace

TEST(paging_request_map_aperture, records_the_pages_the_allocation_is_mapped_from)
{ Paging paging(5);
  Allocation *allocation = paging.allocate();
  const std::uint64_t frames[] = {5u * page, 6u * page, 9u * page, 0x20000u};
  void *mdl = paging.platform().attach_page_frames(frames, 4u);
  paging.place(aperture_map(allocation, mdl, 0x30u, 4u));
  CHECK_EQ(paging.build(), 0);
  const GuestPageList pages = allocation->pages_at(aperture_address(0x30u), 4u);
  CHECK_EQ(pages.count(), 4u);
  CHECK_EQ(pages.frame(0u), 5u * page);
  CHECK_EQ(pages.frame(3u), 0x20000u);
  CHECK_EQ(pages.runs(), 3u); }

TEST(paging_request_map_aperture, has_no_pages_for_an_address_outside_the_mapping)
{ Paging paging(5);
  Allocation *allocation = paging.allocate();
  const std::uint64_t frames[] = {5u * page, 6u * page, 7u * page, 8u * page};
  void *mdl = paging.platform().attach_page_frames(frames, 4u);
  paging.place(aperture_map(allocation, mdl, 0x30u, 4u));
  CHECK_EQ(paging.build(), 0);
  CHECK_EQ(allocation->pages_at(aperture_address(0x31u), 4u).count(), 0u);
  CHECK_EQ(allocation->pages_at(aperture_address(0x30u) + 1u, 4u).count(), 0u);
  CHECK_EQ(allocation->pages_at(0x1000u, 4u).count(), 0u); }

TEST(paging_request_map_aperture, records_nothing_for_a_segment_that_is_not_the_aperture)
{ Paging paging(5);
  Allocation *allocation = paging.allocate();
  const std::uint64_t frames[] = {5u * page};
  void *mdl = paging.platform().attach_page_frames(frames, 1u);
  DxgkBuildPagingBufferMapApertureSegmentRecord request = aperture_map(allocation, mdl, 0u, 1u);
  request.SegmentId = ApertureSegment::system_memory_segment_id;
  paging.place(request);
  CHECK_EQ(paging.build(), 0);
  CHECK_EQ(allocation->pages_at(aperture_address(0u), 1u).count(), 0u); }

TEST(paging_request_map_aperture, answers_a_mapping_of_an_allocation_it_never_made)
{ Paging paging(5);
  const std::uint64_t frames[] = {5u * page};
  void *mdl = paging.platform().attach_page_frames(frames, 1u);
  std::uint64_t not_an_allocation = 0u;
  paging.place(aperture_map(&not_an_allocation, mdl, 0u, 1u));
  CHECK_EQ(paging.build(), 0); }

/* dxgkrnl has no answer for a driver that refuses to map a segment it
 * reported, so a mapping we cannot record is logged and left unrecorded. */
TEST(paging_request_map_aperture, answers_a_mapping_whose_mdl_does_not_cover_it)
{ Paging paging(5);
  Allocation *allocation = paging.allocate();
  const std::uint64_t frames[] = {5u * page};
  void *mdl = paging.platform().attach_page_frames(frames, 1u);
  paging.place(aperture_map(allocation, mdl, 0x30u, 4u));
  CHECK_EQ(paging.build(), 0);
  CHECK_EQ(allocation->pages_at(aperture_address(0x30u), 4u).count(), 0u);
  paging.place(aperture_map(allocation, nullptr, 0x30u, 4u));
  CHECK_EQ(paging.build(), 0);
  CHECK_EQ(allocation->pages_at(aperture_address(0x30u), 4u).count(), 0u); }

TEST(paging_request_map_aperture, says_what_it_recorded)
{ Paging paging(5);
  Allocation *allocation = paging.allocate();
  const std::uint64_t frames[] = {5u * page, 6u * page, 7u * page, 8u * page};
  void *mdl = paging.platform().attach_page_frames(frames, 4u);
  paging.place(aperture_map(allocation, mdl, 0x30u, 4u));
  CHECK_EQ(paging.build(), 0);
  CHECK(std::strstr(paging.platform().last_log(), "aperture map segment 1 offset 48 pages 4 status 0") != nullptr); }

TEST(paging_request_unmap_aperture, forgets_the_pages_the_allocation_is_leaving)
{ Paging paging(5);
  Allocation *allocation = paging.allocate();
  const std::uint64_t frames[] = {5u * page, 6u * page, 7u * page, 8u * page};
  void *mdl = paging.platform().attach_page_frames(frames, 4u);
  paging.place(aperture_map(allocation, mdl, 0x30u, 4u));
  CHECK_EQ(paging.build(), 0);
  DxgkBuildPagingBufferUnmapApertureSegmentRecord unmap = {};
  unmap.hAllocation = allocation;
  unmap.SegmentId = ApertureSegment::segment_id;
  unmap.OffsetInPages = 0x30u;
  unmap.NumberOfPages = 4u;
  paging.request().Operation = 6;
  paging.place(unmap);
  CHECK_EQ(paging.build(), 0);
  CHECK_EQ(allocation->pages_at(aperture_address(0x30u), 4u).count(), 0u);
  CHECK(std::strstr(paging.platform().last_log(), "aperture unmap segment 1 offset 48 pages 4 status 0") != nullptr); }

TEST(paging_request_unmap_aperture, answers_an_unmapping_of_an_allocation_it_never_made)
{ Paging paging(6);
  std::uint64_t not_an_allocation = 0u;
  DxgkBuildPagingBufferUnmapApertureSegmentRecord unmap = {};
  unmap.hAllocation = &not_an_allocation;
  unmap.SegmentId = ApertureSegment::segment_id;
  unmap.NumberOfPages = 1u;
  paging.place(unmap);
  CHECK_EQ(paging.build(), 0); }

/* A storm of mappings the driver cannot record spends the window and then goes
 * quiet: the log is evidence, not a flood. */
TEST(paging_request_map_aperture, stops_talking_once_the_window_is_spent)
{ Paging paging(5);
  paging.place(aperture_map(nullptr, nullptr, 0x30u, 1u));
  for (std::uint32_t line = 0u; line < LogBudget::lines_per_window; line++) CHECK_EQ(paging.build(), 0);
  const std::uint32_t spent = paging.platform().logged();
  CHECK_EQ(paging.build(), 0);
  CHECK_EQ(paging.platform().logged(), spent); }

/* An operation this phase does not implement is named once, then the window is
 * spent and the refusal goes on being silent. */
TEST(paging_request_build, names_an_operation_it_refuses_until_the_window_is_spent)
{ Paging paging(17);   /* DXGK_OPERATION_MAP_APERTURE_SEGMENT2 */
  CHECK_EQ(paging.build(), invalid_parameter);
  CHECK(std::strstr(paging.platform().last_log(), "paging operation 17 refused") != nullptr);
  for (std::uint32_t line = 0u; line < LogBudget::lines_per_window; line++)
      CHECK_EQ(paging.build(), invalid_parameter);
  const std::uint32_t spent = paging.platform().logged();
  CHECK_EQ(paging.build(), invalid_parameter);
  CHECK_EQ(paging.platform().logged(), spent); }
