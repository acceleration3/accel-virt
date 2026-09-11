#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ByteLayout;
using av::CapsetInfo;
using av::GpuIdentity;
using av::GuardedBuffer;
using av::ProtocolVersion;
using av::Status;
using av::SubApi;

namespace {

const std::uint8_t uuid[GpuIdentity::uuid_bytes] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

CapsetInfo sample()
{
    return CapsetInfo(ProtocolVersion(1u, 3u), 0x7u, 64u, 12u, 20u,
                      CapsetInfo::feature_host_visible_memory,
                      GpuIdentity(0x1002u, 0x744Cu, GpuIdentity::discrete_gpu, uuid,
                                  16ull << 30, 32ull << 30, "AMD Radeon RX 7900 XTX"));
}

class Encoded {
public:
    explicit Encoded(const CapsetInfo &capset)
    {
        status_ = capset.encode(bytes_, sizeof bytes_);
    }
    std::uint8_t *bytes() { return bytes_; }
    Status status() const { return status_; }

private:
    std::uint8_t bytes_[CapsetInfo::encoded_size] = {};
    Status       status_ = Status::ok;
};

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    CapsetInfo capset;
    const Status status = capset.decode(bytes, length);
    if (status == Status::ok) {
        std::uint8_t again[CapsetInfo::encoded_size] = {};
        if (capset.encode(again, sizeof again) != Status::ok ||
            std::memcmp(again, bytes, sizeof again) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}  // namespace

TEST(capset_info, is_capset_0x72_of_128_bytes)
{ CHECK_EQ(CapsetInfo::capset_id, 0x72u);
  CHECK_EQ(CapsetInfo::encoded_size, 128u); }

TEST(capset_info_encode, places_every_field_at_its_offset)
{ Encoded encoded(sample());
  CHECK_STATUS(encoded.status(), Status::ok);
  const ByteLayout layout(encoded.bytes(), CapsetInfo::encoded_size);
  CHECK(layout.at(0u, 4u) == 0x314B5652u);
  CHECK(layout.at(4u, 2u) == 1u);
  CHECK(layout.at(6u, 2u) == 3u);
  CHECK(layout.at(8u, 4u) == 7u);
  CHECK(layout.at(12u, 4u) == 64u);
  CHECK(layout.at(16u, 1u) == 12u);
  CHECK(layout.at(17u, 1u) == 20u);
  CHECK(layout.at(18u, 2u) == 0u);
  CHECK(layout.at(20u, 4u) == 0u);
  CHECK(layout.at(24u, 8u) == 1u);
  CHECK(layout.at(32u, 4u) == 0x1002u);
  CHECK(layout.at(40u, 4u) == 2u);
  CHECK(layout.at(48u, 1u) == 1u);
  CHECK(layout.at(64u, 8u) == (16ull << 30));
  CHECK(layout.at(80u, 1u) == 'A'); }

TEST(capset_info, round_trips_through_its_encoding)
{ Encoded encoded(sample());
  CapsetInfo decoded;
  CHECK_STATUS(decoded.decode(encoded.bytes(), CapsetInfo::encoded_size), Status::ok);
  CHECK_EQ(decoded.version().major_version(), 1u);
  CHECK_EQ(decoded.version().minor_version(), 3u);
  CHECK(decoded.offers(SubApi::vulkan));
  CHECK(decoded.offers(SubApi::opengl));
  CHECK(decoded.offers(SubApi::opencl));
  CHECK_EQ(decoded.max_contexts(), 64u);
  CHECK(decoded.accepts_ring_order(12u));
  CHECK(decoded.accepts_ring_order(20u));
  CHECK(!decoded.accepts_ring_order(11u));
  CHECK(!decoded.accepts_ring_order(21u));
  CHECK(decoded.has_feature(CapsetInfo::feature_host_visible_memory));
  CHECK_EQ(decoded.gpu().vendor_id(), 0x1002u);
  Encoded again(decoded);
  CHECK(std::memcmp(again.bytes(), encoded.bytes(), CapsetInfo::encoded_size) == 0); }

TEST(capset_info_offers, only_the_advertised_sub_apis)
{ CapsetInfo capset(ProtocolVersion(1u, 0u), 0x1u, 1u, 12u, 12u, 0u, GpuIdentity());
  CHECK(capset.offers(SubApi::vulkan));
  CHECK(!capset.offers(SubApi::opengl));
  CHECK(!capset.has_feature(CapsetInfo::feature_host_visible_memory)); }

TEST(capset_info_decode, tolerates_unknown_feature_bits)
{ Encoded encoded(sample());
  encoded.bytes()[31] = 0x80u;
  CapsetInfo decoded;
  CHECK_STATUS(decoded.decode(encoded.bytes(), CapsetInfo::encoded_size), Status::ok);
  CHECK(decoded.has_feature(CapsetInfo::feature_host_visible_memory)); }

TEST(capset_info_decode, accepts_a_longer_buffer)
{ std::uint8_t bytes[CapsetInfo::encoded_size + 8u] = {};
  CHECK_STATUS(sample().encode(bytes, sizeof bytes), Status::ok);
  CapsetInfo decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok); }

TEST(capset_info_decode, rejects_each_malformed_field)
{ struct Case { std::size_t offset; std::uint8_t value; };
  const Case cases[] = {
      {0u, 0x00u},    /* magic */
      {8u, 0x00u},    /* no sub-API */
      {8u, 0x0Fu},    /* unknown sub-API bit */
      {12u, 0x00u},   /* zero contexts (low byte of 64) */
      {16u, 11u},     /* min order below 12 */
      {17u, 25u},     /* max order above 24 */
      {16u, 21u},     /* min above max */
      {18u, 1u},      /* reserved u16 */
      {21u, 1u},      /* reserved u32 */
      {41u, 1u},      /* identity: device type high byte */
      {44u, 1u},      /* identity reserved */
  };
  for (const Case &c : cases) {
      Encoded encoded(sample());
      encoded.bytes()[c.offset] = c.value;
      CapsetInfo decoded;
      CHECK_STATUS(decoded.decode(encoded.bytes(), CapsetInfo::encoded_size), Status::malformed);
  } }

TEST(capset_info_decode, a_rejected_capset_leaves_the_destination_unchanged)
{ Encoded encoded(sample());
  CapsetInfo decoded;
  CHECK_STATUS(decoded.decode(encoded.bytes(), CapsetInfo::encoded_size), Status::ok);
  encoded.bytes()[0] = 0u;
  CHECK_STATUS(decoded.decode(encoded.bytes(), CapsetInfo::encoded_size), Status::malformed);
  CHECK_EQ(decoded.max_contexts(), 64u); }

TEST(capset_info_encode, a_short_destination_is_too_large_and_untouched)
{ GuardedBuffer buffer(CapsetInfo::encoded_size - 1u);
  CHECK_STATUS(sample().encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact()); }

TEST(capset_info_decode, survives_the_hostile_sweep)
{ Encoded encoded(sample());
  const av::HostileSweep sweep(rejects, encoded.bytes(), CapsetInfo::encoded_size);
  sweep.run();
  CHECK(sweep.cases_run() > CapsetInfo::encoded_size * 8u);
  CHECK(rejects(nullptr, 0u));
  CHECK(rejects(nullptr, CapsetInfo::encoded_size)); }
