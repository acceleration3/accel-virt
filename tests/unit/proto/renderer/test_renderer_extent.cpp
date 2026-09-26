#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/renderer/renderer_extent.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstddef>
#include <cstdint>

using av::ByteLayout;
using av::RendererExtent;
using av::Status;
using av::WireReader;
using av::WireWriter;

TEST(renderer_extent_write, is_the_region_above_the_offset_then_the_length)
{ std::uint8_t bytes[RendererExtent::encoded_size] = {};
  WireWriter writer(bytes, sizeof bytes);
  RendererExtent(0xABCDEFu, 0x12345678ABull, 0x1000u).write(writer);
  CHECK_STATUS(writer.status(), Status::ok);
  const ByteLayout layout(bytes, sizeof bytes);
  CHECK(layout.at(0u, 8u) == 0xABCDEF12345678ABull);
  CHECK(layout.at(8u, 8u) == 0x1000u); }

TEST(renderer_extent_read, splits_the_address_back_into_region_and_offset)
{ std::uint8_t bytes[RendererExtent::encoded_size] = {};
  WireWriter writer(bytes, sizeof bytes);
  RendererExtent(RendererExtent::region_count - 1u, RendererExtent::region_span - 1u, 1u).write(writer);
  RendererExtent extent;
  WireReader reader(bytes, sizeof bytes);
  extent.read(reader);
  CHECK_STATUS(reader.status(), Status::ok);
  CHECK_EQ(extent.region(), RendererExtent::region_count - 1u);
  CHECK(extent.offset() == RendererExtent::region_span - 1u);
  CHECK(extent.length() == 1u);
  CHECK(extent.fits()); }

TEST(renderer_extent, a_default_extent_is_empty_and_does_not_fit)
{ const RendererExtent extent;
  CHECK_EQ(extent.region(), 0u);
  CHECK(extent.offset() == 0u);
  CHECK(extent.length() == 0u);
  CHECK(!extent.fits()); }

/* The address has 24 bits of region and 40 of offset; an extent that ends past
 * its region would reach whatever the renderer mapped next to it. */
TEST(renderer_extent_fits, only_a_nameable_region_and_a_run_ending_inside_it)
{ const std::uint64_t span = RendererExtent::region_span;
  CHECK(RendererExtent(0u, 0u, span).fits());
  CHECK(RendererExtent(0u, span - 0x1000u, 0x1000u).fits());
  CHECK(!RendererExtent(0u, span - 0x1000u, 0x1001u).fits());
  CHECK(!RendererExtent(0u, 0u, span + 1u).fits());
  CHECK(!RendererExtent(0u, 0x1000u, ~std::uint64_t{0}).fits());
  CHECK(!RendererExtent(0u, span, 1u).fits());
  CHECK(!RendererExtent(RendererExtent::region_count, 0u, 1u).fits());
  CHECK(!RendererExtent(1u, 0u, 0u).fits()); }

TEST(renderer_extent_read, short_bytes_latch_the_reader)
{ const std::uint8_t bytes[RendererExtent::encoded_size - 1u] = {};
  RendererExtent extent;
  WireReader reader(bytes, sizeof bytes);
  extent.read(reader);
  CHECK_STATUS(reader.status(), Status::malformed); }
