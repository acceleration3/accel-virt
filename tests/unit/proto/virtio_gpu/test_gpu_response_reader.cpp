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
  CHECK(reader.ok());
  response.set(0x0100u);   /* a request type, not an answer */
  CHECK_STATUS(reader.decode(response.bytes(), response.length()), Status::ok);
  CHECK(!reader.ok()); }

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
  CHECK(sweep.cases_run() > 1000u);
  /* An answer is a 24-byte virtio-gpu header and, for this type, the 128-byte
   * capset behind it. The reader reports the type rather than judging it - a
   * type it does not know is a header-only answer, which is why the all-zero
   * and all-ones cases are accepted and every one of the type's 32 flips is -
   * and the header's other 20 bytes are the fence and context ids it does not
   * read, 160 more. What is judged is the capset: 32 free bits of protocol
   * version, 64 of features, 64 of the two PCI ids, 128 of UUID and 128 of the
   * two memory sizes; two flips of the sub-api mask that stay inside 0x7 and
   * non-empty, 31 of the context count, two of the lowest ring order and one of
   * the highest that stay inside [12, 24] and in order, two of the device type
   * that stay within VkPhysicalDeviceType, and 32 of the name - its three
   * characters, none of which a single flip can turn into a terminator, and the
   * terminator itself, which just moves one byte on. Every shorter input cuts
   * the capset short, and the untouched answer is the last acceptance. */
  const std::uint32_t acceptable = 2u + 32u + 160u + 32u + 64u + 64u + 128u + 128u +
                                   2u + 31u + 2u + 1u + 2u + 32u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }

/* Feature 041: the answer to RESOURCE_MAP_BLOB. The device says how the guest
 * may cache what it just mapped, and the driver has to read it rather than
 * assume it: a blob it thinks is mapped and is not would hand a process
 * somebody else's memory. */
TEST(gpu_response_reader_decode, reads_the_map_info_a_mapped_blob_answers)
{ GpuResponse mapped;
  mapped.set_map_info(GpuResponse::map_cache_cached);
  GpuResponseReader reader;
  CHECK_STATUS(reader.decode(mapped.bytes(), mapped.length()), Status::ok);
  CHECK_EQ(reader.type(), GpuResponse::ok_map_info);
  CHECK(reader.ok());
  CHECK_EQ(reader.map_info(), GpuResponse::map_cache_cached); }

TEST(gpu_response_reader_decode, a_short_map_info_is_malformed)
{ GpuResponse mapped;
  mapped.set_map_info(GpuResponse::map_cache_cached);
  GpuResponseReader reader;
  CHECK_STATUS(reader.decode(mapped.bytes(), 27u), Status::malformed);
  CHECK_EQ(reader.map_info(), 0u); }

/* Every other answer leaves the map info where it was: zero, which is no
 * caching anyone asked for. */
TEST(gpu_response_reader_decode, an_answer_that_is_not_map_info_carries_none)
{ GpuResponse nodata;
  nodata.set(GpuResponse::ok_nodata);
  GpuResponseReader reader;
  CHECK_STATUS(reader.decode(nodata.bytes(), nodata.length()), Status::ok);
  CHECK_EQ(reader.map_info(), 0u); }
