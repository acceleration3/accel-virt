/* A pixel transfer too large for its command, staged in a host pixel buffer
 * (PixelStagingBuffer), against a host that decodes it with the generated
 * server: what the application's memory, or the texture, holds after - with
 * the pixel store applied as the host applies it - and the binding left as the
 * application had it. */
#include "harness/framework/test_registry.hpp"
#include "harness/icd/gl_transfer_host.hpp"
#include "icd/opengl/buffer_pieces.hpp"
#include "icd/opengl/pixel_staging_buffer.hpp"

#include "gl_dispatch_client.h"
#include "gl_protocol_ids.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>

using av::GlTransferHost;
using av::glicd::BufferPieces;
using av::glicd::PixelStagingBuffer;
namespace ids = av::glproto;

namespace av::glicd {

/* The state the invariant guards, which correct code never reaches. */
struct PixelStagingBufferProbe {
    static void stage_with_no_name(PixelStagingBuffer &staging) { staging.staged_ = PixelStagingBuffer::Staged::download; }
};

}  // namespace av::glicd

namespace {

constexpr GLenum pack_buffer = 0x88EBu, unpack_buffer = 0x88ECu, gl_rgba = 0x1908u, gl_ubyte = 0x1401u;
constexpr GLenum pack_row_length = 0x0D02u, pack_skip_rows = 0x0D03u, pack_skip_pixels = 0x0D04u;
constexpr GLenum unpack_row_length = 0x0CF2u, unpack_skip_rows = 0x0CF3u;
constexpr GLenum texture_2d = 0x0DE1u;

/* Client memory a test owns, every byte `fill` until written. */
class Memory {
public:
    Memory(std::size_t bytes, std::uint8_t fill) : bytes_(static_cast<std::uint8_t *>(std::malloc(bytes)))
    {
        std::memset(bytes_, fill, bytes);
    }
    ~Memory() { std::free(bytes_); }
    Memory(const Memory &) = delete;
    Memory &operator=(const Memory &) = delete;
    std::uint8_t *bytes() { return bytes_; }
    std::uint32_t word(std::size_t offset) const
    {
        std::uint32_t value = 0u;
        std::memcpy(&value, bytes_ + offset, sizeof value);
        return value;
    }

private:
    std::uint8_t *bytes_;
};

/* How far a transfer of `rows` rows of `columns` RGBA8 texels reaches with
 * the given row length and skips, at an alignment of four: what the ICD's
 * pixel store answers for it. */
std::size_t reach(std::size_t columns, std::size_t rows, std::size_t length, std::size_t skip_rows,
                  std::size_t skip_pixels)
{
    return ((skip_rows + rows - 1u) * length + skip_pixels + columns) * 4u;
}

/* A read staged, sent as the ICD sends it, and finished. */
bool staged_read(GlTransferHost &host, PixelStagingBuffer &staging, GLint x, GLint y, GLsizei columns,
                 GLsizei rows, void *pixels, std::size_t bytes)
{
    if (!staging.stage_download(host, pixels, bytes)) return false;
    ids::av_glReadPixels(host, x, y, columns, rows, gl_rgba, gl_ubyte, nullptr, 0u, true);
    staging.finish_download(host, pixels, bytes);
    return true;
}

}  // namespace

/* A read larger than a reply, with no arena, comes back through the buffer:
 * every pixel where the pack store puts it, from where the host read it, and
 * the pack binding none again. What lies between rows is what the buffer's
 * fresh storage held. */
TEST(pixel_staging_buffer, a_read_past_the_reply_comes_back_where_the_pack_store_puts_it)
{ GlTransferHost     host;
  PixelStagingBuffer staging;
  constexpr GLsizei  columns = 300, rows = 200, length = 320, skip_rows = 2, skip_pixels = 3;
  ids::av_glPixelStorei(host, pack_row_length, length);
  ids::av_glPixelStorei(host, pack_skip_rows, skip_rows);
  ids::av_glPixelStorei(host, pack_skip_pixels, skip_pixels);
  const std::size_t bytes = reach(columns, rows, length, skip_rows, skip_pixels);
  CHECK(bytes > BufferPieces::read_room);
  Memory memory(bytes + 4u, 0xEEu);
  CHECK(staged_read(host, staging, 10, 20, columns, rows, memory.bytes(), bytes));
  std::uint32_t wrong = 0u;
  for (GLsizei row = 0; row < rows; row++)
      for (GLsizei column = 0; column < columns; column++) {
          const std::size_t at = (std::size_t(skip_rows + row) * length + std::size_t(skip_pixels + column)) * 4u;
          if (memory.word(at) != host.framebuffer_pixel(10 + column, 20 + row)) wrong++;
      }
  CHECK_EQ(wrong, 0u);
  CHECK_EQ(memory.word(0u), 0u);
  CHECK_EQ(memory.word(bytes), 0xEEEEEEEEu);
  CHECK_EQ(host.bound(pack_buffer), 0u);
  CHECK_EQ(host.refusals(), 0u);
  CHECK_EQ(host.count_of(ids::kCmd_glGetBufferSubData),
           static_cast<std::uint32_t>((bytes + BufferPieces::read_room - 1u) / BufferPieces::read_room));
  CHECK(staging.invariants_hold()); }

/* The buffer is named once and given each transfer's size. */
TEST(pixel_staging_buffer, one_buffer_serves_every_transfer_at_its_size)
{ GlTransferHost     host;
  PixelStagingBuffer staging;
  Memory             memory(GlTransferHost::image_bytes, 0u);
  CHECK(staged_read(host, staging, 0, 0, 1024, 32, memory.bytes(), 1024u * 32u * 4u));
  const GLuint name = staging.name();
  CHECK(name != 0u);
  CHECK_EQ(host.buffer_size(name), 1024u * 32u * 4u);
  CHECK(staged_read(host, staging, 0, 0, 1024, 512, memory.bytes(), GlTransferHost::image_bytes));
  CHECK_EQ(staging.name(), name);
  CHECK_EQ(host.generated(), 1u);
  CHECK_EQ(host.buffer_size(name), GlTransferHost::image_bytes);
  CHECK_EQ(memory.word(GlTransferHost::image_bytes - 4u), host.framebuffer_pixel(1023, 511));
  CHECK_EQ(host.refusals(), 0u); }

/* An upload larger than a request, with no arena, goes into the buffer in
 * pieces and the command reads it from there with the unpack store applied;
 * the unpack binding is none again. */
TEST(pixel_staging_buffer, an_upload_past_the_request_reaches_the_texture_through_the_buffer)
{ GlTransferHost     host;
  PixelStagingBuffer staging;
  constexpr GLsizei  columns = 512, rows = 256, length = 600, skip_rows = 1;
  ids::av_glPixelStorei(host, unpack_row_length, length);
  ids::av_glPixelStorei(host, unpack_skip_rows, skip_rows);
  const std::size_t bytes = reach(columns, rows, length, skip_rows, 0u);
  Memory            memory(bytes, 0u);
  for (std::size_t at = 0u; at + 4u <= bytes; at += 4u) {
      const auto value = static_cast<std::uint32_t>(at * 2654435761u);
      std::memcpy(memory.bytes() + at, &value, 4u);
  }
  CHECK(staging.stage_upload(host, memory.bytes(), bytes));
  CHECK_EQ(host.bound(unpack_buffer), staging.name());
  ids::av_glTexSubImage2D(host, texture_2d, 0, 7, 9, columns, rows, gl_rgba, gl_ubyte, nullptr, 0u, true);
  staging.finish_upload(host);
  CHECK_EQ(host.bound(unpack_buffer), 0u);
  std::uint32_t wrong = 0u;
  for (GLsizei row = 0; row < rows; row++)
      for (GLsizei column = 0; column < columns; column++)
          if (host.texture_pixel(7 + column, 9 + row) !=
              memory.word((std::size_t(skip_rows + row) * length + std::size_t(column)) * 4u))
              wrong++;
  CHECK_EQ(wrong, 0u);
  CHECK_EQ(host.count_of(ids::kCmd_glBufferSubData),
           static_cast<std::uint32_t>((bytes + BufferPieces::write_room - 1u) / BufferPieces::write_room));
  CHECK(host.largest_request() <= 256u * 1024u);
  CHECK_EQ(host.refusals(), 0u); }

/* What crosses with its command, or in a lease the arena has room for, is
 * not staged and sends nothing; nor are no pixels, or a size that is a
 * negative one cast. */
TEST(pixel_staging_buffer, what_crosses_otherwise_is_not_staged)
{ GlTransferHost     host;
  PixelStagingBuffer staging;
  Memory             memory(4u << 20u, 0u);
  CHECK(!staging.stage_download(host, memory.bytes(), BufferPieces::read_room));
  CHECK(!staging.stage_upload(host, memory.bytes(), BufferPieces::write_room));
  CHECK(!staging.stage_download(host, nullptr, 1u << 20u));
  CHECK(!staging.stage_upload(host, nullptr, 1u << 20u));
  CHECK(!staging.stage_download(host, memory.bytes(), ~std::size_t{0}));
  CHECK(!staging.stage_upload(host, memory.bytes(), ~std::size_t{0}));
  static std::uint8_t arena[2u << 20];
  host.lend(arena, sizeof arena);
  CHECK(!staging.stage_download(host, memory.bytes(), 1u << 20u));
  CHECK(!staging.stage_upload(host, memory.bytes(), 1u << 20u));
  CHECK(staging.stage_download(host, memory.bytes(), 3u << 20u));
  CHECK_EQ(host.generated(), 1u);
  staging.finish_download(host, memory.bytes(), 3u << 20u);
  CHECK_EQ(staging.name(), 1u); }

/* A host that names no buffer leaves the transfer to cross as it would have,
 * with nothing bound. */
TEST(pixel_staging_buffer, a_host_that_names_no_buffer_stages_nothing)
{ GlTransferHost     host;
  PixelStagingBuffer staging;
  host.name_nothing();
  Memory memory(1u << 20u, 0u);
  CHECK(!staging.stage_download(host, memory.bytes(), 1u << 20u));
  CHECK(!staging.stage_upload(host, memory.bytes(), 1u << 20u));
  CHECK_EQ(staging.name(), 0u);
  CHECK_EQ(host.bound(pack_buffer), 0u);
  CHECK_EQ(host.bound(unpack_buffer), 0u);
  CHECK_EQ(host.count_of(ids::kCmd_glBindBuffer), 0u); }

/* A finish with nothing staged sends nothing and writes nothing. */
TEST(pixel_staging_buffer, a_finish_with_nothing_staged_does_nothing)
{ GlTransferHost     host;
  PixelStagingBuffer staging;
  Memory             memory(16u, 0x77u);
  staging.finish_download(host, memory.bytes(), 16u);
  staging.finish_upload(host);
  CHECK_EQ(host.commands(), 0u);
  CHECK_EQ(memory.word(0u), 0x77777777u); }

/* A context that goes deletes the buffer; a lost one only forgets it. */
TEST(pixel_staging_buffer, release_deletes_the_buffer_and_forget_asks_nothing)
{ GlTransferHost     host;
  PixelStagingBuffer staging;
  Memory             memory(1u << 20u, 0u);
  CHECK(staged_read(host, staging, 0, 0, 512, 128, memory.bytes(), 1u << 18u));
  CHECK_EQ(host.live_buffers(), 1u);
  staging.release(host);
  CHECK_EQ(host.live_buffers(), 0u);
  CHECK_EQ(staging.name(), 0u);
  staging.release(host);
  CHECK_EQ(host.count_of(ids::kCmd_glDeleteBuffers), 1u);
  CHECK(staged_read(host, staging, 0, 0, 512, 128, memory.bytes(), 1u << 18u));
  const std::uint32_t asked = host.commands();
  staging.forget();
  CHECK_EQ(staging.name(), 0u);
  CHECK_EQ(host.commands(), asked);
  CHECK(staging.invariants_hold()); }

TEST(pixel_staging_buffer_invariants, a_transfer_staged_in_no_buffer_is_caught)
{ PixelStagingBuffer staging;
  CHECK(staging.invariants_hold());
  av::glicd::PixelStagingBufferProbe::stage_with_no_name(staging);
  CHECK(!staging.invariants_hold()); }
