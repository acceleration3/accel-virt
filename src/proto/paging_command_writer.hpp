/* The commands a paging buffer carries to the device (docs/protocol.md): TLB
 * flushes and the copies, fills and page table copies VidMm asks for by GPU
 * virtual address. One command per DxgkDdiBuildPagingBuffer call, at the start
 * of the space dxgkrnl passes; a command that does not fit writes nothing. */
#ifndef AV_PAGING_COMMAND_WRITER_HPP
#define AV_PAGING_COMMAND_WRITER_HPP

#include "../core/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireWriter;

class PagingCommandWriter {
public:
    static constexpr size_t flush_tlb_size = 40u;
    static constexpr size_t fill_size = 32u;
    static constexpr size_t transfer_size = 56u;
    static constexpr size_t copy_header_size = 16u;
    static constexpr size_t copy_range_size = 32u;

    PagingCommandWriter(uint8_t *bytes, size_t capacity);

    [[nodiscard]] Status flush_tlb(uint32_t root_segment_id, uint64_t root_segment_offset, uint64_t start_address,
                                   uint64_t end_address);
    [[nodiscard]] Status fill(uint64_t destination_address, uint64_t size, uint32_t pattern);
    [[nodiscard]] Status transfer(uint64_t source_address, uint64_t destination_address, uint64_t size,
                                  uint64_t source_page_table, uint64_t destination_page_table, uint32_t direction,
                                  uint32_t flags);
    /* A page table copy with no ranges yet; each copy_range appends one and
     * counts it in the header. */
    [[nodiscard]] Status copy_page_table_entries();
    /* invalid_state unless the command written is a page table copy. */
    [[nodiscard]] Status copy_range(uint64_t source_page_table, uint64_t destination_page_table, uint32_t source_start,
                                    uint32_t destination_start, uint32_t count);
    size_t               length() const;

private:
    static constexpr uint32_t magic = 0x4D4B5652u;   /* "RVKM" */
    static constexpr size_t   header_size = 8u;
    static constexpr size_t   range_count_offset = 8u;
    enum : uint32_t { op_flush_tlb = 1u, op_fill = 2u, op_transfer = 3u, op_copy_page_table_entries = 4u };

    /* A writer over the next `size` bytes with the command header written;
     * false, writing nothing, when they do not fit. */
    bool begin(WireWriter &writer, uint32_t operation, size_t size);

    uint8_t *bytes_;
    size_t   capacity_;
    size_t   length_;
    uint32_t ranges_;
    bool     copying_;
};

}  // namespace av

#endif  // AV_PAGING_COMMAND_WRITER_HPP
