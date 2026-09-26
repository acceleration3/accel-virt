/* A transfer of any size, cut into commands a guest's staging room and reply
 * slot carry. ClTransferHost is a host's buffer and image that applies what
 * arrives, so each test states what memory holds afterwards - every byte, in
 * its place - and then how many commands it took and what each named. */
#include "harness/framework/test_registry.hpp"
#include "harness/icd/cl_transfer_host.hpp"
#include "icd/opencl/cl_image_region.hpp"
#include "icd/opencl/cl_transfer_chunks.hpp"
#include "proto/generated/opencl/cl_protocol_ids.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

using av::ClTransferHost;
using av::clicd::ClImageRegion;
using av::clicd::ClTransferChunks;

namespace {

constexpr std::uint64_t queue = 0x6500u;
constexpr std::size_t   element = ClTransferHost::element_bytes;

/* Memory of the application's, filled with a marker a transfer must not leave
 * behind where it wrote and must leave alone where it did not. */
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

}  // namespace

TEST(cl_transfer_chunks, a_read_that_fits_one_reply_is_one_blocking_command)
{ ClTransferHost host;
  host.fill(3u);
  ClTransferChunks chunks(host, queue);
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
  ClTransferChunks  chunks(host, queue);
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
  ClTransferChunks  chunks(host, queue);
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
  /* Blocking, every piece: the bytes are in the device's decode arena, which
   * the next command reuses. */
  for (std::uint32_t index = 0u; index < host.commands(); ++index) CHECK(host.command(index).blocking); }

TEST(cl_transfer_chunks, a_refused_piece_ends_the_transfer_with_the_hosts_error)
{ ClTransferHost host;
  host.refuse(1u, CL_INVALID_VALUE);
  ClTransferChunks  chunks(host, queue);
  const std::size_t bytes = 3u * ClTransferChunks::read_room;
  Memory            into(bytes);
  CHECK_EQ(chunks.read(ClTransferHost::buffer_handle, 0u, bytes, into.at(0u)), CL_INVALID_VALUE);
  CHECK_EQ(host.commands(), 2u); }

/* A write's pieces go without waiting for an answer each: the host makes every
 * write blocking and in queue order, and the caller's drain after the last is
 * the answer for all of them. So a piece the host refuses stops nothing here,
 * and only its own bytes are missing. */
TEST(cl_transfer_chunks, a_write_sends_every_piece_whatever_the_host_answers_one)
{ ClTransferHost refusing;
  refusing.refuse(0u, CL_MEM_OBJECT_ALLOCATION_FAILURE);
  ClTransferChunks  writes(refusing, queue);
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
  ClTransferChunks chunks(host, queue);
  std::uint8_t     byte = 0u;
  /* The host's buffer ends here, so this host cannot read the command. */
  CHECK_EQ(chunks.read(ClTransferHost::buffer_handle, ClTransferHost::buffer_bytes, 1u, &byte), CL_OUT_OF_RESOURCES);
  CHECK_EQ(chunks.write(ClTransferHost::buffer_handle, ClTransferHost::buffer_bytes, 1u, &byte),
           CL_OUT_OF_RESOURCES); }

TEST(cl_transfer_chunks, a_transfer_of_nothing_sends_nothing)
{ ClTransferHost   host;
  ClTransferChunks chunks(host, queue);
  std::uint8_t     byte = 0u;
  CHECK_EQ(chunks.read(ClTransferHost::buffer_handle, 0u, 0u, &byte), CL_SUCCESS);
  CHECK_EQ(chunks.write(ClTransferHost::buffer_handle, 0u, 0u, &byte), CL_SUCCESS);
  CHECK_EQ(host.commands(), 0u); }

TEST(cl_transfer_chunks, packed_image_rows_come_back_as_runs_that_fit_a_reply)
{ ClTransferHost host;
  host.fill(0u);
  ClTransferChunks  chunks(host, queue);
  /* 60000 texels a row is 240000 bytes: four rows fit a reply, five do not. */
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
  ClTransferChunks    chunks(host, queue);
  const std::size_t   origin[3] = {0u, 2u, 0u}, extent[3] = {8u, 5u, 1u};
  const std::size_t   pitch = 8u * element + 12u;
  const ClImageRegion region(origin, extent, pitch, 0u, element);
  Memory              into(5u * pitch);
  CHECK_EQ(chunks.read_image(ClTransferHost::image_handle, region, into.at(0u)), CL_SUCCESS);
  CHECK_EQ(host.commands(), 5u);
  for (std::size_t y = 0u; y < 5u; ++y) {
      CHECK_EQ(into.texel(y * pitch), host.texel_of(0u, 2u + y, 0u));
      CHECK_EQ(into.texel(y * pitch + 7u * element), host.texel_of(7u, 2u + y, 0u));
      /* The twelve bytes after each row are the application's own. */
      CHECK_EQ(*into.at(y * pitch + 8u * element), 0xEEu);
      CHECK_EQ(*into.at(y * pitch + pitch - 1u), 0xEEu);
  } }

TEST(cl_transfer_chunks, an_image_row_longer_than_a_reply_comes_back_in_pieces_of_whole_texels)
{ ClTransferHost host;
  host.fill(0u);
  ClTransferChunks    chunks(host, queue);
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

TEST(cl_transfer_chunks, image_rows_go_to_the_host_in_runs_at_the_applications_own_pitch)
{ ClTransferHost host;
  ClTransferChunks    chunks(host, queue);
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
  /* Three rows fit the staging room at this pitch, so each slice of ten is
   * four commands, and each names the pitch it was sent at. */
  const std::size_t per_command = 1u + (ClTransferChunks::write_room - width * element) / pitch;
  CHECK_EQ(per_command, 3u);
  CHECK_EQ(host.commands(), 8u);
  CHECK_EQ(host.command(0u).row_pitch, pitch);
  CHECK_EQ(host.command(0u).bytes, 2u * pitch + width * element);
  CHECK_EQ(host.command(3u).region[1], 1u);
  CHECK_EQ(host.command(4u).origin[2], 1u); }

TEST(cl_transfer_chunks, an_image_row_longer_than_the_staging_room_goes_in_pieces_of_whole_texels)
{ ClTransferHost host;
  ClTransferChunks    chunks(host, queue);
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
    ClTransferChunks chunks(host, queue);
    CHECK_EQ(chunks.read_image(ClTransferHost::image_handle, wide, memory.at(0u)), CL_INVALID_OPERATION);
    CHECK_EQ(host.commands(), 2u); }
  { ClTransferHost host;
    host.refuse(0u, CL_INVALID_OPERATION);
    ClTransferChunks chunks(host, queue);
    CHECK_EQ(chunks.read_image(ClTransferHost::image_handle, narrow, memory.at(0u)), CL_INVALID_OPERATION);
    CHECK_EQ(host.commands(), 1u); }
  { ClTransferHost host;
    host.refuse(0u, CL_INVALID_OPERATION);
    ClTransferChunks chunks(host, queue);
    CHECK_EQ(chunks.write_image(ClTransferHost::image_handle, wide, memory.at(0u)), CL_INVALID_OPERATION);
    CHECK_EQ(host.commands(), 1u); }
  { ClTransferHost host;
    host.refuse(0u, CL_INVALID_OPERATION);
    ClTransferChunks chunks(host, queue);
    CHECK_EQ(chunks.write_image(ClTransferHost::image_handle, narrow, memory.at(0u)), CL_INVALID_OPERATION);
    CHECK_EQ(host.commands(), 1u); } }
