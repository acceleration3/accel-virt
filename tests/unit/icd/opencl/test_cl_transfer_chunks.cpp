#include "harness/framework/test_registry.hpp"
#include "harness/icd/cl_transfer_host.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opencl/cl_arena_blocks.hpp"
#include "icd/opencl/cl_buffer_rect.hpp"
#include "icd/opencl/cl_image_region.hpp"
#include "icd/opencl/cl_staging_block.hpp"
#include "icd/opencl/cl_transfer_chunks.hpp"
#include "proto/generated/opencl/cl_protocol_ids.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

using av::ClTransferHost;
using av::clicd::ClArenaBlocks;
using av::clicd::ClBufferRect;
using av::clicd::ClImageRegion;
using av::clicd::ClTransferChunks;

namespace {

constexpr std::uint64_t queue = 0x6500u;

av::LockTrace           trace;
av::RecordingThreadLock lock(trace, 'B');
av::MockPlatform        platform;
ClArenaBlocks inline_only(platform, lock);
constexpr std::size_t   element = ClTransferHost::element_bytes;

class Memory {
public:
    explicit Memory(std::size_t bytes) : bytes_(static_cast<std::uint8_t *>(std::malloc(bytes))), size_(bytes)
    {
        std::memset(bytes_, 0xEE, size_);
    }
    ~Memory() { std::free(bytes_); }
    Memory(const Memory &) = delete;
    Memory &operator=(const Memory &) = delete;

    std::uint8_t *at(std::size_t offset) { return bytes_ + offset; }
    std::uint32_t texel(std::size_t offset) const
    {
        std::uint32_t value = 0u;
        std::memcpy(&value, bytes_ + offset, sizeof value);
        return value;
    }

private:
    std::uint8_t *bytes_;
    std::size_t   size_;
};

}

TEST(cl_transfer_chunks, a_read_that_fits_one_reply_is_one_blocking_command)
{ ClTransferHost host;
  host.fill(3u);
  ClTransferChunks chunks(host, queue, inline_only);
  Memory into(100u);
  CHECK_EQ(chunks.read(ClTransferHost::buffer_handle, 1000u, 100u, into.at(0u)), CL_SUCCESS);
  for (std::size_t index = 0u; index < 100u; ++index) CHECK_EQ(*into.at(index), host.buffer_at(1000u + index));
  CHECK_EQ(chunks.commands(), 1u);
  CHECK_EQ(host.commands(), 1u);
  CHECK_EQ(host.command(0u).id, av::clproto::kCmd_clEnqueueReadBuffer);
  CHECK(host.command(0u).blocking); }

TEST(cl_transfer_chunks, a_read_larger_than_a_reply_is_cut_into_pieces_that_cover_it_once_in_order)
{ ClTransferHost host;
  host.fill(11u);
  ClTransferChunks  chunks(host, queue, inline_only);
  const std::size_t bytes = 3u * ClTransferChunks::read_room + 17u;
  Memory            into(bytes);
  CHECK_EQ(chunks.read(ClTransferHost::buffer_handle, 777u, bytes, into.at(0u)), CL_SUCCESS);
  bool every_byte = true;
  for (std::size_t index = 0u; index < bytes; ++index)
      every_byte = every_byte && *into.at(index) == host.buffer_at(777u + index);
  CHECK(every_byte);
  CHECK_EQ(host.commands(), 4u);
  std::uint64_t next = 777u;
  for (std::uint32_t index = 0u; index < host.commands(); ++index) {
      CHECK_EQ(host.command(index).offset, next);
      CHECK(host.command(index).bytes <= ClTransferChunks::read_room);
      next += host.command(index).bytes;
  }
  CHECK_EQ(next, 777u + bytes);
  CHECK_EQ(host.command(3u).bytes, 17u); }

TEST(cl_transfer_chunks, a_write_larger_than_the_staging_room_is_cut_into_pieces_that_fit_it)
{ ClTransferHost host;
  ClTransferChunks  chunks(host, queue, inline_only);
  const std::size_t bytes = 2u * ClTransferChunks::write_room + 5u;
  Memory            from(bytes);
  for (std::size_t index = 0u; index < bytes; ++index) *from.at(index) = static_cast<std::uint8_t>(index * 7u);
  CHECK_EQ(chunks.write(ClTransferHost::buffer_handle, 3u, bytes, from.at(0u)), CL_SUCCESS);
  bool every_byte = true;
  for (std::size_t index = 0u; index < bytes; ++index)
      every_byte = every_byte && host.buffer_at(3u + index) == static_cast<std::uint8_t>(index * 7u);
  CHECK(every_byte);
  CHECK_EQ(host.commands(), 3u);
  CHECK_EQ(host.command(0u).bytes, ClTransferChunks::write_room);
  CHECK_EQ(host.command(1u).offset, 3u + ClTransferChunks::write_room);
  CHECK_EQ(host.command(2u).bytes, 5u);
  for (std::uint32_t index = 0u; index < host.commands(); ++index) CHECK(host.command(index).blocking); }

TEST(cl_transfer_chunks, a_refused_piece_ends_the_transfer_with_the_hosts_error)
{ ClTransferHost host;
  host.refuse(1u, CL_INVALID_VALUE);
  ClTransferChunks  chunks(host, queue, inline_only);
  const std::size_t bytes = 3u * ClTransferChunks::read_room;
  Memory            into(bytes);
  CHECK_EQ(chunks.read(ClTransferHost::buffer_handle, 0u, bytes, into.at(0u)), CL_INVALID_VALUE);
  CHECK_EQ(host.commands(), 2u); }

TEST(cl_transfer_chunks, a_write_sends_every_piece_whatever_the_host_answers_one)
{ ClTransferHost refusing;
  refusing.refuse(0u, CL_MEM_OBJECT_ALLOCATION_FAILURE);
  ClTransferChunks  writes(refusing, queue, inline_only);
  const std::size_t bytes = 2u * ClTransferChunks::write_room + 5u;
  Memory            from(bytes);
  std::memset(from.at(0u), 0x5A, bytes);
  CHECK_EQ(writes.write(ClTransferHost::buffer_handle, 0u, bytes, from.at(0u)), CL_SUCCESS);
  CHECK_EQ(refusing.commands(), 3u);
  CHECK_EQ(writes.commands(), 3u);
  CHECK(refusing.buffer_at(0u) != 0x5Au);
  CHECK_EQ(refusing.buffer_at(ClTransferChunks::write_room), 0x5Au);
  CHECK_EQ(refusing.buffer_at(bytes - 1u), 0x5Au); }

TEST(cl_transfer_chunks, a_transport_that_fails_is_the_apis_out_of_resources)
{ ClTransferHost host;
  ClTransferChunks chunks(host, queue, inline_only);
  std::uint8_t     byte = 0u;
  CHECK_EQ(chunks.read(ClTransferHost::buffer_handle, ClTransferHost::buffer_bytes, 1u, &byte), CL_OUT_OF_RESOURCES);
  CHECK_EQ(chunks.write(ClTransferHost::buffer_handle, ClTransferHost::buffer_bytes, 1u, &byte),
           CL_OUT_OF_RESOURCES); }

TEST(cl_transfer_chunks, a_transfer_of_nothing_sends_nothing)
{ ClTransferHost   host;
  ClTransferChunks chunks(host, queue, inline_only);
  std::uint8_t     byte = 0u;
  CHECK_EQ(chunks.read(ClTransferHost::buffer_handle, 0u, 0u, &byte), CL_SUCCESS);
  CHECK_EQ(chunks.write(ClTransferHost::buffer_handle, 0u, 0u, &byte), CL_SUCCESS);
  CHECK_EQ(host.commands(), 0u); }

TEST(cl_transfer_chunks, packed_image_rows_come_back_as_runs_that_fit_a_reply)
{ ClTransferHost host;
  host.fill(0u);
  ClTransferChunks  chunks(host, queue, inline_only);
  constexpr std::size_t width = 60000u;
  const std::size_t   origin[3] = {100u, 0u, 1u}, extent[3] = {width, 16u, 1u};
  const ClImageRegion region(origin, extent, 0u, 0u, element);
  Memory              into(16u * width * element);
  CHECK_EQ(chunks.read_image(ClTransferHost::image_handle, region, into.at(0u)), CL_SUCCESS);
  bool every_texel = true;
  for (std::size_t y = 0u; y < 16u; ++y)
      for (std::size_t x = 0u; x < width; ++x)
          every_texel = every_texel && into.texel((y * width + x) * element) == host.texel_of(100u + x, y, 1u);
  CHECK(every_texel);
  const std::size_t per_command = ClTransferChunks::read_room / (width * element);
  CHECK_EQ(per_command, 4u);
  CHECK_EQ(host.command(0u).region[1], per_command);
  CHECK_EQ(host.commands(), static_cast<std::uint32_t>((16u + per_command - 1u) / per_command));
  CHECK_EQ(host.command(1u).origin[1], per_command); }

TEST(cl_transfer_chunks, image_rows_with_a_gap_come_back_one_at_a_time_and_the_gap_is_left_alone)
{ ClTransferHost host;
  host.fill(0u);
  ClTransferChunks    chunks(host, queue, inline_only);
  const std::size_t   origin[3] = {0u, 2u, 0u}, extent[3] = {8u, 5u, 1u};
  const std::size_t   pitch = 8u * element + 12u;
  const ClImageRegion region(origin, extent, pitch, 0u, element);
  Memory              into(5u * pitch);
  CHECK_EQ(chunks.read_image(ClTransferHost::image_handle, region, into.at(0u)), CL_SUCCESS);
  CHECK_EQ(host.commands(), 5u);
  for (std::size_t y = 0u; y < 5u; ++y) {
      CHECK_EQ(into.texel(y * pitch), host.texel_of(0u, 2u + y, 0u));
      CHECK_EQ(into.texel(y * pitch + 7u * element), host.texel_of(7u, 2u + y, 0u));
      CHECK_EQ(*into.at(y * pitch + 8u * element), 0xEEu);
      CHECK_EQ(*into.at(y * pitch + pitch - 1u), 0xEEu);
  } }

TEST(cl_transfer_chunks, an_image_row_longer_than_a_reply_comes_back_in_pieces_of_whole_texels)
{ ClTransferHost host;
  host.fill(0u);
  ClTransferChunks    chunks(host, queue, inline_only);
  const std::size_t   width = ClTransferChunks::read_room / element + 100u;
  const std::size_t   origin[3] = {10u, 3u, 2u}, extent[3] = {width, 2u, 1u};
  const ClImageRegion region(origin, extent, 0u, 0u, element);
  Memory              into(2u * width * element);
  CHECK_EQ(chunks.read_image(ClTransferHost::image_handle, region, into.at(0u)), CL_SUCCESS);
  CHECK_EQ(host.commands(), 4u);
  CHECK_EQ(host.command(0u).region[0], ClTransferChunks::read_room / element);
  CHECK_EQ(host.command(1u).region[0], 100u);
  CHECK_EQ(host.command(1u).origin[0], 10u + ClTransferChunks::read_room / element);
  bool every_texel = true;
  for (std::size_t y = 0u; y < 2u; ++y)
      for (std::size_t x = 0u; x < width; ++x)
          every_texel = every_texel && into.texel((y * width + x) * element) == host.texel_of(10u + x, 3u + y, 2u);
  CHECK(every_texel); }

TEST(cl_transfer_chunks, wide_image_rows_with_a_gap_come_back_in_runs_and_leave_the_gap)
{ ClTransferHost host;
  host.fill(0u);
  ClTransferChunks    chunks(host, queue, inline_only);
  const std::size_t   width = ClTransferChunks::read_room / element + 3u;
  const std::size_t   origin[3] = {0u, 1u, 0u}, extent[3] = {width, 2u, 1u};
  const std::size_t   pitch = width * element + 8u;
  const ClImageRegion region(origin, extent, pitch, 0u, element);
  Memory              into(2u * pitch);
  CHECK_EQ(chunks.read_image(ClTransferHost::image_handle, region, into.at(0u)), CL_SUCCESS);
  CHECK_EQ(host.commands(), 4u);
  CHECK_EQ(host.command(1u).region[0], 3u);
  CHECK_EQ(host.command(2u).origin[1], 2u);
  for (std::size_t y = 0u; y < 2u; ++y) {
      CHECK_EQ(into.texel(y * pitch), host.texel_of(0u, 1u + y, 0u));
      CHECK_EQ(into.texel(y * pitch + (width - 1u) * element), host.texel_of(width - 1u, 1u + y, 0u));
      CHECK_EQ(*into.at(y * pitch + width * element), 0xEEu);
      CHECK_EQ(*into.at(y * pitch + pitch - 1u), 0xEEu);
  } }

TEST(cl_transfer_chunks, image_rows_go_to_the_host_in_runs_at_the_applications_own_pitch)
{ ClTransferHost host;
  ClTransferChunks    chunks(host, queue, inline_only);
  const std::size_t   width = 20000u;
  const std::size_t   pitch = width * element + 256u;
  const std::size_t   origin[3] = {5u, 1u, 0u}, extent[3] = {width, 10u, 2u};
  const ClImageRegion region(origin, extent, pitch, 0u, element);
  Memory              from(2u * 10u * pitch);
  for (std::size_t z = 0u; z < 2u; ++z)
      for (std::size_t y = 0u; y < 10u; ++y)
          for (std::size_t x = 0u; x < width; ++x) {
              const std::uint32_t value = host.texel_of(x, y, z) ^ 0x5A5A5A5Au;
              std::memcpy(from.at((z * 10u + y) * pitch + x * element), &value, sizeof value);
          }
  CHECK_EQ(chunks.write_image(ClTransferHost::image_handle, region, from.at(0u)), CL_SUCCESS);
  bool every_texel = true;
  for (std::size_t z = 0u; z < 2u; ++z)
      for (std::size_t y = 0u; y < 10u; ++y)
          for (std::size_t x = 0u; x < width; ++x)
              every_texel = every_texel && host.texel(5u + x, 1u + y, z) == (host.texel_of(x, y, z) ^ 0x5A5A5A5Au);
  CHECK(every_texel);
  const std::size_t per_command = 1u + (ClTransferChunks::write_room - width * element) / pitch;
  CHECK_EQ(per_command, 3u);
  CHECK_EQ(host.commands(), 8u);
  CHECK_EQ(host.command(0u).row_pitch, pitch);
  CHECK_EQ(host.command(0u).bytes, 2u * pitch + width * element);
  CHECK_EQ(host.command(3u).region[1], 1u);
  CHECK_EQ(host.command(4u).origin[2], 1u); }

TEST(cl_transfer_chunks, an_image_row_longer_than_the_staging_room_goes_in_pieces_of_whole_texels)
{ ClTransferHost host;
  ClTransferChunks    chunks(host, queue, inline_only);
  const std::size_t   width = ClTransferChunks::write_room / element + 1000u;
  const std::size_t   origin[3] = {0u, 7u, 0u}, extent[3] = {width, 1u, 1u};
  const ClImageRegion region(origin, extent, 0u, 0u, element);
  Memory              from(width * element);
  for (std::size_t x = 0u; x < width; ++x) {
      const std::uint32_t value = static_cast<std::uint32_t>(x * 3u);
      std::memcpy(from.at(x * element), &value, sizeof value);
  }
  CHECK_EQ(chunks.write_image(ClTransferHost::image_handle, region, from.at(0u)), CL_SUCCESS);
  CHECK_EQ(host.commands(), 2u);
  CHECK_EQ(host.command(0u).region[0], ClTransferChunks::write_room / element);
  CHECK_EQ(host.command(1u).origin[0], ClTransferChunks::write_room / element);
  bool every_texel = true;
  for (std::size_t x = 0u; x < width; ++x) every_texel = every_texel && host.texel(x, 7u, 0u) == x * 3u;
  CHECK(every_texel); }

TEST(cl_transfer_chunks, a_refused_image_piece_ends_the_transfer_with_the_hosts_error)
{ const std::size_t   long_row[3] = {ClTransferHost::image_width, 2u, 1u};
  const std::size_t   short_rows[3] = {16u, 8u, 1u};
  const std::size_t   origin[3] = {0u, 0u, 0u};
  const ClImageRegion wide(origin, long_row, 0u, 0u, element);
  const ClImageRegion narrow(origin, short_rows, 0u, 0u, element);
  Memory              memory(2u * ClTransferHost::image_width * element);
  { ClTransferHost host;
    host.refuse(1u, CL_INVALID_OPERATION);
    ClTransferChunks chunks(host, queue, inline_only);
    CHECK_EQ(chunks.read_image(ClTransferHost::image_handle, wide, memory.at(0u)), CL_INVALID_OPERATION);
    CHECK_EQ(host.commands(), 2u); }
  { ClTransferHost host;
    host.refuse(0u, CL_INVALID_OPERATION);
    ClTransferChunks chunks(host, queue, inline_only);
    CHECK_EQ(chunks.read_image(ClTransferHost::image_handle, narrow, memory.at(0u)), CL_INVALID_OPERATION);
    CHECK_EQ(host.commands(), 1u); }
  { ClTransferHost host;
    host.refuse(0u, CL_INVALID_OPERATION);
    ClTransferChunks chunks(host, queue, inline_only);
    CHECK_EQ(chunks.write_image(ClTransferHost::image_handle, wide, memory.at(0u)), CL_INVALID_OPERATION);
    CHECK_EQ(host.commands(), 1u); }
  { ClTransferHost host;
    host.refuse(0u, CL_INVALID_OPERATION);
    ClTransferChunks chunks(host, queue, inline_only);
    CHECK_EQ(chunks.write_image(ClTransferHost::image_handle, narrow, memory.at(0u)), CL_INVALID_OPERATION);
    CHECK_EQ(host.commands(), 1u); } }

TEST(cl_transfer_chunks, packed_rectangle_rows_come_back_as_runs_that_fit_a_reply)
{ ClTransferHost host;
  host.fill(5u);
  ClTransferChunks   chunks(host, queue, inline_only);
  constexpr std::size_t width = 3000u, rows = 600u, pitch = 4096u;
  const std::size_t  corner[3] = {100u, 2u, 0u}, host_corner[3] = {0u, 0u, 0u}, extent[3] = {width, rows, 1u};
  const ClBufferRect rect(corner, host_corner, extent, pitch, 0u, 0u, 0u);
  CHECK_EQ(rect.check(), CL_SUCCESS);
  Memory into(rows * width);
  CHECK_EQ(chunks.read_buffer_rect(ClTransferHost::buffer_handle, rect, into.at(0u)), CL_SUCCESS);
  bool every_byte = true;
  for (std::size_t y = 0u; y < rows; ++y)
      for (std::size_t x = 0u; x < width; ++x)
          every_byte = every_byte && *into.at(y * width + x) == host.buffer_at((2u + y) * pitch + 100u + x);
  CHECK(every_byte);
  const std::size_t per_command = ClTransferChunks::read_room / width;
  CHECK_EQ(host.commands(), static_cast<std::uint32_t>((rows + per_command - 1u) / per_command));
  CHECK_EQ(host.command(0u).id, av::clproto::kCmd_clEnqueueReadBufferRect);
  CHECK(host.command(0u).blocking);
  CHECK_EQ(host.command(0u).region[1], per_command);
  CHECK_EQ(host.command(0u).buffer_pitch[0], pitch);
  CHECK_EQ(host.command(1u).origin[1], 2u + per_command); }

TEST(cl_transfer_chunks, rectangle_rows_with_a_gap_come_back_one_at_a_time_and_the_gap_is_left_alone)
{ ClTransferHost host;
  host.fill(9u);
  ClTransferChunks   chunks(host, queue, inline_only);
  const std::size_t  corner[3] = {7u, 3u, 1u}, host_corner[3] = {2u, 1u, 0u}, extent[3] = {20u, 5u, 1u};
  const ClBufferRect rect(corner, host_corner, extent, 64u, 1024u, 32u, 0u);
  CHECK_EQ(rect.check(), CL_SUCCESS);
  Memory into(8u * 32u);
  CHECK_EQ(chunks.read_buffer_rect(ClTransferHost::buffer_handle, rect, into.at(0u)), CL_SUCCESS);
  CHECK_EQ(host.commands(), 5u);
  for (std::size_t y = 0u; y < 5u; ++y) {
      const std::size_t row = (1u + y) * 32u + 2u;
      CHECK_EQ(*into.at(row), host.buffer_at(1024u + (3u + y) * 64u + 7u));
      CHECK_EQ(*into.at(row + 19u), host.buffer_at(1024u + (3u + y) * 64u + 26u));
      CHECK_EQ(*into.at(row - 1u), 0xEEu);
      CHECK_EQ(*into.at(row + 20u), 0xEEu);
  }
  CHECK_EQ(*into.at(0u), 0xEEu);
  CHECK_EQ(host.command(4u).origin[1], 7u);
  CHECK_EQ(host.command(4u).origin[2], 1u);
  CHECK_EQ(host.command(4u).buffer_pitch[1], 1024u); }

TEST(cl_transfer_chunks, a_rectangle_row_longer_than_a_reply_comes_back_in_runs)
{ ClTransferHost host;
  host.fill(1u);
  ClTransferChunks   chunks(host, queue, inline_only);
  const std::size_t  width = ClTransferChunks::read_room + 10u;
  const std::size_t  corner[3] = {3u, 0u, 0u}, host_corner[3] = {0u, 0u, 0u}, extent[3] = {width, 2u, 1u};
  const ClBufferRect rect(corner, host_corner, extent, width + 16u, 0u, 0u, 0u);
  Memory             into(2u * width);
  CHECK_EQ(chunks.read_buffer_rect(ClTransferHost::buffer_handle, rect, into.at(0u)), CL_SUCCESS);
  CHECK_EQ(host.commands(), 4u);
  CHECK_EQ(host.command(1u).region[0], 10u);
  CHECK_EQ(host.command(1u).origin[0], 3u + ClTransferChunks::read_room);
  CHECK_EQ(*into.at(width - 1u), host.buffer_at(3u + width - 1u));
  CHECK_EQ(*into.at(width), host.buffer_at(width + 16u + 3u)); }

TEST(cl_transfer_chunks, rectangle_rows_go_to_the_buffer_at_its_pitch_from_the_applications_corner)
{ ClTransferHost host;
  host.fill(0u);
  ClTransferChunks   chunks(host, queue, inline_only);
  const std::size_t  corner[3] = {16u, 4u, 1u}, host_corner[3] = {8u, 1u, 0u}, extent[3] = {400u, 10u, 2u};
  const ClBufferRect rect(corner, host_corner, extent, 512u, 10240u, 448u, 448u * 12u);
  CHECK_EQ(rect.check(), CL_SUCCESS);
  Memory from(2u * 448u * 12u);
  for (std::size_t index = 0u; index < 2u * 448u * 12u; ++index) *from.at(index) = static_cast<std::uint8_t>(index * 7u);
  CHECK_EQ(chunks.write_buffer_rect(ClTransferHost::buffer_handle, rect, from.at(0u)), CL_SUCCESS);
  bool every_byte = true;
  for (std::size_t z = 0u; z < 2u; ++z)
      for (std::size_t y = 0u; y < 10u; ++y)
          for (std::size_t x = 0u; x < 400u; ++x)
              every_byte = every_byte && host.buffer_at((1u + z) * 10240u + (4u + y) * 512u + 16u + x) ==
                                             *from.at(z * 448u * 12u + (1u + y) * 448u + 8u + x);
  CHECK(every_byte);
  CHECK_EQ(host.buffer_at(10240u + 4u * 512u + 15u), static_cast<std::uint8_t>(10240u + 4u * 512u + 15u));
  CHECK_EQ(host.buffer_at(10240u + 4u * 512u + 416u), static_cast<std::uint8_t>(10240u + 4u * 512u + 416u));
  CHECK_EQ(host.commands(), 2u);
  CHECK_EQ(host.command(0u).id, av::clproto::kCmd_clEnqueueWriteBufferRect);
  CHECK_EQ(host.command(0u).row_pitch, 448u);
  CHECK_EQ(host.command(0u).bytes, 9u * 448u + 400u);
  CHECK_EQ(host.command(1u).origin[2], 2u); }

TEST(cl_transfer_chunks, a_rectangle_row_longer_than_the_staging_room_goes_in_packed_runs)
{ ClTransferHost host;
  ClTransferChunks   chunks(host, queue, inline_only);
  const std::size_t  width = ClTransferChunks::write_room + 5u;
  const std::size_t  corner[3] = {0u, 1u, 0u}, host_corner[3] = {0u, 0u, 0u}, extent[3] = {width, 1u, 1u};
  const ClBufferRect rect(corner, host_corner, extent, width, 0u, 0u, 0u);
  Memory             from(width);
  for (std::size_t index = 0u; index < width; ++index) *from.at(index) = static_cast<std::uint8_t>(index ^ 0x5Au);
  CHECK_EQ(chunks.write_buffer_rect(ClTransferHost::buffer_handle, rect, from.at(0u)), CL_SUCCESS);
  CHECK_EQ(host.commands(), 2u);
  CHECK_EQ(host.command(1u).region[0], 5u);
  CHECK_EQ(host.command(1u).row_pitch, 0u);
  CHECK_EQ(host.buffer_at(width + width - 1u), static_cast<std::uint8_t>((width - 1u) ^ 0x5Au)); }

TEST(cl_transfer_chunks, a_refused_rectangle_piece_ends_the_transfer_with_the_hosts_error)
{ ClTransferHost host;
  ClTransferChunks   chunks(host, queue, inline_only);
  const std::size_t  corner[3] = {0u, 0u, 0u}, extent[3] = {16u, 4u, 1u};
  const ClBufferRect rect(corner, corner, extent, 32u, 0u, 32u, 0u);
  Memory             memory(4u * 32u);
  host.refuse(0u, CL_INVALID_VALUE);
  CHECK_EQ(chunks.read_buffer_rect(ClTransferHost::buffer_handle, rect, memory.at(0u)), CL_INVALID_VALUE);
  CHECK_EQ(host.commands(), 1u);
  host.refuse(1u, CL_OUT_OF_RESOURCES);
  CHECK_EQ(chunks.write_buffer_rect(ClTransferHost::buffer_handle, rect, memory.at(0u)), CL_OUT_OF_RESOURCES);
  CHECK_EQ(host.commands(), 2u); }

namespace {
class Arena {
public:
    Arena(ClTransferHost &host, std::size_t bytes)
        : memory_(static_cast<std::uint8_t *>(std::aligned_alloc(4096u, bytes))), blocks_(platform, lock)
    {
        blocks_.lend(memory_, bytes);
        host.lend(memory_, bytes);
    }
    ~Arena() { std::free(memory_); }
    Arena(const Arena &) = delete;
    Arena &operator=(const Arena &) = delete;

    ClArenaBlocks &blocks() { return blocks_; }

private:
    std::uint8_t *memory_;
    ClArenaBlocks blocks_;
};

}

TEST(cl_transfer_chunks, a_read_with_room_in_the_arena_is_one_lent_command)
{ ClTransferHost host;
  host.fill(3u);
  Arena             arena(host, 8u << 20);
  ClTransferChunks  chunks(host, queue, arena.blocks());
  const std::size_t bytes = 3u << 20;
  Memory            into(bytes);
  CHECK_EQ(chunks.read(ClTransferHost::buffer_handle, 1000u, bytes, into.at(0u)), CL_SUCCESS);
  bool every_byte = true;
  for (std::size_t index = 0u; index < bytes; ++index)
      every_byte = every_byte && *into.at(index) == host.buffer_at(1000u + index);
  CHECK(every_byte);
  CHECK_EQ(host.commands(), 1u);
  CHECK(host.command(0u).lent);
  CHECK(host.command(0u).blocking);
  CHECK_EQ(host.command(0u).bytes, bytes);
  CHECK_EQ(arena.blocks().live(), 0u); }

TEST(cl_transfer_chunks, a_crowded_arena_stages_a_block_at_a_time)
{ ClTransferHost host;
  host.fill(7u);
  Arena             arena(host, 1u << 20);
  ClTransferChunks  chunks(host, queue, arena.blocks());
  const std::size_t bytes = 3u << 20, block = 768u << 10;
  Memory            into(bytes);
  CHECK_EQ(chunks.read(ClTransferHost::buffer_handle, 5u, bytes, into.at(0u)), CL_SUCCESS);
  bool every_byte = true;
  for (std::size_t index = 0u; index < bytes; ++index)
      every_byte = every_byte && *into.at(index) == host.buffer_at(5u + index);
  CHECK(every_byte);
  CHECK_EQ(host.commands(), 4u);
  for (std::uint32_t index = 0u; index < host.commands(); ++index) {
      CHECK(host.command(index).lent);
      CHECK_EQ(host.command(index).offset, 5u + index * block);
      CHECK_EQ(host.command(index).bytes, block);
  }
  CHECK_EQ(arena.blocks().live(), 0u); }

TEST(cl_transfer_chunks, a_staged_write_lands_whole_and_a_refused_piece_ends_it)
{ { ClTransferHost host;
    Arena             arena(host, 8u << 20);
    ClTransferChunks  chunks(host, queue, arena.blocks());
    const std::size_t bytes = 2u << 20;
    Memory            from(bytes);
    for (std::size_t index = 0u; index < bytes; ++index) *from.at(index) = static_cast<std::uint8_t>(index * 13u);
    CHECK_EQ(chunks.write(ClTransferHost::buffer_handle, 64u, bytes, from.at(0u)), CL_SUCCESS);
    bool every_byte = true;
    for (std::size_t index = 0u; index < bytes; ++index)
        every_byte = every_byte && host.buffer_at(64u + index) == *from.at(index);
    CHECK(every_byte);
    CHECK_EQ(host.commands(), 1u);
    CHECK(host.command(0u).lent);
    CHECK_EQ(arena.blocks().live(), 0u); }
  { ClTransferHost host;
    host.refuse(0u, CL_MEM_OBJECT_ALLOCATION_FAILURE);
    Arena            arena(host, 1u << 20);
    ClTransferChunks chunks(host, queue, arena.blocks());
    Memory           from(3u << 20);
    CHECK_EQ(chunks.write(ClTransferHost::buffer_handle, 0u, 3u << 20, from.at(0u)), CL_MEM_OBJECT_ALLOCATION_FAILURE);
    CHECK_EQ(host.commands(), 1u);
    CHECK_EQ(arena.blocks().live(), 0u); }
  { ClTransferHost host;
    host.refuse(1u, CL_INVALID_VALUE);
    Arena            arena(host, 1u << 20);
    ClTransferChunks chunks(host, queue, arena.blocks());
    Memory           into(3u << 20);
    CHECK_EQ(chunks.read(ClTransferHost::buffer_handle, 0u, 3u << 20, into.at(0u)), CL_INVALID_VALUE);
    CHECK_EQ(host.commands(), 2u);
    CHECK_EQ(arena.blocks().live(), 0u); } }

TEST(cl_transfer_chunks, a_small_transfer_or_a_full_arena_goes_inline)
{ ClTransferHost host;
  host.fill(1u);
  { Arena            arena(host, 8u << 20);
    ClTransferChunks chunks(host, queue, arena.blocks());
    Memory           into(av::clicd::ClStagingBlock::least - 1u);
    CHECK_EQ(chunks.read(ClTransferHost::buffer_handle, 0u, av::clicd::ClStagingBlock::least - 1u, into.at(0u)),
             CL_SUCCESS);
    CHECK_EQ(host.commands(), 1u);
    CHECK(!host.command(0u).lent);
    CHECK_EQ(*into.at(av::clicd::ClStagingBlock::least - 2u), host.buffer_at(av::clicd::ClStagingBlock::least - 2u)); }
  { Arena            arena(host, 32u << 10);
    ClTransferChunks chunks(host, queue, arena.blocks());
    Memory           into(av::clicd::ClStagingBlock::least);
    CHECK_EQ(chunks.read(ClTransferHost::buffer_handle, 0u, av::clicd::ClStagingBlock::least, into.at(0u)),
             CL_SUCCESS);
    CHECK_EQ(host.commands(), 2u);
    CHECK(!host.command(1u).lent);
    CHECK_EQ(*into.at(100u), host.buffer_at(100u)); } }

TEST(cl_transfer_chunks, a_staged_image_read_packs_gapped_rows_into_one_command_a_slice)
{ ClTransferHost host;
  host.fill(0u);
  Arena               arena(host, 8u << 20);
  ClTransferChunks    chunks(host, queue, arena.blocks());
  constexpr std::size_t width = 4096u, rows = 5u;
  const std::size_t   pitch = width * element + 12u, slice_pitch = rows * pitch + 40u;
  const std::size_t   origin[3] = {3u, 2u, 0u}, extent[3] = {width, rows, 2u};
  const ClImageRegion region(origin, extent, pitch, slice_pitch, element);
  Memory              into(2u * slice_pitch);
  CHECK_EQ(chunks.read_image(ClTransferHost::image_handle, region, into.at(0u)), CL_SUCCESS);
  CHECK_EQ(host.commands(), 2u);
  CHECK(host.command(0u).lent);
  CHECK_EQ(host.command(0u).row_pitch, 0u);
  CHECK_EQ(host.command(0u).region[1], rows);
  CHECK_EQ(host.command(1u).origin[2], 1u);
  bool every_texel = true;
  for (std::size_t z = 0u; z < 2u; ++z)
      for (std::size_t y = 0u; y < rows; ++y)
          for (std::size_t x = 0u; x < width; ++x)
              every_texel = every_texel &&
                            into.texel(z * slice_pitch + y * pitch + x * element) == host.texel_of(3u + x, 2u + y, z);
  CHECK(every_texel);
  CHECK_EQ(*into.at(width * element), 0xEEu);
  CHECK_EQ(*into.at(pitch - 1u), 0xEEu);
  CHECK_EQ(*into.at(slice_pitch - 1u), 0xEEu);
  CHECK_EQ(arena.blocks().live(), 0u); }

TEST(cl_transfer_chunks, a_staged_image_write_packs_gapped_rows_into_one_command)
{ ClTransferHost host;
  Arena               arena(host, 8u << 20);
  ClTransferChunks    chunks(host, queue, arena.blocks());
  constexpr std::size_t width = 4096u, rows = 5u;
  const std::size_t   pitch = width * element + 12u;
  const std::size_t   origin[3] = {9u, 1u, 2u}, extent[3] = {width, rows, 1u};
  const ClImageRegion region(origin, extent, pitch, 0u, element);
  Memory              from(rows * pitch);
  for (std::size_t y = 0u; y < rows; ++y)
      for (std::size_t x = 0u; x < width; ++x) {
          const std::uint32_t value = static_cast<std::uint32_t>(y * 100000u + x);
          std::memcpy(from.at(y * pitch + x * element), &value, sizeof value);
      }
  CHECK_EQ(chunks.write_image(ClTransferHost::image_handle, region, from.at(0u)), CL_SUCCESS);
  CHECK_EQ(host.commands(), 1u);
  CHECK(host.command(0u).lent);
  CHECK_EQ(host.command(0u).row_pitch, 0u);
  bool every_texel = true;
  for (std::size_t y = 0u; y < rows; ++y)
      for (std::size_t x = 0u; x < width; ++x)
          every_texel = every_texel && host.texel(9u + x, 1u + y, 2u) == static_cast<std::uint32_t>(y * 100000u + x);
  CHECK(every_texel);
  CHECK_EQ(arena.blocks().live(), 0u); }

TEST(cl_transfer_chunks, a_staged_image_row_longer_than_its_block_goes_in_runs)
{ ClTransferHost host;
  host.fill(0u);
  Arena               arena(host, 512u << 10);
  ClTransferChunks    chunks(host, queue, arena.blocks());
  const std::size_t   width = ClTransferHost::image_width - 10u, run = (512u << 10) / element;
  const std::size_t   origin[3] = {10u, 4u, 1u}, extent[3] = {width, 2u, 1u};
  const ClImageRegion region(origin, extent, 0u, 0u, element);
  Memory              into(2u * width * element);
  CHECK_EQ(chunks.read_image(ClTransferHost::image_handle, region, into.at(0u)), CL_SUCCESS);
  CHECK_EQ(host.commands(), 4u);
  CHECK_EQ(host.command(0u).region[0], run);
  CHECK_EQ(host.command(1u).region[0], width - run);
  CHECK_EQ(host.command(1u).origin[0], 10u + run);
  CHECK_EQ(host.command(2u).origin[1], 5u);
  bool every_texel = true;
  for (std::size_t y = 0u; y < 2u; ++y)
      for (std::size_t x = 0u; x < width; ++x)
          every_texel = every_texel && into.texel((y * width + x) * element) == host.texel_of(10u + x, 4u + y, 1u);
  CHECK(every_texel); }

TEST(cl_transfer_chunks, a_staged_rectangle_is_one_packed_command_each_way)
{ constexpr std::size_t width = 3000u, rows = 30u, pitch = 4096u, host_pitch = 3100u;
  const std::size_t corner[3] = {100u, 2u, 0u}, host_corner[3] = {5u, 1u, 0u}, extent[3] = {width, rows, 1u};
  const ClBufferRect rect(corner, host_corner, extent, pitch, 0u, host_pitch, 0u);
  CHECK_EQ(rect.check(), CL_SUCCESS);
  const std::size_t at_corner = host_pitch + 5u;
  { ClTransferHost host;
    host.fill(5u);
    Arena            arena(host, 8u << 20);
    ClTransferChunks chunks(host, queue, arena.blocks());
    Memory           into((rows + 1u) * host_pitch);
    CHECK_EQ(chunks.read_buffer_rect(ClTransferHost::buffer_handle, rect, into.at(0u)), CL_SUCCESS);
    CHECK_EQ(host.commands(), 1u);
    CHECK(host.command(0u).lent);
    CHECK_EQ(host.command(0u).row_pitch, 0u);
    CHECK_EQ(host.command(0u).buffer_pitch[0], pitch);
    bool every_byte = true;
    for (std::size_t y = 0u; y < rows; ++y)
        for (std::size_t x = 0u; x < width; ++x)
            every_byte = every_byte &&
                         *into.at(at_corner + y * host_pitch + x) == host.buffer_at((2u + y) * pitch + 100u + x);
    CHECK(every_byte);
    CHECK_EQ(*into.at(at_corner - 1u), 0xEEu);
    CHECK_EQ(*into.at(at_corner + width), 0xEEu);
    CHECK_EQ(arena.blocks().live(), 0u); }
  { ClTransferHost host;
    Arena            arena(host, 8u << 20);
    ClTransferChunks chunks(host, queue, arena.blocks());
    Memory           from((rows + 1u) * host_pitch);
    for (std::size_t index = 0u; index < (rows + 1u) * host_pitch; ++index)
        *from.at(index) = static_cast<std::uint8_t>(index * 31u);
    CHECK_EQ(chunks.write_buffer_rect(ClTransferHost::buffer_handle, rect, from.at(0u)), CL_SUCCESS);
    CHECK_EQ(host.commands(), 1u);
    CHECK(host.command(0u).lent);
    bool every_byte = true;
    for (std::size_t y = 0u; y < rows; ++y)
        for (std::size_t x = 0u; x < width; ++x)
            every_byte = every_byte &&
                         host.buffer_at((2u + y) * pitch + 100u + x) == *from.at(at_corner + y * host_pitch + x);
    CHECK(every_byte);
    CHECK_EQ(host.buffer_at(2u * pitch + 99u), 0u);
    CHECK_EQ(arena.blocks().live(), 0u); } }

TEST(cl_transfer_chunks, a_refused_staged_region_ends_the_transfer)
{ constexpr std::size_t width = 4096u, rows = 5u;
  const std::size_t   origin[3] = {0u, 0u, 0u}, extent[3] = {width, rows, 1u};
  const ClImageRegion region(origin, extent, 0u, 0u, element);
  const std::size_t   corner[3] = {0u, 0u, 0u}, rect_extent[3] = {width * element, rows, 1u};
  const ClBufferRect  rect(corner, corner, rect_extent, width * element, 0u, 0u, 0u);
  Memory              memory(rows * width * element);
  for (std::uint32_t which = 0u; which < 4u; ++which) {
      ClTransferHost host;
      host.refuse(0u, CL_INVALID_OPERATION);
      Arena            arena(host, 8u << 20);
      ClTransferChunks chunks(host, queue, arena.blocks());
      const cl_int     answered =
          which == 0u ? chunks.read_image(ClTransferHost::image_handle, region, memory.at(0u))
          : which == 1u ? chunks.write_image(ClTransferHost::image_handle, region, memory.at(0u))
          : which == 2u ? chunks.read_buffer_rect(ClTransferHost::buffer_handle, rect, memory.at(0u))
                        : chunks.write_buffer_rect(ClTransferHost::buffer_handle, rect, memory.at(0u));
      CHECK_EQ(answered, CL_INVALID_OPERATION);
      CHECK_EQ(host.commands(), 1u);
      CHECK(host.command(0u).lent);
      CHECK_EQ(arena.blocks().live(), 0u);
  } }

TEST(cl_transfer_chunks, a_small_read_with_gapped_rows_is_one_staged_command)
{ constexpr std::size_t width = 64u, rows = 16u, pitch = 4096u, host_pitch = 100u;
  const std::size_t corner[3] = {32u, 1u, 0u}, host_corner[3] = {0u, 0u, 0u}, extent[3] = {width, rows, 1u};
  { ClTransferHost host;
    host.fill(3u);
    Arena              arena(host, 8u << 20);
    ClTransferChunks   chunks(host, queue, arena.blocks());
    const ClBufferRect gapped(corner, host_corner, extent, pitch, 0u, host_pitch, 0u);
    CHECK_EQ(gapped.check(), CL_SUCCESS);
    Memory into(rows * host_pitch);
    CHECK_EQ(chunks.read_buffer_rect(ClTransferHost::buffer_handle, gapped, into.at(0u)), CL_SUCCESS);
    CHECK_EQ(host.commands(), 1u);
    CHECK(host.command(0u).lent);
    CHECK_EQ(host.command(0u).region[1], rows);
    bool every_byte = true;
    for (std::size_t y = 0u; y < rows; ++y)
        for (std::size_t x = 0u; x < width; ++x)
            every_byte = every_byte && *into.at(y * host_pitch + x) == host.buffer_at((1u + y) * pitch + 32u + x);
    CHECK(every_byte);
    CHECK_EQ(*into.at(width), 0xEEu);
    CHECK_EQ(*into.at(host_pitch - 1u), 0xEEu);
    CHECK_EQ(arena.blocks().live(), 0u); }
  { ClTransferHost host;
    host.fill(3u);
    Arena              arena(host, 8u << 20);
    ClTransferChunks   chunks(host, queue, arena.blocks());
    const ClBufferRect packed(corner, host_corner, extent, pitch, 0u, 0u, 0u);
    Memory             into(rows * width);
    CHECK_EQ(chunks.read_buffer_rect(ClTransferHost::buffer_handle, packed, into.at(0u)), CL_SUCCESS);
    CHECK_EQ(host.commands(), 1u);
    CHECK(!host.command(0u).lent);
    CHECK_EQ(*into.at(width), host.buffer_at(2u * pitch + 32u)); }
  { ClTransferHost host;
    host.fill(0u);
    Arena               arena(host, 8u << 20);
    ClTransferChunks    chunks(host, queue, arena.blocks());
    const std::size_t   origin[3] = {5u, 2u, 0u}, image_extent[3] = {8u, 5u, 1u};
    const std::size_t   gap_pitch = 8u * element + 12u;
    const ClImageRegion region(origin, image_extent, gap_pitch, 0u, element);
    Memory              into(5u * gap_pitch);
    CHECK_EQ(chunks.read_image(ClTransferHost::image_handle, region, into.at(0u)), CL_SUCCESS);
    CHECK_EQ(host.commands(), 1u);
    CHECK(host.command(0u).lent);
    for (std::size_t y = 0u; y < 5u; ++y) {
        CHECK_EQ(into.texel(y * gap_pitch), host.texel_of(5u, 2u + y, 0u));
        CHECK_EQ(into.texel(y * gap_pitch + 7u * element), host.texel_of(12u, 2u + y, 0u));
        CHECK_EQ(*into.at(y * gap_pitch + 8u * element), 0xEEu);
    } } }
