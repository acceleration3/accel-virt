#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/virtio_gpu/blob_request.hpp"
#include "proto/virtio_gpu/create_blob_request.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstdint>
#include <cstring>

using av::BlobRequest;
using av::CreateBlobRequest;
using av::Status;
using av::WireReader;

namespace {

void put32(std::uint8_t *bytes, std::size_t offset, std::uint32_t value) { std::memcpy(bytes + offset, &value, 4); }
void put64(std::uint8_t *bytes, std::size_t offset, std::uint64_t value) { std::memcpy(bytes + offset, &value, 8); }

}

TEST(create_blob_request_decode, reads_the_fixed_part_after_the_header)
{ std::uint8_t bytes[CreateBlobRequest::fixed_size + CreateBlobRequest::entry_size] = {};
  put32(bytes, 0u, 0x010Cu);
  put32(bytes, 24u, 7u);
  put32(bytes, 28u, 2u);
  put32(bytes, 32u, 1u);
  put32(bytes, 36u, 3u);
  put64(bytes, 40u, 0x1122334455667788ull);
  put64(bytes, 48u, 0x10000u);
  WireReader reader(bytes, sizeof bytes);
  CreateBlobRequest request;
  CHECK_STATUS(request.decode(reader), Status::ok);
  CHECK_EQ(request.resource_id(), 7u);
  CHECK_EQ(request.memory(), 2u);
  CHECK_EQ(request.flags(), 1u);
  CHECK_EQ(request.entries(), 3u);
  CHECK_EQ(request.blob_id(), 0x1122334455667788ull);
  CHECK_EQ(request.size(), 0x10000u);
  CHECK_EQ(reader.offset(), CreateBlobRequest::fixed_size); }

TEST(create_blob_request_decode, a_short_request_is_malformed)
{ std::uint8_t bytes[CreateBlobRequest::fixed_size - 1u] = {};
  WireReader reader(bytes, sizeof bytes);
  CreateBlobRequest request;
  CHECK_STATUS(request.decode(reader), Status::malformed); }

TEST(blob_request_decode_resource, reads_the_id_of_a_32_byte_request)
{ std::uint8_t bytes[32] = {};
  put32(bytes, 24u, 9u);
  put32(bytes, 28u, 0xFFFFFFFFu);
  WireReader reader(bytes, sizeof bytes);
  BlobRequest request;
  CHECK_STATUS(request.decode_resource(reader), Status::ok);
  CHECK_EQ(request.resource_id(), 9u);
  WireReader short_reader(bytes, 31u);
  CHECK_STATUS(request.decode_resource(short_reader), Status::malformed); }

TEST(blob_request_decode_map, reads_the_id_and_offset)
{ std::uint8_t bytes[40] = {};
  put32(bytes, 24u, 5u);
  put64(bytes, 32u, 0x20000u);
  WireReader reader(bytes, sizeof bytes);
  BlobRequest request;
  CHECK_STATUS(request.decode_map(reader), Status::ok);
  CHECK_EQ(request.resource_id(), 5u);
  CHECK_EQ(request.offset(), 0x20000u);
  WireReader short_reader(bytes, 39u);
  CHECK_STATUS(request.decode_map(short_reader), Status::malformed); }
