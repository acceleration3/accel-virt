#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/virtio_gpu/av_request_writer.hpp"
#include "proto/virtio_gpu/scanout_plane_request.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstdint>
#include <cstring>

using av::AvRequestWriter;
using av::ScanoutPlaneRequest;
using av::Status;
using av::WireReader;

namespace {

const std::int32_t source[4] = {10, 20, 330, 260};
const std::int32_t destination[4] = {100, 50, 740, 530};

Status round_trip(const ScanoutPlaneRequest &plane, ScanoutPlaneRequest &read)
{
    std::uint8_t    bytes[AvRequestWriter::max_request_size] = {};
    AvRequestWriter writer(bytes, sizeof bytes);
    CHECK_STATUS(writer.scanout_plane(plane), Status::ok);
    CHECK_EQ(writer.length(), ScanoutPlaneRequest::encoded_size);
    std::uint32_t type = 0u;
    std::memcpy(&type, bytes, sizeof type);
    CHECK_EQ(type, ScanoutPlaneRequest::type);
    WireReader reader(bytes, writer.length());
    return read.decode(reader);
}

}

TEST(scanout_plane_request, the_writer_s_request_decodes_to_its_plane)
{ const ScanoutPlaneRequest plane(1u, 2u, ScanoutPlaneRequest::flag_shown | ScanoutPlaneRequest::flag_filtered,
                                  0x5100000002ull, source, destination, 1920u, 1080u);
  ScanoutPlaneRequest read;
  CHECK_STATUS(round_trip(plane, read), Status::ok);
  CHECK_EQ(read.head(), 1u);
  CHECK_EQ(read.layer(), 2u);
  CHECK(read.shown());
  CHECK(read.filtered());
  CHECK_EQ(read.key(), 0x5100000002ull);
  CHECK_EQ(read.source()[2], 330);
  CHECK_EQ(read.destination()[0], 100);
  CHECK_EQ(read.destination()[3], 530);
  CHECK_EQ(read.head_width(), 1920u);
  CHECK_EQ(read.head_height(), 1080u); }

TEST(scanout_plane_request, a_plane_turned_off_needs_no_key_or_rectangles)
{ const ScanoutPlaneRequest off(0u, 1u, 0u, 0u, nullptr, nullptr, 1920u, 1080u);
  ScanoutPlaneRequest read;
  CHECK_STATUS(round_trip(off, read), Status::ok);
  CHECK(!read.shown());
  CHECK_EQ(read.layer(), 1u); }

TEST(scanout_plane_request, a_malformed_plane_decodes_to_nothing)
{ const std::int32_t empty[4] = {10, 20, 10, 260};
  const std::int32_t left_of_image[4] = {-1, 0, 100, 100};
  const std::int32_t flat[4] = {10, 20, 330, 20};
  const std::int32_t above_image[4] = {0, -1, 100, 100};
  const std::uint32_t shown = ScanoutPlaneRequest::flag_shown;
  const ScanoutPlaneRequest refused[] = {
      ScanoutPlaneRequest(0u, 1u, shown, 1u, source, destination, 1920u, 0u),
      ScanoutPlaneRequest(0u, 1u, shown, 1u, source, destination, ScanoutPlaneRequest::most_extent + 1u, 1080u),
      ScanoutPlaneRequest(0u, 1u, shown, 1u, flat, destination, 1920u, 1080u),
      ScanoutPlaneRequest(0u, 1u, shown, 1u, above_image, destination, 1920u, 1080u),
      ScanoutPlaneRequest(0u, ScanoutPlaneRequest::layers, shown, 1u, source, destination, 1920u, 1080u),
      ScanoutPlaneRequest(0u, 1u, shown, 1u, source, destination, 0u, 1080u),
      ScanoutPlaneRequest(0u, 1u, shown, 1u, source, destination, 1920u, ScanoutPlaneRequest::most_extent + 1u),
      ScanoutPlaneRequest(0u, 1u, shown, 0u, source, destination, 1920u, 1080u),
      ScanoutPlaneRequest(0u, 1u, shown, 1u, empty, destination, 1920u, 1080u),
      ScanoutPlaneRequest(0u, 1u, shown, 1u, source, empty, 1920u, 1080u),
      ScanoutPlaneRequest(0u, 1u, shown, 1u, left_of_image, destination, 1920u, 1080u)};
  for (const ScanoutPlaneRequest &plane : refused) {
      ScanoutPlaneRequest read(7u, 3u, shown, 9u, source, destination, 640u, 480u);
      CHECK_STATUS(round_trip(plane, read), Status::malformed);
      CHECK_EQ(read.head(), 7u);
  }
  std::uint8_t short_request[ScanoutPlaneRequest::encoded_size - 1u] = {};
  WireReader   reader(short_request, sizeof short_request);
  ScanoutPlaneRequest read;
  CHECK_STATUS(read.decode(reader), Status::malformed); }

TEST(scanout_plane_request, a_video_plane_carries_its_conversion)
{ const std::uint32_t video = ScanoutPlaneRequest::flag_shown | ScanoutPlaneRequest::flag_ycbcr |
                              ScanoutPlaneRequest::flag_bt709;
  ScanoutPlaneRequest read;
  CHECK_STATUS(round_trip(ScanoutPlaneRequest(0u, 1u, video, 9u, source, destination, 1920u, 1080u), read),
               Status::ok);
  CHECK(read.ycbcr());
  CHECK(read.bt709());
  CHECK(!read.full_range());
  CHECK_STATUS(round_trip(ScanoutPlaneRequest(0u, 1u, ScanoutPlaneRequest::flag_shown, 9u, source, destination,
                                              1920u, 1080u),
                          read),
               Status::ok);
  CHECK(!read.ycbcr()); }
