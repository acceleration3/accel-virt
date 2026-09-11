#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/virtio_gpu/gpu_request_writer.hpp"

#include <cstdint>
#include <cstring>

using av::GpuRequestWriter;
using av::Status;

namespace {

std::uint32_t u32(const std::uint8_t *bytes, std::size_t offset)
{
    return std::uint32_t{bytes[offset]} | std::uint32_t{bytes[offset + 1u]} << 8u |
           std::uint32_t{bytes[offset + 2u]} << 16u | std::uint32_t{bytes[offset + 3u]} << 24u;
}

/* A header with no flags, fence, context or ring. */
bool plain_header(const std::uint8_t *bytes, std::uint32_t type)
{
    for (std::size_t byte = 4u; byte < GpuRequestWriter::header_size; byte++)
        if (bytes[byte] != 0u) return false;
    return u32(bytes, 0u) == type;
}

struct Buffer {
    Buffer() { std::memset(bytes, 0xEE, sizeof bytes); }
    std::uint8_t bytes[GpuRequestWriter::max_request_size + 8u];
};

}  // namespace

TEST(gpu_request_writer_create_2d, is_virtio_gpu_resource_create_2d)
{ Buffer buffer;
  GpuRequestWriter writer(buffer.bytes, sizeof buffer.bytes);
  CHECK_STATUS(writer.create_2d(1u, GpuRequestWriter::format_b8g8r8a8, 64u, 48u), Status::ok);
  CHECK_EQ(writer.length(), 40u);
  CHECK(plain_header(buffer.bytes, 0x0101u));
  CHECK_EQ(u32(buffer.bytes, 24u), 1u);
  CHECK_EQ(u32(buffer.bytes, 28u), 1u);
  CHECK_EQ(u32(buffer.bytes, 32u), 64u);
  CHECK_EQ(u32(buffer.bytes, 36u), 48u);
  CHECK_EQ(buffer.bytes[40], 0xEEu); }

TEST(gpu_request_writer_attach_backing, is_one_memory_entry)
{ Buffer buffer;
  GpuRequestWriter writer(buffer.bytes, sizeof buffer.bytes);
  CHECK_STATUS(writer.attach_backing(1u, 0x0000000123456000ull, 16384u), Status::ok);
  CHECK_EQ(writer.length(), 48u);
  CHECK(plain_header(buffer.bytes, 0x0106u));
  CHECK_EQ(u32(buffer.bytes, 24u), 1u);
  CHECK_EQ(u32(buffer.bytes, 28u), 1u);
  CHECK_EQ(u32(buffer.bytes, 32u), 0x23456000u);
  CHECK_EQ(u32(buffer.bytes, 36u), 1u);
  CHECK_EQ(u32(buffer.bytes, 40u), 16384u);
  CHECK_EQ(u32(buffer.bytes, 44u), 0u); }

TEST(gpu_request_writer_transfer_to_host_2d, covers_the_whole_resource)
{ Buffer buffer;
  GpuRequestWriter writer(buffer.bytes, sizeof buffer.bytes);
  CHECK_STATUS(writer.transfer_to_host_2d(1u, 64u, 64u), Status::ok);
  CHECK_EQ(writer.length(), 56u);
  CHECK(plain_header(buffer.bytes, 0x0105u));
  CHECK_EQ(u32(buffer.bytes, 24u), 0u);
  CHECK_EQ(u32(buffer.bytes, 28u), 0u);
  CHECK_EQ(u32(buffer.bytes, 32u), 64u);
  CHECK_EQ(u32(buffer.bytes, 36u), 64u);
  CHECK_EQ(u32(buffer.bytes, 40u), 0u);
  CHECK_EQ(u32(buffer.bytes, 44u), 0u);
  CHECK_EQ(u32(buffer.bytes, 48u), 1u);
  CHECK_EQ(u32(buffer.bytes, 52u), 0u); }

TEST(gpu_request_writer_update_cursor, names_the_position_resource_and_hot_spot)
{ Buffer buffer;
  GpuRequestWriter writer(buffer.bytes, sizeof buffer.bytes);
  CHECK_STATUS(writer.update_cursor(0u, -3, 200, 1u, 5u, 6u), Status::ok);
  CHECK_EQ(writer.length(), 56u);
  CHECK(plain_header(buffer.bytes, 0x0300u));
  CHECK_EQ(u32(buffer.bytes, 24u), 0u);
  CHECK_EQ(u32(buffer.bytes, 28u), 0xFFFFFFFDu);
  CHECK_EQ(u32(buffer.bytes, 32u), 200u);
  CHECK_EQ(u32(buffer.bytes, 36u), 0u);
  CHECK_EQ(u32(buffer.bytes, 40u), 1u);
  CHECK_EQ(u32(buffer.bytes, 44u), 5u);
  CHECK_EQ(u32(buffer.bytes, 48u), 6u);
  CHECK_EQ(u32(buffer.bytes, 52u), 0u); }

TEST(gpu_request_writer_move_cursor, leaves_the_update_fields_zero)
{ Buffer buffer;
  GpuRequestWriter writer(buffer.bytes, sizeof buffer.bytes);
  CHECK_STATUS(writer.move_cursor(0u, 100, 50), Status::ok);
  CHECK_EQ(writer.length(), 56u);
  CHECK(plain_header(buffer.bytes, 0x0301u));
  CHECK_EQ(u32(buffer.bytes, 28u), 100u);
  CHECK_EQ(u32(buffer.bytes, 32u), 50u);
  CHECK_EQ(u32(buffer.bytes, 40u), 0u); }

TEST(gpu_request_writer, a_request_that_does_not_fit_writes_nothing)
{ Buffer buffer;
  GpuRequestWriter writer(buffer.bytes, 55u);
  CHECK_STATUS(writer.move_cursor(0u, 1, 1), Status::too_large);
  CHECK_EQ(writer.length(), 0u);
  CHECK_EQ(buffer.bytes[0], 0xEEu);
  CHECK_STATUS(writer.create_2d(1u, 1u, 1u, 1u), Status::ok);
  CHECK_EQ(writer.length(), 40u);
  GpuRequestWriter tiny(buffer.bytes, 39u);
  CHECK_STATUS(tiny.create_2d(1u, 1u, 1u, 1u), Status::too_large);
  CHECK_STATUS(tiny.transfer_to_host_2d(1u, 1u, 1u), Status::too_large);
  CHECK_STATUS(tiny.update_cursor(0u, 0, 0, 1u, 0u, 0u), Status::too_large);
  CHECK_EQ(tiny.length(), 0u);
  GpuRequestWriter none(nullptr, 64u);
  CHECK_STATUS(none.attach_backing(1u, 0u, 0u), Status::too_large); }
