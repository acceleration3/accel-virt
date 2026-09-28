/* The present copy's fixed part, byte by byte: the driver's own request type
 * in the header, the fields in the order the device reads them, and every rule
 * the device holds a request to - each broken alone - refused as malformed,
 * leaving what was decoded before as it was. */
#include "harness/framework/test_registry.hpp"
#include "proto/commands/present_operation.hpp"
#include "proto/commands/present_region.hpp"
#include "proto/virtio_gpu/present_copy_request.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>

using av::PresentCopyRequest;
using av::PresentOperation;
using av::PresentRegion;
using av::Status;
using av::WireReader;
using av::WireWriter;

namespace {

constexpr std::uint32_t swap = PresentCopyRequest::flag_swap_red_blue;
constexpr std::uint32_t max_entries = PresentCopyRequest::max_entries;
constexpr std::uint32_t max_regions = PresentCopyRequest::max_regions;

struct Encoded {
    std::uint8_t bytes[PresentCopyRequest::encoded_size] = {};
};

Encoded encode(const PresentCopyRequest &request)
{
    Encoded    encoded;
    WireWriter writer(encoded.bytes, sizeof encoded.bytes);
    request.write(writer);
    CHECK_EQ(writer.offset(), PresentCopyRequest::encoded_size);
    return encoded;
}

Status decode(const PresentCopyRequest &request, PresentCopyRequest &into)
{
    const Encoded encoded = encode(request);
    WireReader    reader(encoded.bytes, sizeof encoded.bytes);
    return into.decode(reader);
}

PresentCopyRequest blt(std::uint32_t flags, std::uint32_t colour, std::uint32_t regions, std::uint32_t source_entries,
                       std::uint32_t source_pitch, std::uint32_t destination_entries, std::uint32_t destination_pitch)
{
    return PresentCopyRequest(PresentOperation::blt, flags, colour, regions, source_entries, source_pitch,
                              destination_entries, destination_pitch);
}

PresentCopyRequest fill(std::uint32_t flags, std::uint32_t regions, std::uint32_t source_entries,
                        std::uint32_t source_pitch, std::uint32_t destination_entries, std::uint32_t destination_pitch)
{
    return PresentCopyRequest(PresentOperation::color_fill, flags, 0xFF00FF00u, regions, source_entries, source_pitch,
                              destination_entries, destination_pitch);
}

}  // namespace

TEST(present_copy_request, writes_the_driver_s_type_and_every_field_in_order)
{ const Encoded encoded = encode(blt(swap, 0u, 3u, 5u, 5120u, 7u, 8192u));
  std::uint32_t type = 0u;
  std::memcpy(&type, encoded.bytes, sizeof type);
  CHECK_EQ(type, PresentCopyRequest::type);
  for (std::uint32_t at = 4u; at < 24u; at++) CHECK_EQ(encoded.bytes[at], 0u);
  std::uint32_t fields[8] = {};
  std::memcpy(fields, encoded.bytes + 24u, sizeof fields);
  const std::uint32_t expected[8] = {1u, swap, 0u, 3u, 5u, 7u, 5120u, 8192u};
  for (std::uint32_t index = 0u; index < 8u; index++) CHECK_EQ(fields[index], expected[index]); }

TEST(present_copy_request, a_blt_reads_back_as_written_and_places_its_parts)
{ PresentCopyRequest read;
  CHECK_STATUS(decode(blt(swap, 0u, 3u, 5u, 5120u, 7u, 8192u), read), Status::ok);
  CHECK(read.operation() == PresentOperation::blt);
  CHECK(read.swaps_red_and_blue());
  CHECK_EQ(read.colour(), 0u);
  CHECK_EQ(read.regions(), 3u);
  CHECK_EQ(read.source_entries(), 5u);
  CHECK_EQ(read.source_pitch(), 5120u);
  CHECK_EQ(read.destination_entries(), 7u);
  CHECK_EQ(read.destination_pitch(), 8192u);
  CHECK_EQ(read.destination_entries_offset(), PresentCopyRequest::encoded_size + 5u * PresentCopyRequest::entry_size);
  CHECK_EQ(read.regions_offset(), PresentCopyRequest::encoded_size + 12u * PresentCopyRequest::entry_size);
  CHECK_EQ(read.size(), read.regions_offset() + 3u * PresentRegion::encoded_size);
  /* The largest request the device takes. */
  CHECK_STATUS(decode(blt(0u, 0u, max_regions, max_entries, 4u, max_entries, 4u), read), Status::ok);
  CHECK(!read.swaps_red_and_blue()); }

TEST(present_copy_request, a_fill_has_a_colour_and_no_source)
{ PresentCopyRequest read;
  CHECK_STATUS(decode(fill(0u, 1u, 0u, 0u, 2u, 64u), read), Status::ok);
  CHECK(read.operation() == PresentOperation::color_fill);
  CHECK_EQ(read.colour(), 0xFF00FF00u);
  CHECK_EQ(read.source_entries(), 0u);
  CHECK_EQ(read.destination_entries_offset(), PresentCopyRequest::encoded_size); }

/* Each rule broken alone, after a request that held: the refusal leaves it. */
TEST(present_copy_request, every_rule_broken_is_malformed_and_changes_nothing)
{ PresentCopyRequest read;
  CHECK_STATUS(decode(blt(0u, 0u, 1u, 1u, 64u, 1u, 64u), read), Status::ok);
  const PresentCopyRequest broken[] = {
      PresentCopyRequest(PresentOperation::flip, 0u, 0u, 1u, 1u, 64u, 1u, 64u),
      PresentCopyRequest(static_cast<PresentOperation>(0u), 0u, 0u, 1u, 1u, 64u, 1u, 64u),
      blt(2u, 0u, 1u, 1u, 64u, 1u, 64u),                   /* a flag nothing names */
      blt(0u, 5u, 1u, 1u, 64u, 1u, 64u),                   /* a colour on a blt */
      blt(0u, 0u, 0u, 1u, 64u, 1u, 64u),                   /* no regions */
      blt(0u, 0u, max_regions + 1u, 1u, 64u, 1u, 64u),     /* too many */
      blt(0u, 0u, 1u, 0u, 64u, 1u, 64u),                   /* a blt with no source */
      blt(0u, 0u, 1u, max_entries + 1u, 64u, 1u, 64u),     /* more source than the device maps */
      blt(0u, 0u, 1u, 1u, 0u, 1u, 64u),                    /* no source pitch */
      blt(0u, 0u, 1u, 1u, 66u, 1u, 64u),                   /* a source pitch of part of a pixel */
      blt(0u, 0u, 1u, 1u, 64u, 0u, 64u),                   /* no destination */
      blt(0u, 0u, 1u, 1u, 64u, max_entries + 1u, 64u),     /* more destination than the device maps */
      blt(0u, 0u, 1u, 1u, 64u, 1u, 0u),                    /* no destination pitch */
      blt(0u, 0u, 1u, 1u, 64u, 1u, 62u),                   /* a destination pitch of part of a pixel */
      fill(swap, 1u, 0u, 0u, 1u, 64u),                     /* nothing to swap in a fill */
      fill(0u, 1u, 1u, 0u, 1u, 64u),                       /* a fill with a source */
      fill(0u, 1u, 0u, 64u, 1u, 64u),                      /* a fill with a source pitch */
  };
  for (const PresentCopyRequest &request : broken) {
      CHECK_STATUS(decode(request, read), Status::malformed);
      CHECK(read.operation() == PresentOperation::blt);
      CHECK_EQ(read.source_pitch(), 64u);
      CHECK_EQ(read.regions(), 1u);
  }
  /* Too short to hold the fixed part at all. */
  const Encoded encoded = encode(blt(0u, 0u, 2u, 1u, 64u, 1u, 64u));
  WireReader    reader(encoded.bytes, sizeof encoded.bytes - 1u);
  CHECK_STATUS(read.decode(reader), Status::malformed);
  CHECK_EQ(read.regions(), 1u); }
