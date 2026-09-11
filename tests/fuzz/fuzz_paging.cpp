/* BuildPagingBuffer over an attacker-chosen operation, DMA size, multipass
 * offset and operation record, with the page table, entries and copy ranges it
 * points at exactly as large as they claim. Whatever is refused, nothing may be
 * written outside the DMA buffer or the one page table, the DMA pointer may
 * only advance within the buffer, and no IRQL rule may break. */
#include "core/ddi/dxgk_build_paging_buffer_copy_page_table_entries_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_copy_range_record.hpp"
#include "core/ddi/dxgk_build_paging_buffer_update_page_table_record.hpp"
#include "core/ddi/dxgk_pte_record.hpp"
#include "core/ddi/dxgkarg_build_paging_buffer_record.hpp"
#include "core/memory/paging_request.hpp"
#include "harness/platform/mock_platform.hpp"
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
constexpr std::int32_t  update_page_table = 11;
constexpr std::int32_t  copy_page_table_entries = 14;

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

    av::MockPlatform platform;
    static_cast<void>(av::PagingRequest(platform, request).build());

    if (!untouched(dma, dma_capacity, sizeof dma) || !untouched(table, table_bytes, sizeof table)) __builtin_trap();
    if (request.pDmaBuffer != nullptr &&
        static_cast<std::size_t>(static_cast<std::uint8_t *>(request.pDmaBuffer) - dma) > dma_capacity)
        __builtin_trap();
    if (request.MultipassOffset > max_ranges + 1u) __builtin_trap();
    if (platform.unclaimed_violations() != 0u) __builtin_trap();
    return 0;
}
