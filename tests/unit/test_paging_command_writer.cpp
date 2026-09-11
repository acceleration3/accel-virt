#include "../../src/core/status.hpp"
#include "../../src/proto/paging_command_writer.hpp"
#include "../harness/test_registry.hpp"

#include <cstdint>
#include <cstring>

using av::PagingCommandWriter;
using av::Status;

namespace {

constexpr std::uint8_t poison = 0xEEu;
constexpr std::uint32_t magic = 0x4D4B5652u;   /* "RVKM" */

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

struct Buffer {
    Buffer() { std::memset(bytes, poison, sizeof bytes); }
    bool untouched_from(std::size_t first) const
    {
        for (std::size_t byte = first; byte < sizeof bytes; byte++)
            if (bytes[byte] != poison) return false;
        return true;
    }
    std::uint8_t bytes[160];
};

}  // namespace

TEST(paging_command_writer_flush_tlb, names_the_root_table_and_the_range)
{ Buffer buffer;
  PagingCommandWriter writer(buffer.bytes, sizeof buffer.bytes);
  CHECK_STATUS(writer.flush_tlb(3u, 0x2000u, 0x10000u, 0x20000u), Status::ok);
  CHECK_EQ(writer.length(), PagingCommandWriter::flush_tlb_size);
  CHECK_EQ(u32(buffer.bytes, 0u), magic);
  CHECK_EQ(u32(buffer.bytes, 4u), 1u);
  CHECK_EQ(u64(buffer.bytes, 8u), 0x2000u);
  CHECK_EQ(u32(buffer.bytes, 16u), 3u);
  CHECK_EQ(u32(buffer.bytes, 20u), 0u);
  CHECK_EQ(u64(buffer.bytes, 24u), 0x10000u);
  CHECK_EQ(u64(buffer.bytes, 32u), 0x20000u);
  CHECK(buffer.untouched_from(40u)); }

TEST(paging_command_writer_fill, carries_the_pattern)
{ Buffer buffer;
  PagingCommandWriter writer(buffer.bytes, sizeof buffer.bytes);
  CHECK_STATUS(writer.fill(0x40000u, 0x3000u, 0xAABBCCDDu), Status::ok);
  CHECK_EQ(writer.length(), PagingCommandWriter::fill_size);
  CHECK_EQ(u32(buffer.bytes, 4u), 2u);
  CHECK_EQ(u64(buffer.bytes, 8u), 0x40000u);
  CHECK_EQ(u64(buffer.bytes, 16u), 0x3000u);
  CHECK_EQ(u32(buffer.bytes, 24u), 0xAABBCCDDu);
  CHECK_EQ(u32(buffer.bytes, 28u), 0u);
  CHECK(buffer.untouched_from(32u)); }

TEST(paging_command_writer_transfer, carries_both_tables_and_the_direction)
{ Buffer buffer;
  PagingCommandWriter writer(buffer.bytes, sizeof buffer.bytes);
  CHECK_STATUS(writer.transfer(0x1000u, 0x2000u, 0x3000u, 0x4000u, 0x5000u, 2u, 1u), Status::ok);
  CHECK_EQ(writer.length(), PagingCommandWriter::transfer_size);
  CHECK_EQ(u32(buffer.bytes, 4u), 3u);
  CHECK_EQ(u64(buffer.bytes, 8u), 0x1000u);
  CHECK_EQ(u64(buffer.bytes, 16u), 0x2000u);
  CHECK_EQ(u64(buffer.bytes, 24u), 0x3000u);
  CHECK_EQ(u64(buffer.bytes, 32u), 0x4000u);
  CHECK_EQ(u64(buffer.bytes, 40u), 0x5000u);
  CHECK_EQ(u32(buffer.bytes, 48u), 2u);
  CHECK_EQ(u32(buffer.bytes, 52u), 1u);
  CHECK(buffer.untouched_from(56u)); }

TEST(paging_command_writer_copy_page_table_entries, is_a_header_then_each_range)
{ Buffer buffer;
  PagingCommandWriter writer(buffer.bytes, sizeof buffer.bytes);
  CHECK_STATUS(writer.copy_page_table_entries(), Status::ok);
  CHECK_EQ(writer.length(), PagingCommandWriter::copy_header_size);
  CHECK_EQ(u32(buffer.bytes, 8u), 0u);
  CHECK_STATUS(writer.copy_range(0x1000u, 0x2000u, 5u, 6u, 7u), Status::ok);
  CHECK_STATUS(writer.copy_range(0x3000u, 0x4000u, 8u, 9u, 10u), Status::ok);
  CHECK_EQ(writer.length(), 16u + 2u * PagingCommandWriter::copy_range_size);
  CHECK_EQ(u32(buffer.bytes, 4u), 4u);
  CHECK_EQ(u32(buffer.bytes, 8u), 2u);
  CHECK_EQ(u32(buffer.bytes, 12u), 0u);
  CHECK_EQ(u64(buffer.bytes, 16u), 0x1000u);
  CHECK_EQ(u64(buffer.bytes, 24u), 0x2000u);
  CHECK_EQ(u32(buffer.bytes, 32u), 5u);
  CHECK_EQ(u32(buffer.bytes, 36u), 6u);
  CHECK_EQ(u32(buffer.bytes, 40u), 7u);
  CHECK_EQ(u32(buffer.bytes, 44u), 0u);
  CHECK_EQ(u64(buffer.bytes, 48u), 0x3000u);
  CHECK_EQ(u32(buffer.bytes, 72u), 10u);
  CHECK(buffer.untouched_from(80u)); }

TEST(paging_command_writer_copy_range, needs_a_page_table_copy_to_append_to)
{ Buffer buffer;
  PagingCommandWriter writer(buffer.bytes, sizeof buffer.bytes);
  CHECK_STATUS(writer.copy_range(0x1000u, 0x2000u, 0u, 0u, 1u), Status::invalid_state);
  CHECK_STATUS(writer.fill(0u, 0u, 0u), Status::ok);
  CHECK_STATUS(writer.copy_range(0x1000u, 0x2000u, 0u, 0u, 1u), Status::invalid_state);
  CHECK(buffer.untouched_from(PagingCommandWriter::fill_size)); }

TEST(paging_command_writer_copy_range, a_range_that_does_not_fit_writes_nothing)
{ Buffer buffer;
  PagingCommandWriter writer(buffer.bytes, 16u + PagingCommandWriter::copy_range_size + 31u);
  CHECK_STATUS(writer.copy_page_table_entries(), Status::ok);
  CHECK_STATUS(writer.copy_range(0x1000u, 0x2000u, 0u, 0u, 1u), Status::ok);
  CHECK_STATUS(writer.copy_range(0x1000u, 0x2000u, 0u, 0u, 1u), Status::too_large);
  CHECK_EQ(u32(buffer.bytes, 8u), 1u);
  CHECK_EQ(writer.length(), 48u);
  CHECK(buffer.untouched_from(48u)); }

TEST(paging_command_writer, a_command_that_does_not_fit_writes_nothing)
{ Buffer buffer;
  PagingCommandWriter flush(buffer.bytes, PagingCommandWriter::flush_tlb_size - 1u);
  CHECK_STATUS(flush.flush_tlb(1u, 0u, 0u, 0u), Status::too_large);
  PagingCommandWriter fill(buffer.bytes, PagingCommandWriter::fill_size - 1u);
  CHECK_STATUS(fill.fill(0u, 0u, 0u), Status::too_large);
  PagingCommandWriter transfer(buffer.bytes, PagingCommandWriter::transfer_size - 1u);
  CHECK_STATUS(transfer.transfer(0u, 0u, 0u, 0u, 0u, 0u, 0u), Status::too_large);
  PagingCommandWriter copy(buffer.bytes, PagingCommandWriter::copy_header_size - 1u);
  CHECK_STATUS(copy.copy_page_table_entries(), Status::too_large);
  CHECK_STATUS(copy.copy_range(0u, 0u, 0u, 0u, 0u), Status::invalid_state);
  CHECK_EQ(copy.length(), 0u);
  PagingCommandWriter none(nullptr, sizeof buffer.bytes);
  CHECK_STATUS(none.fill(0u, 0u, 0u), Status::too_large);
  CHECK(buffer.untouched_from(0u)); }
