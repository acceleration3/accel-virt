#include "harness/framework/test_registry.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"

#include <cstdint>
#include <cstring>

using av::GpuResponse;

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
