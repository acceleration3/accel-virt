#include "harness/framework/test_registry.hpp"
#include "proto/dma/present_region.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>

using av::PresentRegion;
using av::Status;
using av::WireReader;
using av::WireWriter;

TEST(present_region, is_empty_by_default)
{ const PresentRegion region;
  CHECK_EQ(region.width(), 0u);
  CHECK_EQ(region.height(), 0u); }

TEST(present_region, reads_back_what_it_wrote_in_its_order)
{ const PresentRegion region(1u, 2u, 3u, 4u, 5u, 6u);
  std::uint8_t bytes[PresentRegion::encoded_size] = {};
  WireWriter   writer(bytes, sizeof bytes);
  region.write(writer);
  CHECK_EQ(writer.offset(), PresentRegion::encoded_size);
  std::uint32_t fields[6] = {};
  std::memcpy(fields, bytes, sizeof fields);
  for (std::uint32_t index = 0u; index < 6u; index++) CHECK_EQ(fields[index], index + 1u);
  WireReader    reader(bytes, sizeof bytes);
  PresentRegion read;
  CHECK_STATUS(read.decode(reader), Status::ok);
  CHECK_EQ(read.source_x(), 1u);
  CHECK_EQ(read.source_y(), 2u);
  CHECK_EQ(read.destination_x(), 3u);
  CHECK_EQ(read.destination_y(), 4u);
  CHECK_EQ(read.width(), 5u);
  CHECK_EQ(read.height(), 6u); }

TEST(present_region, no_width_no_height_or_too_few_bytes_is_malformed)
{ const PresentRegion shapes[] = {PresentRegion(1u, 1u, 1u, 1u, 0u, 3u), PresentRegion(1u, 1u, 1u, 1u, 3u, 0u)};
  for (const PresentRegion &shape : shapes) {
      std::uint8_t bytes[PresentRegion::encoded_size] = {};
      WireWriter   writer(bytes, sizeof bytes);
      shape.write(writer);
      WireReader    reader(bytes, sizeof bytes);
      PresentRegion kept(9u, 9u, 9u, 9u, 9u, 9u);
      CHECK_STATUS(kept.decode(reader), Status::malformed);
      CHECK_EQ(kept.width(), 9u);
  }
  std::uint8_t short_bytes[PresentRegion::encoded_size - 1u] = {1u};
  WireReader    reader(short_bytes, sizeof short_bytes);
  PresentRegion kept(9u, 9u, 9u, 9u, 9u, 9u);
  CHECK_STATUS(kept.decode(reader), Status::malformed);
  CHECK_EQ(kept.source_x(), 9u); }
