#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/resources/guest_page_list.hpp"
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

/* Room for a backing request with several memory entries. */
struct BigBuffer {
    BigBuffer() { std::memset(bytes, 0xEE, sizeof bytes); }
    std::uint8_t bytes[256u];
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

/* One memory entry per run of adjacent pages, each covering the whole run. */
TEST(gpu_request_writer_attach_backing_pages, joins_adjacent_pages_into_one_entry)
{ BigBuffer buffer;
  GpuRequestWriter writer(buffer.bytes, sizeof buffer.bytes);
  const std::uint64_t frames[] = {0x1000u, 0x2000u, 0x9000u};
  CHECK_STATUS(writer.attach_backing_pages(3u, av::GuestPageList(frames, 3u)), Status::ok);
  CHECK_EQ(writer.length(), 64u);
  CHECK(plain_header(buffer.bytes, 0x0106u));
  CHECK_EQ(u32(buffer.bytes, 24u), 3u);
  CHECK_EQ(u32(buffer.bytes, 28u), 2u);
  CHECK_EQ(u32(buffer.bytes, 32u), 0x1000u);
  CHECK_EQ(u32(buffer.bytes, 36u), 0u);
  CHECK_EQ(u32(buffer.bytes, 40u), 0x2000u);
  CHECK_EQ(u32(buffer.bytes, 44u), 0u);
  CHECK_EQ(u32(buffer.bytes, 48u), 0x9000u);
  CHECK_EQ(u32(buffer.bytes, 52u), 0u);
  CHECK_EQ(u32(buffer.bytes, 56u), 0x1000u);
  CHECK_EQ(u32(buffer.bytes, 60u), 0u); }

TEST(gpu_request_writer_attach_backing_pages, refuses_a_list_with_a_page_that_was_never_mapped)
{ BigBuffer buffer;
  GpuRequestWriter writer(buffer.bytes, sizeof buffer.bytes);
  const std::uint64_t frames[] = {0x1000u, 0u};
  CHECK_STATUS(writer.attach_backing_pages(3u, av::GuestPageList(frames, 2u)), Status::invalid_parameter);
  CHECK_EQ(writer.length(), 0u); }

TEST(gpu_request_writer_attach_backing_pages, refuses_an_empty_list)
{ BigBuffer buffer;
  GpuRequestWriter writer(buffer.bytes, sizeof buffer.bytes);
  CHECK_STATUS(writer.attach_backing_pages(3u, av::GuestPageList()), Status::invalid_parameter);
  CHECK_EQ(writer.length(), 0u); }

TEST(gpu_request_writer_attach_backing_pages, refuses_more_runs_than_the_destination_holds)
{ BigBuffer buffer;
  const std::uint64_t frames[] = {0x1000u, 0x3000u, 0x5000u};
  GpuRequestWriter narrow(buffer.bytes, 64u);
  CHECK_STATUS(narrow.attach_backing_pages(3u, av::GuestPageList(frames, 3u)), Status::too_large);
  CHECK_EQ(narrow.length(), 0u); }

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

/* struct virtio_gpu_set_scanout: the rectangle the host shows, then which
 * scanout shows it and from which resource. */
TEST(gpu_request_writer_resource_unref, names_the_resource_the_host_may_forget)
{ Buffer buffer;
  GpuRequestWriter writer(buffer.bytes, sizeof buffer.bytes);
  CHECK_STATUS(writer.resource_unref(7u), Status::ok);
  CHECK_EQ(writer.length(), 32u);
  CHECK(plain_header(buffer.bytes, 0x0102u));
  CHECK_EQ(u32(buffer.bytes, 24u), 7u);
  CHECK_EQ(u32(buffer.bytes, 28u), 0u);
  CHECK_EQ(buffer.bytes[32], 0xEEu); }

/* struct virtio_gpu_resource_detach_backing: the resource, then padding. */
TEST(gpu_request_writer_detach_backing, names_the_resource_that_lets_go_of_its_pages)
{ Buffer buffer;
  GpuRequestWriter writer(buffer.bytes, sizeof buffer.bytes);
  CHECK_STATUS(writer.detach_backing(9u), Status::ok);
  CHECK_EQ(writer.length(), 32u);
  CHECK(plain_header(buffer.bytes, 0x0107u));
  CHECK_EQ(u32(buffer.bytes, 24u), 9u);
  CHECK_EQ(u32(buffer.bytes, 28u), 0u);
  CHECK_EQ(buffer.bytes[32], 0xEEu);
  GpuRequestWriter header_only(buffer.bytes, GpuRequestWriter::header_size - 1u);
  CHECK_STATUS(header_only.detach_backing(9u), Status::too_large); }

/* rvk's own request: the resource, the frame the display clock shows it as
 * (0: at once), and the key the host image was published under, which the
 * host shows in place of a transfer from the guest's pages. */
TEST(gpu_request_writer_scanout_image, names_the_resource_the_frame_and_the_host_image_key)
{ Buffer buffer;
  GpuRequestWriter writer(buffer.bytes, sizeof buffer.bytes);
  CHECK_STATUS(writer.scanout_image(9u, 0x1122334455667788ull, 0xA1B2C3D4u), Status::ok);
  CHECK_EQ(writer.length(), 40u);
  CHECK(plain_header(buffer.bytes, 0x02F0u));
  CHECK_EQ(u32(buffer.bytes, 24u), 9u);
  CHECK_EQ(u32(buffer.bytes, 28u), 0xA1B2C3D4u);
  CHECK_EQ(u32(buffer.bytes, 32u), 0x55667788u);
  CHECK_EQ(u32(buffer.bytes, 36u), 0x11223344u);
  CHECK_EQ(buffer.bytes[40], 0xEEu);
  CHECK_STATUS(writer.scanout_image(9u, 1u, 0u), Status::ok);
  CHECK_EQ(u32(buffer.bytes, 28u), 0u);
  GpuRequestWriter short_of_the_key(buffer.bytes, 39u);
  CHECK_STATUS(short_of_the_key.scanout_image(9u, 1u, 1u), Status::too_large); }

TEST(gpu_request_writer_set_scanout, names_the_scanout_the_resource_and_the_rectangle)
{ Buffer buffer;
  GpuRequestWriter writer(buffer.bytes, sizeof buffer.bytes);
  CHECK_STATUS(writer.set_scanout(1u, 7u, 1280u, 800u), Status::ok);
  CHECK_EQ(writer.length(), 48u);
  CHECK(plain_header(buffer.bytes, 0x0103u));
  CHECK_EQ(u32(buffer.bytes, 24u), 0u);
  CHECK_EQ(u32(buffer.bytes, 28u), 0u);
  CHECK_EQ(u32(buffer.bytes, 32u), 1280u);
  CHECK_EQ(u32(buffer.bytes, 36u), 800u);
  CHECK_EQ(u32(buffer.bytes, 40u), 1u);
  CHECK_EQ(u32(buffer.bytes, 44u), 7u);
  CHECK_EQ(buffer.bytes[48], 0xEEu); }

/* A scanout of resource 0 is how virtio-gpu is told to show nothing. */
TEST(gpu_request_writer_set_scanout, disables_a_scanout_with_resource_zero)
{ Buffer buffer;
  GpuRequestWriter writer(buffer.bytes, sizeof buffer.bytes);
  CHECK_STATUS(writer.set_scanout(0u, 0u, 0u, 0u), Status::ok);
  CHECK_EQ(u32(buffer.bytes, 32u), 0u);
  CHECK_EQ(u32(buffer.bytes, 44u), 0u); }

TEST(gpu_request_writer_resource_flush, covers_the_rectangle_of_one_resource)
{ Buffer buffer;
  GpuRequestWriter writer(buffer.bytes, sizeof buffer.bytes);
  CHECK_STATUS(writer.resource_flush(7u, 1280u, 800u), Status::ok);
  CHECK_EQ(writer.length(), 48u);
  CHECK(plain_header(buffer.bytes, 0x0104u));
  CHECK_EQ(u32(buffer.bytes, 24u), 0u);
  CHECK_EQ(u32(buffer.bytes, 28u), 0u);
  CHECK_EQ(u32(buffer.bytes, 32u), 1280u);
  CHECK_EQ(u32(buffer.bytes, 36u), 800u);
  CHECK_EQ(u32(buffer.bytes, 40u), 7u);
  CHECK_EQ(u32(buffer.bytes, 44u), 0u); }

TEST(gpu_request_writer_get_display_info, is_a_header_and_nothing_else)
{ Buffer buffer;
  GpuRequestWriter writer(buffer.bytes, sizeof buffer.bytes);
  CHECK_STATUS(writer.get_display_info(), Status::ok);
  CHECK_EQ(writer.length(), GpuRequestWriter::header_size);
  CHECK(plain_header(buffer.bytes, 0x0100u));
  CHECK_EQ(buffer.bytes[GpuRequestWriter::header_size], 0xEEu); }

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
  CHECK_STATUS(tiny.set_scanout(0u, 1u, 1u, 1u), Status::too_large);
  CHECK_STATUS(tiny.resource_flush(1u, 1u, 1u), Status::too_large);
  /* The scanout blob is the longest of them, so the buffer every other
   * request fits is still too small for it. */
  CHECK_STATUS(writer.set_scanout_blob(0u, 1u, 64u, 64u, 256u), Status::too_large);
  CHECK_STATUS(tiny.set_scanout_blob(0u, 1u, 64u, 64u, 256u), Status::too_large);
  CHECK_EQ(tiny.length(), 0u);
  /* An unref is 32 bytes, so it fits where the bigger requests do not. */
  CHECK_STATUS(tiny.resource_unref(1u), Status::ok);
  CHECK_EQ(tiny.length(), 32u);
  GpuRequestWriter header_only(buffer.bytes, GpuRequestWriter::header_size - 1u);
  CHECK_STATUS(header_only.get_display_info(), Status::too_large);
  CHECK_STATUS(header_only.resource_unref(1u), Status::too_large);
  CHECK_EQ(header_only.length(), 0u);
  GpuRequestWriter none(nullptr, 64u);
  CHECK_STATUS(none.attach_backing(1u, 0u, 0u), Status::too_large); }
