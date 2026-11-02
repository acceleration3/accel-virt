#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/virtio_gpu/flush_request.hpp"
#include "proto/virtio_gpu/gpu_rect.hpp"
#include "proto/virtio_gpu/set_scanout_blob_request.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstdint>
#include <cstring>

using av::FlushRequest;
using av::GpuRect;
using av::SetScanoutBlobRequest;
using av::Status;
using av::WireReader;

namespace {

constexpr std::uint32_t max32 = 0xFFFFFFFFu;

void put32(std::uint8_t *bytes, std::size_t offset, std::uint32_t value) { std::memcpy(bytes + offset, &value, 4); }

struct Scanout {
    std::uint8_t bytes[SetScanoutBlobRequest::encoded_size] = {};

    Scanout()
    {
        put32(bytes, 0u, 0x010Du);
        rect(0u, 0u, 1280u, 800u);
        put32(bytes, 40u, 0u);
        put32(bytes, 44u, 7u);
        put32(bytes, 48u, 1280u);
        put32(bytes, 52u, 800u);
        put32(bytes, 56u, 1u);
        put32(bytes, 64u, 5120u);
    }

    void rect(std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height)
    {
        put32(bytes, 24u, x);
        put32(bytes, 28u, y);
        put32(bytes, 32u, width);
        put32(bytes, 36u, height);
    }

    Status decode(SetScanoutBlobRequest &request, std::size_t length = SetScanoutBlobRequest::encoded_size)
    {
        WireReader reader(bytes, length);
        return request.decode(reader);
    }
};

struct Flush {
    std::uint8_t bytes[FlushRequest::encoded_size] = {};

    Flush()
    {
        put32(bytes, 0u, 0x0104u);
        put32(bytes, 24u, 16u);
        put32(bytes, 28u, 32u);
        put32(bytes, 32u, 64u);
        put32(bytes, 36u, 48u);
        put32(bytes, 40u, 7u);
    }

    Status decode(FlushRequest &request, std::size_t length = FlushRequest::encoded_size)
    {
        WireReader reader(bytes, length);
        return request.decode(reader);
    }
};

}

TEST(gpu_rect_decode, reads_the_four_fields_in_wire_order)
{ std::uint8_t bytes[GpuRect::encoded_size] = {};
  put32(bytes, 0u, 3u);
  put32(bytes, 4u, 5u);
  put32(bytes, 8u, 100u);
  put32(bytes, 12u, 50u);
  WireReader reader(bytes, sizeof bytes);
  GpuRect rect;
  CHECK_STATUS(rect.decode(reader), Status::ok);
  CHECK_EQ(rect.x(), 3u);
  CHECK_EQ(rect.y(), 5u);
  CHECK_EQ(rect.width(), 100u);
  CHECK_EQ(rect.height(), 50u);
  CHECK(rect.decoded());
  CHECK_EQ(reader.offset(), GpuRect::encoded_size); }

TEST(gpu_rect_decode, a_short_buffer_leaves_every_field_zero)
{ std::uint8_t bytes[GpuRect::encoded_size] = {};
  put32(bytes, 0u, max32);
  put32(bytes, 8u, max32);
  WireReader reader(bytes, GpuRect::encoded_size - 1u);
  GpuRect rect;
  CHECK_STATUS(rect.decode(reader), Status::malformed);
  CHECK(!rect.decoded());
  CHECK_EQ(rect.x(), 0u);
  CHECK_EQ(rect.width(), 0u); }

TEST(gpu_rect_inside, the_exact_fit_is_inside_and_one_past_each_edge_is_not)
{ std::uint8_t bytes[GpuRect::encoded_size] = {};
  const auto decode = [&bytes](std::uint32_t x, std::uint32_t y, std::uint32_t w, std::uint32_t h) {
      put32(bytes, 0u, x); put32(bytes, 4u, y); put32(bytes, 8u, w); put32(bytes, 12u, h);
      WireReader reader(bytes, sizeof bytes);
      GpuRect rect;
      CHECK_STATUS(rect.decode(reader), Status::ok);
      return rect;
  };
  CHECK(decode(0u, 0u, 1280u, 800u).inside(1280u, 800u));
  CHECK(decode(1279u, 799u, 1u, 1u).inside(1280u, 800u));
  CHECK(!decode(0u, 0u, 1281u, 800u).inside(1280u, 800u));
  CHECK(!decode(0u, 0u, 1280u, 801u).inside(1280u, 800u));
  CHECK(!decode(1u, 0u, 1280u, 800u).inside(1280u, 800u));
  CHECK(!decode(0u, 1u, 1280u, 800u).inside(1280u, 800u)); }

TEST(gpu_rect_inside, a_side_of_zero_is_inside_nothing)
{ std::uint8_t bytes[GpuRect::encoded_size] = {};
  put32(bytes, 8u, 0u);
  put32(bytes, 12u, 800u);
  WireReader reader(bytes, sizeof bytes);
  GpuRect rect;
  CHECK_STATUS(rect.decode(reader), Status::ok);
  CHECK(!rect.inside(1280u, 800u));
  put32(bytes, 8u, 1280u);
  put32(bytes, 12u, 0u);
  WireReader second(bytes, sizeof bytes);
  GpuRect flat;
  CHECK_STATUS(flat.decode(second), Status::ok);
  CHECK(!flat.inside(1280u, 800u)); }

TEST(gpu_rect_inside, an_origin_that_would_wrap_a_32_bit_sum_is_not_inside)
{ std::uint8_t bytes[GpuRect::encoded_size] = {};
  put32(bytes, 0u, max32);
  put32(bytes, 8u, 1u);
  put32(bytes, 12u, 1u);
  WireReader reader(bytes, sizeof bytes);
  GpuRect rect;
  CHECK_STATUS(rect.decode(reader), Status::ok);
  CHECK(!rect.inside(max32, max32)); }

TEST(set_scanout_blob_request, reads_the_desktop_a_guest_sends)
{ Scanout wire;
  SetScanoutBlobRequest request;
  CHECK_STATUS(wire.decode(request), Status::ok);
  CHECK_EQ(request.scanout_id(), 0u);
  CHECK_EQ(request.resource_id(), 7u);
  CHECK_EQ(request.width(), 1280u);
  CHECK_EQ(request.height(), 800u);
  CHECK_EQ(request.format(), 1u);
  CHECK_EQ(request.stride(), 5120u);
  CHECK_EQ(request.offset(), 0u);
  CHECK(request.rect().inside(1280u, 800u)); }

TEST(set_scanout_blob_request, truncation_at_every_field_boundary_is_malformed)
{ for (std::size_t length = 0u; length < SetScanoutBlobRequest::encoded_size; length += 4u) {
      Scanout wire;
      SetScanoutBlobRequest request;
      CHECK_STATUS(wire.decode(request, length), Status::malformed);
      CHECK(!request.decoded());
      CHECK_EQ(request.resource_id(), 0u);
      CHECK_EQ(request.width(), 0u);
      CHECK_EQ(request.stride(), 0u);
  } }

TEST(set_scanout_blob_request, a_non_zero_padding_word_is_malformed)
{ Scanout wire;
  put32(wire.bytes, 60u, 1u);
  SetScanoutBlobRequest request;
  CHECK_STATUS(wire.decode(request), Status::malformed);
  CHECK_EQ(request.width(), 0u); }

TEST(set_scanout_blob_request, a_second_plane_is_refused_rather_than_dropped)
{ for (std::size_t plane = 1u; plane < SetScanoutBlobRequest::planes; plane++) {
      Scanout strided;
      put32(strided.bytes, 64u + plane * 4u, 4096u);
      SetScanoutBlobRequest with_stride;
      CHECK_STATUS(strided.decode(with_stride), Status::malformed);
      CHECK_EQ(with_stride.stride(), 0u);

      Scanout offsetted;
      put32(offsetted.bytes, 80u + plane * 4u, 4096u);
      SetScanoutBlobRequest with_offset;
      CHECK_STATUS(offsetted.decode(with_offset), Status::malformed);
      CHECK_EQ(with_offset.offset(), 0u);
  } }

TEST(set_scanout_blob_request, the_maximum_of_every_scalar_survives_the_decode)
{ Scanout wire;
  wire.rect(max32, max32, max32, max32);
  put32(wire.bytes, 40u, max32);
  put32(wire.bytes, 44u, max32);
  put32(wire.bytes, 48u, max32);
  put32(wire.bytes, 52u, max32);
  put32(wire.bytes, 56u, max32);
  put32(wire.bytes, 64u, max32);
  put32(wire.bytes, 80u, max32);
  SetScanoutBlobRequest request;
  CHECK_STATUS(wire.decode(request), Status::ok);
  CHECK_EQ(request.scanout_id(), max32);
  CHECK_EQ(request.resource_id(), max32);
  CHECK_EQ(request.width(), max32);
  CHECK_EQ(request.height(), max32);
  CHECK_EQ(request.format(), max32);
  CHECK_EQ(request.stride(), max32);
  CHECK_EQ(request.offset(), max32);
  CHECK_EQ(request.rect().height(), max32); }

TEST(set_scanout_blob_request, a_disable_names_resource_zero_and_is_well_formed)
{ Scanout wire;
  put32(wire.bytes, 44u, 0u);
  wire.rect(0u, 0u, 0u, 0u);
  put32(wire.bytes, 48u, 0u);
  put32(wire.bytes, 52u, 0u);
  put32(wire.bytes, 56u, 0u);
  put32(wire.bytes, 64u, 0u);
  SetScanoutBlobRequest request;
  CHECK_STATUS(wire.decode(request), Status::ok);
  CHECK_EQ(request.resource_id(), 0u);
  CHECK(request.decoded()); }

TEST(flush_request, reads_the_resource_and_the_rectangle)
{ Flush wire;
  FlushRequest request;
  CHECK_STATUS(wire.decode(request), Status::ok);
  CHECK_EQ(request.resource_id(), 7u);
  CHECK_EQ(request.rect().x(), 16u);
  CHECK_EQ(request.rect().y(), 32u);
  CHECK_EQ(request.rect().width(), 64u);
  CHECK_EQ(request.rect().height(), 48u); }

TEST(flush_request, truncation_at_every_field_boundary_is_malformed)
{ for (std::size_t length = 0u; length < FlushRequest::encoded_size; length += 4u) {
      Flush wire;
      FlushRequest request;
      CHECK_STATUS(wire.decode(request, length), Status::malformed);
      CHECK(!request.decoded());
      CHECK_EQ(request.resource_id(), 0u);
      CHECK_EQ(request.rect().width(), 0u);
  } }

TEST(flush_request, a_non_zero_padding_word_is_malformed)
{ Flush wire;
  put32(wire.bytes, 44u, max32);
  FlushRequest request;
  CHECK_STATUS(wire.decode(request), Status::malformed);
  CHECK_EQ(request.resource_id(), 0u); }
