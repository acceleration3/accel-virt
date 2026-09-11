#include "../../src/core/status.hpp"
#include "../../src/proto/capset_info.hpp"
#include "../../src/proto/context_setup.hpp"
#include "../../src/proto/gpu_identity.hpp"
#include "../../src/proto/protocol_version.hpp"
#include "../../src/proto/sub_api.hpp"
#include "../harness/byte_layout.hpp"
#include "../harness/guarded_buffer.hpp"
#include "../harness/hostile_sweep.hpp"
#include "../harness/test_registry.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ByteLayout;
using av::CapsetInfo;
using av::ContextSetup;
using av::GpuIdentity;
using av::GuardedBuffer;
using av::ProtocolVersion;
using av::Status;
using av::SubApi;

namespace {

ContextSetup sample()
{
    return ContextSetup(ProtocolVersion(1u, 0u), SubApi::opengl, 17u, 16u, 18u);
}

CapsetInfo offering(std::uint32_t sub_apis)
{
    return CapsetInfo(ProtocolVersion(1u, 2u), sub_apis, 8u, 12u, 20u, 0u, GpuIdentity());
}

class Encoded {
public:
    explicit Encoded(const ContextSetup &setup) { status_ = setup.encode(bytes_, sizeof bytes_); }
    std::uint8_t *bytes() { return bytes_; }
    Status status() const { return status_; }

private:
    std::uint8_t bytes_[ContextSetup::encoded_size] = {};
    Status       status_ = Status::ok;
};

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    ContextSetup setup;
    const Status status = setup.decode(bytes, length);
    if (status == Status::ok) {
        std::uint8_t again[ContextSetup::encoded_size] = {};
        if (setup.encode(again, sizeof again) != Status::ok || std::memcmp(again, bytes, sizeof again) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}  // namespace

TEST(context_setup_encode, places_every_field_at_its_offset)
{ Encoded encoded(sample());
  CHECK_STATUS(encoded.status(), Status::ok);
  const ByteLayout layout(encoded.bytes(), ContextSetup::encoded_size);
  CHECK(layout.at(0u, 4u) == 0x434B5652u);
  CHECK(layout.at(4u, 2u) == 1u);
  CHECK(layout.at(6u, 2u) == 0u);
  CHECK(layout.at(8u, 4u) == 2u);
  CHECK(layout.at(12u, 4u) == 17u);
  CHECK(layout.at(16u, 1u) == 16u);
  CHECK(layout.at(17u, 3u) == 0u);
  CHECK(layout.at(20u, 4u) == 18u);
  CHECK(layout.at(24u, 8u) == 0u); }

TEST(context_setup, round_trips_through_its_encoding)
{ Encoded encoded(sample());
  ContextSetup decoded;
  CHECK_STATUS(decoded.decode(encoded.bytes(), ContextSetup::encoded_size), Status::ok);
  CHECK(decoded.sub_api() == SubApi::opengl);
  CHECK_EQ(decoded.ring_resource(), 17u);
  CHECK_EQ(decoded.ring_order(), 16u);
  CHECK_EQ(decoded.fence_resource(), 18u);
  CHECK_EQ(decoded.version().major_version(), 1u);
  Encoded again(decoded);
  CHECK(std::memcmp(again.bytes(), encoded.bytes(), ContextSetup::encoded_size) == 0); }

TEST(context_setup_decode, rejects_each_malformed_field)
{ struct Case { std::size_t offset; std::uint8_t value; };
  const Case cases[] = {
      {0u, 0u},      /* magic */
      {8u, 0u},      /* no sub-API */
      {8u, 3u},      /* two sub-APIs */
      {8u, 8u},      /* unknown sub-API */
      {12u, 0u},     /* ring resource 0 (low byte of 17) */
      {16u, 11u},    /* order below 12 */
      {16u, 25u},    /* order above 24 */
      {18u, 1u},     /* reserved */
      {20u, 17u},    /* fence resource equals ring resource */
      {29u, 1u},     /* reserved u64 */
  };
  for (const Case &c : cases) {
      Encoded encoded(sample());
      encoded.bytes()[c.offset] = c.value;
      ContextSetup decoded;
      CHECK_STATUS(decoded.decode(encoded.bytes(), ContextSetup::encoded_size), Status::malformed);
  } }

TEST(context_setup_decode, a_zero_fence_resource_is_malformed)
{ Encoded encoded(ContextSetup(ProtocolVersion(1u, 0u), SubApi::vulkan, 5u, 12u, 0u));
  ContextSetup decoded;
  CHECK_STATUS(decoded.decode(encoded.bytes(), ContextSetup::encoded_size), Status::malformed); }

TEST(context_setup_decode, a_rejected_setup_leaves_the_destination_unchanged)
{ Encoded encoded(sample());
  ContextSetup decoded;
  CHECK_STATUS(decoded.decode(encoded.bytes(), ContextSetup::encoded_size), Status::ok);
  encoded.bytes()[0] = 0u;
  CHECK_STATUS(decoded.decode(encoded.bytes(), ContextSetup::encoded_size), Status::malformed);
  CHECK_EQ(decoded.ring_resource(), 17u); }

TEST(context_setup_validate_against, accepts_what_the_capset_offers)
{ CHECK_STATUS(sample().validate_against(offering(0x2u)), Status::ok); }

TEST(context_setup_validate_against, rejects_a_different_major)
{ const ContextSetup setup(ProtocolVersion(2u, 0u), SubApi::opengl, 17u, 16u, 18u);
  CHECK_STATUS(setup.validate_against(offering(0x7u)), Status::not_supported); }

TEST(context_setup_validate_against, rejects_a_sub_api_the_capset_lacks)
{ CHECK_STATUS(sample().validate_against(offering(0x5u)), Status::not_supported); }

TEST(context_setup_validate_against, rejects_a_ring_order_outside_the_capset_range)
{ const ContextSetup setup(ProtocolVersion(1u, 0u), SubApi::opengl, 17u, 22u, 18u);
  CHECK_STATUS(setup.validate_against(offering(0x7u)), Status::not_supported); }

TEST(context_setup_encode, a_short_destination_is_too_large_and_untouched)
{ GuardedBuffer buffer(ContextSetup::encoded_size - 1u);
  CHECK_STATUS(sample().encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact()); }

TEST(context_setup_decode, survives_the_hostile_sweep)
{ Encoded encoded(sample());
  const av::HostileSweep sweep(rejects, encoded.bytes(), ContextSetup::encoded_size);
  sweep.run();
  CHECK(sweep.cases_run() > ContextSetup::encoded_size * 8u);
  CHECK(rejects(nullptr, ContextSetup::encoded_size)); }

TEST(context_setup_decode, accepts_each_sub_api)
{ const SubApi apis[] = {SubApi::vulkan, SubApi::opengl, SubApi::opencl};
  for (const SubApi api : apis) {
      Encoded encoded(ContextSetup(ProtocolVersion(1u, 0u), api, 3u, 12u, 4u));
      ContextSetup decoded;
      CHECK_STATUS(decoded.decode(encoded.bytes(), ContextSetup::encoded_size), Status::ok);
      CHECK(decoded.sub_api() == api);
  } }
