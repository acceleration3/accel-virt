#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/virtio_gpu/av_request_writer.hpp"
#include "proto/virtio_gpu/shared_release_request.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstdint>
#include <cstring>

using av::AvRequestWriter;
using av::SharedReleaseRequest;
using av::Status;
using av::WireReader;

TEST(shared_release_request, the_writer_s_request_decodes_to_its_id)
{ std::uint8_t bytes[AvRequestWriter::max_request_size] = {};
  AvRequestWriter writer(bytes, sizeof bytes);
  CHECK_STATUS(writer.release_shared(0x1234567890ABCDEFu), Status::ok);
  CHECK_EQ(writer.length(), SharedReleaseRequest::encoded_size);
  std::uint32_t type = 0u;
  std::memcpy(&type, bytes, sizeof type);
  CHECK_EQ(type, SharedReleaseRequest::type);
  WireReader reader(bytes, writer.length());
  SharedReleaseRequest release;
  CHECK_STATUS(release.decode(reader), Status::ok);
  CHECK_EQ(release.object_id(), 0x1234567890ABCDEFu);
  CHECK_EQ(reader.offset(), SharedReleaseRequest::encoded_size); }

TEST(shared_release_request, a_short_request_or_an_id_of_zero_is_malformed)
{ std::uint8_t bytes[SharedReleaseRequest::encoded_size] = {};
  bytes[24] = 7u;
  WireReader short_reader(bytes, sizeof bytes - 1u);
  SharedReleaseRequest short_release;
  CHECK_STATUS(short_release.decode(short_reader), Status::malformed);
  CHECK_EQ(short_release.object_id(), 0u);
  bytes[24] = 0u;
  WireReader zero_reader(bytes, sizeof bytes);
  SharedReleaseRequest zero;
  CHECK_STATUS(zero.decode(zero_reader), Status::malformed);
  CHECK_EQ(zero.object_id(), 0u);
  bytes[31] = 1u;
  WireReader high_reader(bytes, sizeof bytes);
  SharedReleaseRequest high;
  CHECK_STATUS(high.decode(high_reader), Status::ok);
  CHECK_EQ(high.object_id(), 0x0100000000000000u); }

TEST(shared_release_request, a_writer_too_small_writes_nothing)
{ std::uint8_t bytes[SharedReleaseRequest::encoded_size - 1u] = {};
  AvRequestWriter writer(bytes, sizeof bytes);
  CHECK_STATUS(writer.release_shared(1u), Status::too_large);
  CHECK_EQ(writer.length(), 0u);
  AvRequestWriter none(nullptr, 64u);
  CHECK_STATUS(none.release_shared(1u), Status::too_large); }
