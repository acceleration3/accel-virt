/* The answers the device core writes, read back as the driver reads them. */
#include "core/support/status.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"
#include "proto/virtio_gpu/gpu_response_reader.hpp"

#include <cstdint>
#include <cstring>

using av::CapsetInfo;
using av::GpuIdentity;
using av::GpuResponse;
using av::GpuResponseReader;
using av::ProtocolVersion;
using av::Status;
using av::SubApi;

namespace {

constexpr std::uint8_t uuid[GpuIdentity::uuid_bytes] = {0x42};

CapsetInfo capset()
{
    return CapsetInfo(ProtocolVersion(1u, 0u), static_cast<std::uint32_t>(SubApi::vulkan), 64u, 12u, 24u, 1u,
                      GpuIdentity(0x10DEu, 0x24E0u, GpuIdentity::discrete_gpu, uuid, 8u, 16u, "gpu"));
}

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    GpuResponseReader reader;
    return reader.decode(bytes, length) != Status::ok;
}

}  // namespace

TEST(gpu_response_reader_decode, reads_a_header_only_answer)
{ GpuResponse response;
  response.set(GpuResponse::err_invalid_context_id);
  GpuResponseReader reader;
  CHECK_STATUS(reader.decode(response.bytes(), response.length()), Status::ok);
  CHECK_EQ(reader.type(), GpuResponse::err_invalid_context_id);
  CHECK(!reader.ok());
  response.set(GpuResponse::ok_nodata);
  CHECK_STATUS(reader.decode(response.bytes(), response.length()), Status::ok);
  CHECK(reader.ok()); }

TEST(gpu_response_reader_decode, reads_capset_info)
{ GpuResponse response;
  response.set_capset_info(0x72u, 1u, 128u);
  GpuResponseReader reader;
  CHECK_STATUS(reader.decode(response.bytes(), response.length()), Status::ok);
  CHECK_EQ(reader.type(), GpuResponse::ok_capset_info);
  CHECK(reader.ok());
  CHECK_EQ(reader.capset_id(), 0x72u);
  CHECK_EQ(reader.capset_max_version(), 1u);
  CHECK_EQ(reader.capset_max_size(), 128u); }

TEST(gpu_response_reader_decode, reads_the_capset)
{ GpuResponse response;
  response.set_capset(capset());
  GpuResponseReader reader;
  CHECK_STATUS(reader.decode(response.bytes(), response.length()), Status::ok);
  CHECK_EQ(reader.type(), GpuResponse::ok_capset);
  CHECK_EQ(reader.capset().max_contexts(), 64u);
  CHECK_EQ(reader.capset().gpu().vendor_id(), 0x10DEu);
  CHECK(reader.capset().offers(SubApi::vulkan)); }

TEST(gpu_response_reader_decode, an_answer_short_of_its_type_or_a_capset_that_does_not_decode_is_malformed)
{ GpuResponse info;
  info.set_capset_info(0x72u, 1u, 128u);
  GpuResponse whole;
  whole.set_capset(capset());
  std::uint8_t bytes[GpuResponse::max_length];
  std::memcpy(bytes, whole.bytes(), whole.length());
  bytes[24] = 0u;   /* the capset's magic */
  GpuResponseReader reader;
  CHECK_STATUS(reader.decode(info.bytes(), info.length()), Status::ok);
  CHECK_STATUS(reader.decode(info.bytes(), 39u), Status::malformed);
  CHECK_STATUS(reader.decode(whole.bytes(), whole.length() - 1u), Status::malformed);
  CHECK_STATUS(reader.decode(bytes, whole.length()), Status::malformed);
  CHECK_STATUS(reader.decode(info.bytes(), 23u), Status::malformed);
  CHECK_STATUS(reader.decode(nullptr, 24u), Status::malformed);
  CHECK_EQ(reader.type(), GpuResponse::ok_capset_info);   /* the last good answer */
  CHECK_EQ(reader.capset_id(), 0x72u); }

TEST(gpu_response_reader_decode, survives_the_hostile_sweep)
{ GpuResponse whole;
  whole.set_capset(capset());
  const av::HostileSweep sweep(rejects, whole.bytes(), whole.length());
  sweep.run();
  CHECK(sweep.cases_run() > 1000u); }
