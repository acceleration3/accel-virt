#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/virtio_gpu/capset_request.hpp"
#include "proto/virtio_gpu/context_request.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstdint>
#include <cstring>

using av::CapsetRequest;
using av::ContextRequest;
using av::Status;
using av::WireReader;

namespace {

void put32(std::uint8_t *bytes, std::size_t offset, std::uint32_t value) { std::memcpy(bytes + offset, &value, 4); }

}  // namespace

TEST(context_request_decode_destroy, reads_the_context_id_from_the_header)
{ std::uint8_t bytes[ContextRequest::header_size] = {};
  put32(bytes, 0u, 0x0201u);
  put32(bytes, 16u, 42u);
  bytes[20] = 3u;   /* ring_idx is not the context */
  WireReader reader(bytes, sizeof bytes);
  ContextRequest request;
  CHECK_STATUS(request.decode_destroy(reader), Status::ok);
  CHECK_EQ(request.context_id(), 42u);
  CHECK_EQ(reader.offset(), 24u); }

TEST(context_request_decode_create, reads_the_name_length_and_context_init_past_the_debug_name)
{ std::uint8_t bytes[ContextRequest::create_size] = {};
  put32(bytes, 16u, 7u);
  put32(bytes, 24u, 12u);
  put32(bytes, 28u, 0x172u);
  std::memcpy(bytes + 32u, "vulkan-probe", 12u);
  WireReader reader(bytes, sizeof bytes);
  ContextRequest request;
  CHECK_STATUS(request.decode_create(reader), Status::ok);
  CHECK_EQ(request.context_id(), 7u);
  CHECK_EQ(request.name_length(), 12u);
  CHECK_EQ(request.context_init(), 0x172u);
  CHECK_EQ(reader.offset(), ContextRequest::create_size); }

TEST(context_request_decode_submit, reads_the_command_size)
{ std::uint8_t bytes[ContextRequest::submit_size + 32u] = {};
  put32(bytes, 16u, 9u);
  put32(bytes, 24u, 32u);
  WireReader reader(bytes, sizeof bytes);
  ContextRequest request;
  CHECK_STATUS(request.decode_submit(reader), Status::ok);
  CHECK_EQ(request.context_id(), 9u);
  CHECK_EQ(request.command_size(), 32u);
  CHECK_EQ(reader.offset(), ContextRequest::submit_size); }

TEST(context_request, a_short_request_is_malformed_and_changes_nothing)
{ std::uint8_t bytes[ContextRequest::create_size] = {};
  put32(bytes, 16u, 5u);
  put32(bytes, 24u, 64u);
  ContextRequest request;
  WireReader destroy(bytes, ContextRequest::header_size - 1u);
  CHECK_STATUS(request.decode_destroy(destroy), Status::malformed);
  WireReader create(bytes, ContextRequest::create_size - 1u);
  CHECK_STATUS(request.decode_create(create), Status::malformed);
  WireReader submit(bytes, ContextRequest::submit_size - 1u);
  CHECK_STATUS(request.decode_submit(submit), Status::malformed);
  CHECK_EQ(request.context_id(), 0u);
  CHECK_EQ(request.name_length(), 0u);
  CHECK_EQ(request.context_init(), 0u);
  CHECK_EQ(request.command_size(), 0u); }

TEST(capset_request_decode_info, reads_the_index)
{ std::uint8_t bytes[CapsetRequest::encoded_size] = {};
  put32(bytes, 24u, 2u);
  WireReader reader(bytes, sizeof bytes);
  CapsetRequest request;
  CHECK_STATUS(request.decode_info(reader), Status::ok);
  CHECK_EQ(request.index(), 2u);
  CHECK_EQ(reader.offset(), 32u); }

TEST(capset_request_decode_capset, reads_the_id_and_version)
{ std::uint8_t bytes[CapsetRequest::encoded_size] = {};
  put32(bytes, 24u, 0x72u);
  put32(bytes, 28u, 1u);
  WireReader reader(bytes, sizeof bytes);
  CapsetRequest request;
  CHECK_STATUS(request.decode_capset(reader), Status::ok);
  CHECK_EQ(request.capset_id(), 0x72u);
  CHECK_EQ(request.capset_version(), 1u); }

TEST(capset_request, a_short_request_is_malformed_and_changes_nothing)
{ std::uint8_t bytes[CapsetRequest::encoded_size] = {};
  put32(bytes, 24u, 0x72u);
  CapsetRequest request;
  WireReader info(bytes, CapsetRequest::encoded_size - 1u);
  CHECK_STATUS(request.decode_info(info), Status::malformed);
  WireReader capset(bytes, CapsetRequest::encoded_size - 1u);
  CHECK_STATUS(request.decode_capset(capset), Status::malformed);
  CHECK_EQ(request.index(), 0u);
  CHECK_EQ(request.capset_id(), 0u);
  CHECK_EQ(request.capset_version(), 0u); }
