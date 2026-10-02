/* The queued present's fixed part, byte by byte: the driver's own type, the
 * ticket and the source, then the present copy's fields in the copy's order,
 * and every rule the device holds a request to - each broken alone - refused
 * as malformed, leaving what was decoded before as it was. */
#include "harness/framework/test_registry.hpp"
#include "proto/dma/present_operation.hpp"
#include "proto/dma/present_region.hpp"
#include "proto/resources/surface_point.hpp"
#include "proto/virtio_gpu/present_copy_request.hpp"
#include "proto/virtio_gpu/queued_present_request.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>

using av::PresentCopyRequest;
using av::PresentOperation;
using av::PresentRegion;
using av::QueuedPresentRequest;
using av::Status;
using av::SurfacePoint;
using av::WireReader;
using av::WireWriter;
using Source = av::QueuedPresentRequest::Source;

namespace {

constexpr std::uint32_t swap = PresentCopyRequest::flag_swap_red_blue;
constexpr std::uint32_t max_entries = QueuedPresentRequest::max_entries;
constexpr std::uint32_t max_regions = QueuedPresentRequest::max_regions;
constexpr std::uint64_t surface = 0x1122334455667788u;
constexpr std::uint32_t point = 0x99AABBCCu;

struct Encoded {
    std::uint8_t bytes[QueuedPresentRequest::encoded_size] = {};
};

Encoded encode(const QueuedPresentRequest &request)
{
    Encoded    encoded;
    WireWriter writer(encoded.bytes, sizeof encoded.bytes);
    request.write(writer);
    CHECK_EQ(writer.offset(), QueuedPresentRequest::encoded_size);
    return encoded;
}

Status decode_bytes(const Encoded &encoded, QueuedPresentRequest &into)
{
    WireReader reader(encoded.bytes, sizeof encoded.bytes);
    return into.decode(reader);
}

Status decode(const QueuedPresentRequest &request, QueuedPresentRequest &into)
{
    return decode_bytes(encode(request), into);
}

PresentCopyRequest blt(std::uint32_t flags, std::uint32_t colour, std::uint32_t regions, std::uint32_t source_entries,
                       std::uint32_t source_pitch, std::uint32_t destination_entries, std::uint32_t destination_pitch)
{
    return PresentCopyRequest(PresentOperation::blt, flags, colour, regions, source_entries, source_pitch,
                              destination_entries, destination_pitch);
}

PresentCopyRequest fill(std::uint32_t flags, std::uint32_t source_entries, std::uint32_t source_pitch)
{
    return PresentCopyRequest(PresentOperation::color_fill, flags, 0xFF00FF00u, 1u, source_entries, source_pitch, 2u,
                              64u);
}

template <typename T>
T field_at(const std::uint8_t *bytes, std::size_t offset)
{
    T value = 0;
    std::memcpy(&value, bytes + offset, sizeof value);
    return value;
}

}  // namespace

TEST(queued_present_request, writes_the_ticket_the_source_then_the_copy_s_fields_in_order)
{ const Encoded encoded = encode(QueuedPresentRequest(9u, blt(swap, 0u, 3u, 5u, 5120u, 7u, 8192u)));
  CHECK_EQ(field_at<std::uint32_t>(encoded.bytes, 0u), 0x02F7u);
  for (std::uint32_t at = 4u; at < 24u; at++) CHECK_EQ(encoded.bytes[at], 0u);
  CHECK_EQ(field_at<std::uint64_t>(encoded.bytes, 24u), 9u);
  CHECK_EQ(field_at<std::uint32_t>(encoded.bytes, 32u), 0u);
  CHECK_EQ(field_at<std::uint32_t>(encoded.bytes, 36u), 0u);
  CHECK_EQ(field_at<std::uint64_t>(encoded.bytes, 40u), 0u);
  const std::uint32_t expected[8] = {1u, swap, 0u, 3u, 5u, 7u, 5120u, 8192u};
  for (std::uint32_t index = 0u; index < 8u; index++)
      CHECK_EQ(field_at<std::uint32_t>(encoded.bytes, 48u + index * 4u), expected[index]);
  const Encoded image = encode(QueuedPresentRequest(10u, SurfacePoint(surface, point), blt(0u, 0u, 1u, 0u, 0u, 2u, 256u)));
  CHECK_EQ(field_at<std::uint32_t>(image.bytes, 32u), 1u);
  CHECK_EQ(field_at<std::uint32_t>(image.bytes, 36u), point);
  CHECK_EQ(field_at<std::uint64_t>(image.bytes, 40u), surface);
  CHECK_EQ(field_at<std::uint32_t>(image.bytes, 52u), 0u); }

TEST(queued_present_request, a_page_blt_reads_back_as_written_and_places_its_parts_after_its_own_fixed_part)
{ QueuedPresentRequest read;
  CHECK_STATUS(decode(QueuedPresentRequest(9u, blt(swap, 0u, 3u, 5u, 5120u, 7u, 8192u)), read), Status::ok);
  CHECK_EQ(read.ticket(), 9u);
  CHECK(read.source() == Source::pages);
  CHECK(!read.image().names());
  CHECK(read.copy().operation() == PresentOperation::blt);
  CHECK(read.copy().swaps_red_and_blue());
  CHECK_EQ(read.copy().regions(), 3u);
  CHECK_EQ(read.copy().source_entries(), 5u);
  CHECK_EQ(read.copy().source_pitch(), 5120u);
  CHECK_EQ(read.copy().destination_entries(), 7u);
  CHECK_EQ(read.copy().destination_pitch(), 8192u);
  CHECK_EQ(read.destination_entries_offset(), 80u + 5u * 16u);
  CHECK_EQ(read.regions_offset(), 80u + 12u * 16u);
  CHECK_EQ(read.size(), 80u + 12u * 16u + 3u * PresentRegion::encoded_size);
  CHECK_STATUS(decode(QueuedPresentRequest(1u, blt(0u, 0u, max_regions, max_entries, 4u, max_entries, 4u)), read),
               Status::ok);
  CHECK(!read.copy().swaps_red_and_blue()); }

TEST(queued_present_request, an_image_blt_names_its_surface_and_point_and_no_source_pages)
{ QueuedPresentRequest read;
  CHECK_STATUS(decode(QueuedPresentRequest(4u, SurfacePoint(surface, point), blt(swap, 0u, 2u, 0u, 0u, 3u, 256u)), read),
               Status::ok);
  CHECK(read.source() == Source::image);
  CHECK(read.image() == SurfacePoint(surface, point));
  CHECK(read.copy().swaps_red_and_blue());
  CHECK_EQ(read.destination_entries_offset(), 80u);
  CHECK_EQ(read.regions_offset(), 80u + 3u * 16u);
  CHECK_EQ(read.size(), 80u + 3u * 16u + 2u * PresentRegion::encoded_size); }

TEST(queued_present_request, a_page_fill_has_a_colour_and_no_source)
{ QueuedPresentRequest read;
  CHECK_STATUS(decode(QueuedPresentRequest(2u, fill(0u, 0u, 0u)), read), Status::ok);
  CHECK(read.copy().operation() == PresentOperation::color_fill);
  CHECK_EQ(read.copy().colour(), 0xFF00FF00u);
  CHECK_EQ(read.destination_entries_offset(), 80u); }

/* Each rule broken alone, after a request that held: the refusal leaves it. */
TEST(queued_present_request, every_rule_broken_is_malformed_and_changes_nothing)
{ QueuedPresentRequest read;
  CHECK_STATUS(decode(QueuedPresentRequest(7u, blt(0u, 0u, 1u, 1u, 64u, 1u, 64u)), read), Status::ok);
  const QueuedPresentRequest broken[] = {
      QueuedPresentRequest(0u, blt(0u, 0u, 1u, 1u, 64u, 1u, 64u)),                /* no ticket */
      QueuedPresentRequest(8u, PresentCopyRequest(PresentOperation::flip, 0u, 0u, 1u, 1u, 64u, 1u, 64u)),
      QueuedPresentRequest(8u, PresentCopyRequest(static_cast<PresentOperation>(0u), 0u, 0u, 1u, 1u, 64u, 1u, 64u)),
      QueuedPresentRequest(8u, blt(0u, 5u, 1u, 1u, 64u, 1u, 64u)),                /* a colour on a blt */
      QueuedPresentRequest(8u, blt(0u, 0u, 0u, 1u, 64u, 1u, 64u)),                /* no regions */
      QueuedPresentRequest(8u, blt(0u, 0u, max_regions + 1u, 1u, 64u, 1u, 64u)),  /* too many */
      QueuedPresentRequest(8u, blt(0u, 0u, 1u, 0u, 64u, 1u, 64u)),                /* a page blt with no pages */
      QueuedPresentRequest(8u, blt(0u, 0u, 1u, 0u, 0u, 1u, 64u)),                 /* ... nor pitch */
      QueuedPresentRequest(8u, blt(0u, 0u, 1u, max_entries + 1u, 64u, 1u, 64u)),
      QueuedPresentRequest(8u, blt(0u, 0u, 1u, 1u, 0u, 1u, 64u)),                 /* no source pitch */
      QueuedPresentRequest(8u, blt(0u, 0u, 1u, 1u, 66u, 1u, 64u)),                /* part of a pixel */
      QueuedPresentRequest(8u, blt(0u, 0u, 1u, 1u, 64u, 0u, 64u)),                /* no destination */
      QueuedPresentRequest(8u, blt(0u, 0u, 1u, 1u, 64u, max_entries + 1u, 64u)),
      QueuedPresentRequest(8u, blt(0u, 0u, 1u, 1u, 64u, 1u, 0u)),                 /* no destination pitch */
      QueuedPresentRequest(8u, blt(0u, 0u, 1u, 1u, 64u, 1u, 62u)),
      QueuedPresentRequest(8u, fill(swap, 0u, 0u)),                               /* nothing to swap in a fill */
      QueuedPresentRequest(8u, fill(0u, 1u, 0u)),                                 /* a fill with a source */
      QueuedPresentRequest(8u, fill(0u, 0u, 64u)),
      QueuedPresentRequest(8u, SurfacePoint(0u, point), blt(0u, 0u, 1u, 0u, 0u, 1u, 64u)),      /* an image of no surface */
      QueuedPresentRequest(8u, SurfacePoint(surface, 0u), blt(0u, 0u, 1u, 0u, 0u, 1u, 64u)),    /* ... at no point */
      QueuedPresentRequest(8u, SurfacePoint(surface, point), blt(0u, 0u, 1u, 1u, 0u, 1u, 64u)), /* an image with pages */
      QueuedPresentRequest(8u, SurfacePoint(surface, point), blt(0u, 0u, 1u, 0u, 64u, 1u, 64u)),/* ... or a pitch */
      QueuedPresentRequest(8u, SurfacePoint(surface, point), fill(0u, 0u, 0u)),                 /* an image fills nothing */
  };
  for (const QueuedPresentRequest &request : broken) {
      CHECK_STATUS(decode(request, read), Status::malformed);
      CHECK_EQ(read.ticket(), 7u);
      CHECK(read.source() == Source::pages);
      CHECK_EQ(read.copy().source_pitch(), 64u);
  }
  const Encoded good = encode(QueuedPresentRequest(8u, blt(0u, 0u, 1u, 1u, 64u, 1u, 64u)));
  struct Damage {
      std::size_t  offset;
      std::uint8_t value;
  };
  const Damage damages[] = {{32u, 2u},    /* a source kind nothing names */
                            {36u, 1u},    /* a point on a page source */
                            {39u, 1u},    /* ... in its last byte */
                            {40u, 1u},    /* a surface on a page source */
                            {52u, 2u},    /* a flag nothing names */
                            {55u, 0x80u}};
  for (const Damage &damage : damages) {
      Encoded bytes = good;
      bytes.bytes[damage.offset] = damage.value;
      CHECK_STATUS(decode_bytes(bytes, read), Status::malformed);
      CHECK_EQ(read.ticket(), 7u);
  }
  WireReader short_reader(good.bytes, sizeof good.bytes - 1u);
  CHECK_STATUS(read.decode(short_reader), Status::malformed);
  CHECK_EQ(read.ticket(), 7u);
  CHECK_STATUS(decode_bytes(good, read), Status::ok);
  CHECK_EQ(read.ticket(), 8u); }
