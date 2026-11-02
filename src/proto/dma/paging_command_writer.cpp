#include "proto/dma/paging_command_writer.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

PagingCommandWriter::PagingCommandWriter(uint8_t *bytes, size_t capacity)
    : bytes_(bytes), capacity_(bytes != nullptr ? capacity : 0u), length_(0u), ranges_(0u), copying_(false)
    {
        AV_SCOPE("PagingCommandWriter::PagingCommandWriter");
    }

bool PagingCommandWriter::begin(WireWriter &writer, uint32_t operation, size_t size)
{
    AV_SCOPE("PagingCommandWriter::begin");
    if (size > capacity_) return false;
    length_ = size;
    copying_ = operation == op_copy_page_table_entries;
    writer.write_u32(magic);
    writer.write_u32(operation);
    return true;
}

Status PagingCommandWriter::flush_tlb(uint32_t root_segment_id, uint64_t root_segment_offset, uint64_t start_address,
                                      uint64_t end_address)
{
    AV_SCOPE("PagingCommandWriter::flush_tlb");
    WireWriter writer(bytes_, capacity_);
    if (!begin(writer, op_flush_tlb, flush_tlb_size)) return Status::too_large;
    writer.write_u64(root_segment_offset);
    writer.write_u32(root_segment_id);
    writer.write_zeros(4u);
    writer.write_u64(start_address);
    writer.write_u64(end_address);
    return Status::ok;
}

Status PagingCommandWriter::fill(uint64_t destination_address, uint64_t size, uint32_t pattern)
{
    AV_SCOPE("PagingCommandWriter::fill");
    WireWriter writer(bytes_, capacity_);
    if (!begin(writer, op_fill, fill_size)) return Status::too_large;
    writer.write_u64(destination_address);
    writer.write_u64(size);
    writer.write_u32(pattern);
    writer.write_zeros(4u);
    return Status::ok;
}

Status PagingCommandWriter::transfer(uint64_t source_address, uint64_t destination_address, uint64_t size,
                                     uint64_t source_page_table, uint64_t destination_page_table, uint32_t direction,
                                     uint32_t flags)
{
    AV_SCOPE("PagingCommandWriter::transfer");
    WireWriter writer(bytes_, capacity_);
    if (!begin(writer, op_transfer, transfer_size)) return Status::too_large;
    writer.write_u64(source_address);
    writer.write_u64(destination_address);
    writer.write_u64(size);
    writer.write_u64(source_page_table);
    writer.write_u64(destination_page_table);
    writer.write_u32(direction);
    writer.write_u32(flags);
    return Status::ok;
}

Status PagingCommandWriter::copy_page_table_entries()
{
    AV_SCOPE("PagingCommandWriter::copy_page_table_entries");
    WireWriter writer(bytes_, capacity_);
    if (!begin(writer, op_copy_page_table_entries, copy_header_size)) return Status::too_large;
    writer.write_zeros(8u);
    ranges_ = 0u;
    return Status::ok;
}

Status PagingCommandWriter::copy_range(uint64_t source_page_table, uint64_t destination_page_table,
                                       uint32_t source_start, uint32_t destination_start, uint32_t count)
{
    AV_SCOPE("PagingCommandWriter::copy_range");
    if (!copying_) return Status::invalid_state;
    if (capacity_ - length_ < copy_range_size) return Status::too_large;
    WireWriter writer(bytes_ + length_, copy_range_size);
    writer.write_u64(source_page_table);
    writer.write_u64(destination_page_table);
    writer.write_u32(source_start);
    writer.write_u32(destination_start);
    writer.write_u32(count);
    writer.write_zeros(4u);
    length_ += copy_range_size;
    ranges_++;
    WireWriter(bytes_ + range_count_offset, sizeof ranges_).write_u32(ranges_);
    return Status::ok;
}

size_t PagingCommandWriter::length() const
{
    AV_SCOPE("PagingCommandWriter::length");
    return length_;
}

}
