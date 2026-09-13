/* BuildPagingBuffer over an attacker-chosen operation, DMA size, multipass
 * offset and operation record, with the page table, entries and copy ranges it
 * points at exactly as large as they claim. An aperture mapping also gets an
 * MDL of fuzzed page frames and an allocation to record them on, and whatever
 * it recorded is then asked for at a fuzzed address and written as a backing
 * request. Whatever is refused, nothing may be written outside the DMA buffer,
 * the one page table or the backing request, the DMA pointer may only advance
 * within the buffer, and no IRQL rule may break. */
#include "core/ddi/dxgk_build_paging_buffer_copy_page_table_entries_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_copy_range_record.hpp"
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
#include "harness/platform/mock_platform.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "proto/virtio_gpu/gpu_request_writer.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

constexpr std::uint32_t max_bytes = 512u;
constexpr std::uint32_t max_ranges = 16u;
constexpr std::uint32_t table_bytes = 4096u;
constexpr std::uint32_t guard = 64u;
constexpr std::uint8_t  poison = 0x5Au;
constexpr std::int32_t  map_aperture_segment = 5;
constexpr std::int32_t  unmap_aperture_segment = 6;
constexpr std::int32_t  update_page_table = 11;
constexpr std::int32_t  copy_page_table_entries = 14;
constexpr std::uint32_t max_frames = 64u;
/* Room for a backing request of one memory entry per page of the allocation. */
constexpr std::size_t   backing_bytes = av::GpuRequestWriter::backing_header_size +
                                        max_frames * av::GpuRequestWriter::memory_entry_size;

bool untouched(const std::uint8_t *bytes, std::size_t first, std::size_t end)
{
    for (std::size_t byte = first; byte < end; byte++)
        if (bytes[byte] != poison) return false;
    return true;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::WireReader reader(data, size);
    std::uint8_t dma[max_bytes + guard];
    std::uint8_t table[table_bytes + guard];
    av::DxgkPteRecord entries[512];
    av::DxgkBuildPagingBufferCopyRangeRecord ranges[max_ranges];
    std::memset(dma, poison, sizeof dma);
    std::memset(table, poison, sizeof table);

    av::DxgkargBuildPagingBufferRecord request = {};
    request.Operation = static_cast<std::int32_t>(reader.read_u8() % 20u) - 1;
    request.DmaSize = reader.read_u16() % (max_bytes + 1u);
    request.MultipassOffset = reader.read_u8() % (max_ranges + 2u);
    const std::uint8_t nulls = reader.read_u8();
    reader.copy_bytes(reinterpret_cast<std::uint8_t *>(request.Reserved), sizeof request.Reserved);
    reader.copy_bytes(reinterpret_cast<std::uint8_t *>(ranges), sizeof ranges);
    reader.copy_bytes(reinterpret_cast<std::uint8_t *>(entries), sizeof entries);
    request.pDmaBuffer = (nulls & 1u) != 0u ? nullptr : dma;

    if (request.Operation == update_page_table) {
        av::DxgkBuildPagingBufferUpdatePageTableRecord update;
        std::memcpy(&update, request.Reserved, sizeof update);
        update.PageTableAddress = (nulls & 2u) != 0u ? nullptr : table;
        update.pPageTableEntries = (nulls & 4u) != 0u ? nullptr : entries;
        std::memcpy(request.Reserved, &update, sizeof update);
    }
    if (request.Operation == copy_page_table_entries) {
        av::DxgkBuildPagingBufferCopyPageTableEntriesRecord copy;
        std::memcpy(&copy, request.Reserved, sizeof copy);
        copy.NumRanges %= max_ranges + 1u;
        copy.pRanges = (nulls & 2u) != 0u ? nullptr : ranges;
        std::memcpy(request.Reserved, &copy, sizeof copy);
    }
    const std::uint32_t dma_capacity = request.DmaSize;

    std::uint64_t frames[max_frames];
    reader.copy_bytes(reinterpret_cast<std::uint8_t *>(frames), sizeof frames);
    const std::uint32_t mdl_pages = reader.read_u8() % (max_frames + 1u);
    const std::uint64_t asked_address = reader.read_u64();
    const std::uint32_t asked_pages = reader.read_u8() % (max_frames + 2u);

    av::MockPlatform platform;
    av::ApertureSegment segment(platform);
    if (segment.initialize(std::int64_t{1} << 30u) != av::Status::ok) __builtin_trap();
    av::HostVisibleSegment host_visible(platform);
    const av::MemorySegments segments(platform, segment, host_visible);
    av::AllocationTable allocations(platform, segment, segments);
    av::LogBudget       budget(platform);

    /* One allocation of max_frames pages, so a mapping of any length the
     * record claims is either recorded or refused, never truncated silently. */
    const av::AllocationDescriptor descriptor(av::AllocationKind::shadow, true,
                                              av::SurfaceLayout(16u, 16u, 21u, 1024u),
                                              std::uint64_t{max_frames} * 0x1000u, av::ScanoutTarget());
    std::uint8_t encoded[av::AllocationDescriptor::encoded_size] = {};
    if (descriptor.encode(encoded, sizeof encoded) != av::Status::ok) __builtin_trap();
    av::DxgkAllocationInfoRecord info = {};
    info.pPrivateDriverData = encoded;
    info.PrivateDriverDataSize = av::AllocationDescriptor::encoded_size;
    av::DxgkargCreateAllocationRecord create = {};
    create.NumAllocations = 1u;
    create.pAllocationInfo = &info;
    if (allocations.create(create) != av::Status::ok) __builtin_trap();
    auto *const allocation = static_cast<av::Allocation *>(info.hAllocation);

    void *const mdl = platform.attach_page_frames(frames, mdl_pages);
    if (request.Operation == map_aperture_segment) {
        av::DxgkBuildPagingBufferMapApertureSegmentRecord map;
        std::memcpy(&map, request.Reserved, sizeof map);
        map.hAllocation = (nulls & 8u) != 0u ? nullptr : allocation;
        map.pMdl = (nulls & 2u) != 0u ? nullptr : mdl;
        std::memcpy(request.Reserved, &map, sizeof map);
    }
    if (request.Operation == unmap_aperture_segment) {
        av::DxgkBuildPagingBufferUnmapApertureSegmentRecord unmap;
        std::memcpy(&unmap, request.Reserved, sizeof unmap);
        unmap.hAllocation = (nulls & 8u) != 0u ? nullptr : allocation;
        std::memcpy(request.Reserved, &unmap, sizeof unmap);
    }

    static_cast<void>(av::PagingRequest(platform, allocations, budget, request).build());

    std::uint8_t backing[backing_bytes + guard];
    std::memset(backing, poison, sizeof backing);
    av::GpuRequestWriter writer(backing, backing_bytes);
    static_cast<void>(writer.attach_backing_pages(1u, allocation->pages_at(asked_address, asked_pages)));
    if (writer.length() > backing_bytes || !untouched(backing, backing_bytes, sizeof backing)) __builtin_trap();

    if (!untouched(dma, dma_capacity, sizeof dma) || !untouched(table, table_bytes, sizeof table)) __builtin_trap();
    if (request.pDmaBuffer != nullptr &&
        static_cast<std::size_t>(static_cast<std::uint8_t *>(request.pDmaBuffer) - dma) > dma_capacity)
        __builtin_trap();
    if (request.MultipassOffset > max_ranges + 1u) __builtin_trap();
    if (platform.unclaimed_violations() != 0u) __builtin_trap();
    return 0;
}
