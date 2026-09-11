#include "harness/framework/test_registry.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"

#include <cstdint>
#include <cstring>

using av::CapsetInfo;
using av::GpuIdentity;
using av::GpuResponse;
using av::ProtocolVersion;
using av::Status;

namespace {

std::uint32_t u32(const std::uint8_t *bytes, std::size_t offset)
{
    std::uint32_t value = 0u;
    std::memcpy(&value, bytes + offset, 4);
    return value;
}

}  // namespace

TEST(gpu_response, starts_as_ok_nodata)
{ const GpuResponse response;
  CHECK_EQ(response.length(), 24u);
  CHECK_EQ(u32(response.bytes(), 0u), GpuResponse::ok_nodata);
  CHECK_EQ(response.type(), GpuResponse::ok_nodata); }

TEST(gpu_response_set, an_error_is_a_bare_header)
{ GpuResponse response;
  response.set_map_info(GpuResponse::map_cache_cached);
  response.set(GpuResponse::err_invalid_parameter);
  CHECK_EQ(response.length(), 24u);
  CHECK_EQ(u32(response.bytes(), 0u), 0x1205u);
  CHECK_EQ(u32(response.bytes(), 24u), 0u);
  for (std::size_t byte = 4u; byte < 24u; byte++) CHECK_EQ(response.bytes()[byte], 0u); }

TEST(gpu_response_set_map_info, is_a_32_byte_map_info)
{ GpuResponse response;
  response.set_map_info(GpuResponse::map_cache_cached);
  CHECK_EQ(response.length(), 32u);
  CHECK_EQ(u32(response.bytes(), 0u), 0x1106u);
  CHECK_EQ(u32(response.bytes(), 24u), 1u);
  CHECK_EQ(u32(response.bytes(), 28u), 0u); }

TEST(gpu_response_set_capset_info, is_a_40_byte_capset_info)
{ GpuResponse response;
  response.set_capset_info(0x72u, 1u, 128u);
  CHECK_EQ(response.length(), 40u);
  CHECK_EQ(response.type(), GpuResponse::ok_capset_info);
  CHECK_EQ(u32(response.bytes(), 0u), 0x1102u);
  CHECK_EQ(u32(response.bytes(), 24u), 0x72u);
  CHECK_EQ(u32(response.bytes(), 28u), 1u);
  CHECK_EQ(u32(response.bytes(), 32u), 128u);
  CHECK_EQ(u32(response.bytes(), 36u), 0u); }

TEST(gpu_response_set_capset, carries_the_capset_encoding_after_the_header)
{ const std::uint8_t uuid[GpuIdentity::uuid_bytes] = {9};
  const CapsetInfo capset(ProtocolVersion(1u, 0u), 1u, 64u, 12u, 24u, 0u,
                          GpuIdentity(0x10DEu, 0x2684u, GpuIdentity::discrete_gpu, uuid, 1u, 2u, "gpu"));
  GpuResponse response;
  response.set_capset(capset);
  CHECK_EQ(response.length(), GpuResponse::max_length);
  CHECK_EQ(response.type(), GpuResponse::ok_capset);
  CHECK_EQ(u32(response.bytes(), 0u), 0x1103u);
  CapsetInfo decoded;
  CHECK_STATUS(decoded.decode(response.bytes() + 24u, CapsetInfo::encoded_size), Status::ok);
  CHECK_EQ(decoded.gpu().vendor_id(), 0x10DEu);
  response.set(GpuResponse::err_invalid_context_id);
  CHECK_EQ(response.length(), 24u);
  for (std::size_t byte = 24u; byte < GpuResponse::max_length; byte++) CHECK_EQ(response.bytes()[byte], 0u); }
