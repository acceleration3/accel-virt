#include "harness/framework/test_registry.hpp"
#include "proto/dma/present_operation.hpp"
#include "proto/dma/present_region.hpp"
#include "proto/resources/surface_pair.hpp"
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
using av::SurfacePair;
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

}

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

TEST(queued_present_request, every_rule_broken_is_malformed_and_changes_nothing)
{ QueuedPresentRequest read;
  CHECK_STATUS(decode(QueuedPresentRequest(7u, blt(0u, 0u, 1u, 1u, 64u, 1u, 64u)), read), Status::ok);
  const QueuedPresentRequest broken[] = {
      QueuedPresentRequest(0u, blt(0u, 0u, 1u, 1u, 64u, 1u, 64u)),
      QueuedPresentRequest(8u, PresentCopyRequest(PresentOperation::flip, 0u, 0u, 1u, 1u, 64u, 1u, 64u)),
      QueuedPresentRequest(8u, PresentCopyRequest(static_cast<PresentOperation>(0u), 0u, 0u, 1u, 1u, 64u, 1u, 64u)),
      QueuedPresentRequest(8u, blt(0u, 5u, 1u, 1u, 64u, 1u, 64u)),
      QueuedPresentRequest(8u, blt(0u, 0u, 0u, 1u, 64u, 1u, 64u)),
      QueuedPresentRequest(8u, blt(0u, 0u, max_regions + 1u, 1u, 64u, 1u, 64u)),
      QueuedPresentRequest(8u, blt(0u, 0u, 1u, 0u, 64u, 1u, 64u)),
      QueuedPresentRequest(8u, blt(0u, 0u, 1u, 0u, 0u, 1u, 64u)),
      QueuedPresentRequest(8u, blt(0u, 0u, 1u, max_entries + 1u, 64u, 1u, 64u)),
      QueuedPresentRequest(8u, blt(0u, 0u, 1u, 1u, 0u, 1u, 64u)),
      QueuedPresentRequest(8u, blt(0u, 0u, 1u, 1u, 66u, 1u, 64u)),
      QueuedPresentRequest(8u, blt(0u, 0u, 1u, 1u, 64u, 0u, 64u)),
      QueuedPresentRequest(8u, blt(0u, 0u, 1u, 1u, 64u, max_entries + 1u, 64u)),
      QueuedPresentRequest(8u, blt(0u, 0u, 1u, 1u, 64u, 1u, 0u)),
      QueuedPresentRequest(8u, blt(0u, 0u, 1u, 1u, 64u, 1u, 62u)),
      QueuedPresentRequest(8u, fill(swap, 0u, 0u)),
      QueuedPresentRequest(8u, fill(0u, 1u, 0u)),
      QueuedPresentRequest(8u, fill(0u, 0u, 64u)),
      QueuedPresentRequest(8u, SurfacePoint(0u, point), blt(0u, 0u, 1u, 0u, 0u, 1u, 64u)),
      QueuedPresentRequest(8u, SurfacePoint(surface, 0u), blt(0u, 0u, 1u, 0u, 0u, 1u, 64u)),
      QueuedPresentRequest(8u, SurfacePoint(surface, point), blt(0u, 0u, 1u, 1u, 0u, 1u, 64u)),
      QueuedPresentRequest(8u, SurfacePoint(surface, point), blt(0u, 0u, 1u, 0u, 64u, 1u, 64u)),
      QueuedPresentRequest(8u, SurfacePoint(surface, point), fill(0u, 0u, 0u)),
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
  const Damage damages[] = {{32u, 2u},
                            {36u, 1u},
                            {39u, 1u},
                            {40u, 1u},
                            {52u, 2u},
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

TEST(queued_present_request, a_surface_copy_names_its_destination_in_the_header_and_its_source_without_a_point)
{ const Encoded encoded = encode(QueuedPresentRequest(6u, SurfacePair(surface, 0xAABBCCDDEEFF0011u), blt(0u, 0u, 2u, 0u, 0u, 0u, 0u)));
  CHECK_EQ(field_at<std::uint32_t>(encoded.bytes, 0u), 0x02F7u);
  CHECK_EQ(field_at<std::uint64_t>(encoded.bytes, 4u), 0xAABBCCDDEEFF0011u);
  for (std::uint32_t at = 12u; at < 24u; at++) CHECK_EQ(encoded.bytes[at], 0u);
  CHECK_EQ(field_at<std::uint64_t>(encoded.bytes, 24u), 6u);
  CHECK_EQ(field_at<std::uint32_t>(encoded.bytes, 32u), 2u);
  CHECK_EQ(field_at<std::uint32_t>(encoded.bytes, 36u), 0u);
  CHECK_EQ(field_at<std::uint64_t>(encoded.bytes, 40u), surface); }

TEST(queued_present_request, a_surface_copy_reads_back_with_no_pages_and_its_regions_right_after_the_fixed_part)
{ QueuedPresentRequest read;
  CHECK_STATUS(decode(QueuedPresentRequest(6u, SurfacePair(surface, 77u), blt(0u, 0u, 3u, 0u, 0u, 0u, 0u)), read), Status::ok);
  CHECK(read.source() == Source::surfaces);
  CHECK(read.surfaces() == SurfacePair(surface, 77u));
  CHECK_EQ(read.copy().regions(), 3u);
  CHECK_EQ(read.regions_offset(), 80u);
  CHECK_EQ(read.size(), 80u + 3u * PresentRegion::encoded_size);
  CHECK_STATUS(decode(QueuedPresentRequest(6u, SurfacePair(surface, 77u), blt(0u, 0u, max_regions, 0u, 0u, 0u, 0u)), read), Status::ok); }

TEST(queued_present_request, a_surface_copy_with_a_missing_key_pages_a_swap_a_fill_or_too_many_regions_is_malformed)
{ QueuedPresentRequest read;
  CHECK_STATUS(decode(QueuedPresentRequest(6u, SurfacePair(0u, 77u), blt(0u, 0u, 1u, 0u, 0u, 0u, 0u)), read), Status::malformed);
  CHECK_STATUS(decode(QueuedPresentRequest(6u, SurfacePair(surface, 0u), blt(0u, 0u, 1u, 0u, 0u, 0u, 0u)), read), Status::malformed);
  CHECK_STATUS(decode(QueuedPresentRequest(0u, SurfacePair(surface, 77u), blt(0u, 0u, 1u, 0u, 0u, 0u, 0u)), read), Status::malformed);
  CHECK_STATUS(decode(QueuedPresentRequest(6u, SurfacePair(surface, 77u), blt(swap, 0u, 1u, 0u, 0u, 0u, 0u)), read), Status::malformed);
  CHECK_STATUS(decode(QueuedPresentRequest(6u, SurfacePair(surface, 77u), blt(0u, 0u, 0u, 0u, 0u, 0u, 0u)), read), Status::malformed);
  CHECK_STATUS(decode(QueuedPresentRequest(6u, SurfacePair(surface, 77u), blt(0u, 0u, max_regions + 1u, 0u, 0u, 0u, 0u)), read), Status::malformed);
  CHECK_STATUS(decode(QueuedPresentRequest(6u, SurfacePair(surface, 77u), blt(0u, 0u, 1u, 1u, 4u, 0u, 0u)), read), Status::malformed);
  CHECK_STATUS(decode(QueuedPresentRequest(6u, SurfacePair(surface, 77u), blt(0u, 0u, 1u, 0u, 0u, 1u, 4u)), read), Status::malformed);
  CHECK_STATUS(decode(QueuedPresentRequest(6u, SurfacePair(surface, 77u), blt(0u, 5u, 1u, 0u, 0u, 0u, 0u)), read), Status::malformed);
  CHECK_STATUS(decode(QueuedPresentRequest(6u, SurfacePair(surface, 77u), PresentCopyRequest(PresentOperation::color_fill, 0u, 1u, 1u, 0u, 0u, 0u, 0u)), read), Status::malformed); }

TEST(queued_present_request, a_destination_in_the_header_of_a_page_or_image_request_is_malformed)
{ Encoded encoded = encode(QueuedPresentRequest(9u, blt(0u, 0u, 1u, 1u, 64u, 1u, 64u)));
  const std::uint64_t destination = 5u;
  std::memcpy(encoded.bytes + 4u, &destination, sizeof destination);
  QueuedPresentRequest read;
  CHECK_STATUS(decode_bytes(encoded, read), Status::malformed);
  Encoded image = encode(QueuedPresentRequest(4u, SurfacePoint(surface, point), blt(0u, 0u, 2u, 0u, 0u, 3u, 256u)));
  std::memcpy(image.bytes + 4u, &destination, sizeof destination);
  CHECK_STATUS(decode_bytes(image, read), Status::malformed); }

TEST(queued_present_request, a_texture_blt_names_its_source_key_with_no_point_and_the_destination_pages)
{ const Encoded encoded = encode(QueuedPresentRequest(8u, surface, blt(0u, 0u, 2u, 0u, 0u, 3u, 256u)));
  CHECK_EQ(field_at<std::uint32_t>(encoded.bytes, 32u), 3u);
  CHECK_EQ(field_at<std::uint32_t>(encoded.bytes, 36u), 0u);
  CHECK_EQ(field_at<std::uint64_t>(encoded.bytes, 40u), surface);
  CHECK_EQ(field_at<std::uint64_t>(encoded.bytes, 4u), 0u);
  QueuedPresentRequest read;
  CHECK_STATUS(decode_bytes(encoded, read), Status::ok);
  CHECK(read.source() == Source::texture);
  CHECK_EQ(read.image().surface(), surface);
  CHECK_EQ(read.image().point(), 0u);
  CHECK_EQ(read.copy().destination_entries(), 3u);
  CHECK_EQ(read.destination_entries_offset(), 80u); }

TEST(queued_present_request, a_texture_blt_with_a_point_pages_for_a_source_no_destination_or_a_fill_is_malformed)
{ QueuedPresentRequest read;
  Encoded pointed = encode(QueuedPresentRequest(8u, surface, blt(0u, 0u, 2u, 0u, 0u, 3u, 256u)));
  const std::uint32_t one = 1u;
  std::memcpy(pointed.bytes + 36u, &one, sizeof one);
  CHECK_STATUS(decode_bytes(pointed, read), Status::malformed);
  CHECK_STATUS(decode(QueuedPresentRequest(8u, 0u, blt(0u, 0u, 2u, 0u, 0u, 3u, 256u)), read), Status::malformed);
  CHECK_STATUS(decode(QueuedPresentRequest(8u, surface, blt(0u, 0u, 2u, 1u, 64u, 3u, 256u)), read), Status::malformed);
  CHECK_STATUS(decode(QueuedPresentRequest(8u, surface, blt(0u, 0u, 2u, 0u, 0u, 0u, 256u)), read), Status::malformed);
  CHECK_STATUS(decode(QueuedPresentRequest(0u, surface, blt(0u, 0u, 2u, 0u, 0u, 3u, 256u)), read), Status::malformed);
  CHECK_STATUS(decode(QueuedPresentRequest(8u, surface, PresentCopyRequest(PresentOperation::color_fill, 0u, 1u, 1u, 0u, 0u, 2u, 64u)), read), Status::malformed);
  Encoded unknown = encode(QueuedPresentRequest(8u, surface, blt(0u, 0u, 2u, 0u, 0u, 3u, 256u)));
  const std::uint32_t four = 4u;
  std::memcpy(unknown.bytes + 32u, &four, sizeof four);
  CHECK_STATUS(decode_bytes(unknown, read), Status::malformed); }
