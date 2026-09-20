#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

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
    return ContextSetup(ProtocolVersion(1u, 0u), SubApi::opengl, 17u, 16u, 18u, 5u, 19u);
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
        if (setup.size() != length || setup.encode(again, sizeof again) != Status::ok ||
            std::memcmp(again, bytes, setup.size()) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}  // namespace

TEST(context_setup_encode, places_every_field_at_its_offset)
{ Encoded encoded(sample());
  CHECK_STATUS(encoded.status(), Status::ok);
  const ByteLayout layout(encoded.bytes(), ContextSetup::base_size);
  CHECK(layout.at(0u, 4u) == 0x434B5652u);
  CHECK(layout.at(4u, 2u) == 1u);
  CHECK(layout.at(6u, 2u) == 0u);
  CHECK(layout.at(8u, 4u) == 2u);
  CHECK(layout.at(12u, 4u) == 17u);
  CHECK(layout.at(16u, 1u) == 16u);
  CHECK(layout.at(17u, 3u) == 0u);
  CHECK(layout.at(20u, 4u) == 18u);
  CHECK(layout.at(24u, 4u) == 5u);
  CHECK(layout.at(28u, 4u) == 19u); }

TEST(context_setup, round_trips_through_its_encoding)
{ Encoded encoded(sample());
  ContextSetup decoded;
  CHECK_STATUS(decoded.decode(encoded.bytes(), ContextSetup::base_size), Status::ok);
  CHECK(decoded.sub_api() == SubApi::opengl);
  CHECK_EQ(decoded.ring_resource(), 17u);
  CHECK_EQ(decoded.ring_order(), 16u);
  CHECK_EQ(decoded.fence_resource(), 18u);
  CHECK_EQ(decoded.doorbell(), 5u);
  CHECK_EQ(decoded.reply_resource(), 19u);
  CHECK_EQ(decoded.version().major_version(), 1u);
  Encoded again(decoded);
  CHECK(std::memcmp(again.bytes(), encoded.bytes(), ContextSetup::base_size) == 0); }

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
      {28u, 17u},    /* reply resource equals ring resource */
      {28u, 18u},    /* reply resource equals fence resource */
  };
  for (const Case &c : cases) {
      Encoded encoded(sample());
      encoded.bytes()[c.offset] = c.value;
      ContextSetup decoded;
      CHECK_STATUS(decoded.decode(encoded.bytes(), ContextSetup::base_size), Status::malformed);
  } }

TEST(context_setup_decode, a_zero_fence_resource_is_malformed)
{ Encoded encoded(ContextSetup(ProtocolVersion(1u, 0u), SubApi::vulkan, 5u, 12u, 0u, 0u, 0u));
  ContextSetup decoded;
  CHECK_STATUS(decoded.decode(encoded.bytes(), ContextSetup::base_size), Status::malformed); }

TEST(context_setup_decode, no_reply_resource_is_accepted)
{ Encoded encoded(ContextSetup(ProtocolVersion(1u, 0u), SubApi::vulkan, 5u, 12u, 6u, 0u, 0u));
  ContextSetup decoded;
  CHECK_STATUS(decoded.decode(encoded.bytes(), ContextSetup::base_size), Status::ok);
  CHECK_EQ(decoded.reply_resource(), 0u); }

TEST(context_setup_decode, a_rejected_setup_leaves_the_destination_unchanged)
{ Encoded encoded(sample());
  ContextSetup decoded;
  CHECK_STATUS(decoded.decode(encoded.bytes(), ContextSetup::base_size), Status::ok);
  encoded.bytes()[0] = 0u;
  CHECK_STATUS(decoded.decode(encoded.bytes(), ContextSetup::base_size), Status::malformed);
  CHECK_EQ(decoded.ring_resource(), 17u); }

TEST(context_setup_validate_against, accepts_what_the_capset_offers)
{ CHECK_STATUS(sample().validate_against(offering(0x2u)), Status::ok); }

TEST(context_setup_validate_against, rejects_a_different_major)
{ const ContextSetup setup(ProtocolVersion(2u, 0u), SubApi::opengl, 17u, 16u, 18u, 0u, 0u);
  CHECK_STATUS(setup.validate_against(offering(0x7u)), Status::not_supported); }

TEST(context_setup_validate_against, rejects_a_sub_api_the_capset_lacks)
{ CHECK_STATUS(sample().validate_against(offering(0x5u)), Status::not_supported); }

TEST(context_setup_validate_against, rejects_a_ring_order_outside_the_capset_range)
{ const ContextSetup setup(ProtocolVersion(1u, 0u), SubApi::opengl, 17u, 22u, 18u, 0u, 0u);
  CHECK_STATUS(setup.validate_against(offering(0x7u)), Status::not_supported); }

/* One doorbell page per context the capset allows: offering() allows 8. */
TEST(context_setup_validate_against, accepts_the_last_doorbell_the_capset_allows)
{ const ContextSetup setup(ProtocolVersion(1u, 0u), SubApi::opengl, 17u, 16u, 18u, 7u, 0u);
  CHECK_STATUS(setup.validate_against(offering(0x2u)), Status::ok); }

TEST(context_setup_validate_against, rejects_a_doorbell_past_the_capset_contexts)
{ const ContextSetup at_count(ProtocolVersion(1u, 0u), SubApi::opengl, 17u, 16u, 18u, 8u, 0u);
  const ContextSetup far(ProtocolVersion(1u, 0u), SubApi::opengl, 17u, 16u, 18u, 0xFFFFFFFFu, 0u);
  CHECK_STATUS(at_count.validate_against(offering(0x2u)), Status::not_supported);
  CHECK_STATUS(far.validate_against(offering(0x2u)), Status::not_supported); }

TEST(context_setup_encode, a_short_destination_is_too_large_and_untouched)
{ GuardedBuffer buffer(ContextSetup::base_size - 1u);
  CHECK_STATUS(sample().encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact()); }

TEST(context_setup_decode, survives_the_hostile_sweep)
{ Encoded encoded(sample());
  const av::HostileSweep sweep(rejects, encoded.bytes(), ContextSetup::base_size);
  sweep.run();
  CHECK(sweep.cases_run() > ContextSetup::base_size * 8u);
  CHECK(rejects(nullptr, ContextSetup::encoded_size));
  /* A minor-0 setup is judged whole: the major it was written by is the only
   * field no reader checks (16 bits), and the doorbell is judged against the
   * capset rather than the bytes (32). Any flip of the minor asks for a
   * 48-byte setup in 32 bytes, and no flip of the sub-API reaches another one,
   * since 1, 2 and 4 are a bit apart each. The three resource ids must differ
   * and none may be zero: 31 flips of the ring survive (not the one making it
   * the reply's 19), 31 of the fence (the same), and 30 of the reply (not the
   * two making it the ring's 17 or the fence's 18); four flips of ring order
   * 16 stay inside [12, 24]. Every shorter input is a length its version does
   * not give, all-zero and all-ones carry no magic, and the untouched setup is
   * the last acceptance. */
  const std::uint32_t acceptable = 16u + 32u + 31u + 31u + 30u + 4u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }

TEST(context_setup_decode, accepts_each_sub_api)
{ const SubApi apis[] = {SubApi::vulkan, SubApi::opengl, SubApi::opencl};
  for (const SubApi api : apis) {
      Encoded encoded(ContextSetup(ProtocolVersion(1u, 0u), api, 3u, 12u, 4u, 63u, 9u));
      ContextSetup decoded;
      CHECK_STATUS(decoded.decode(encoded.bytes(), ContextSetup::base_size), Status::ok);
      CHECK(decoded.sub_api() == api);
  } }

/* ---- the options a GL context is made with, under minor 1 (feature 050) ---- */

namespace {

using av::GlContextOptions;
using Profile = av::GlContextOptions::Profile;

constexpr std::uint32_t share_device_id = 0x2Au;

ContextSetup with_options(const GlContextOptions &options, SubApi api = SubApi::opengl)
{
    return ContextSetup(ProtocolVersion(1u, ContextSetup::options_minor), api, 17u, 16u, 18u, 5u, 19u, options);
}

const GlContextOptions core_46(4u, 6u, Profile::core, GlContextOptions::debug, share_device_id);

}  // namespace

TEST(context_setup, the_next_minor_carries_the_options_and_the_share_contexts_device_id)
{ CHECK_EQ(ProtocolVersion::current_minor, ContextSetup::options_minor);
  const ContextSetup setup = with_options(core_46);
  CHECK_EQ(setup.size(), 48u);
  Encoded encoded(setup);
  CHECK_STATUS(encoded.status(), Status::ok);
  const ByteLayout layout(encoded.bytes(), ContextSetup::encoded_size);
  CHECK(layout.at(6u, 2u) == 1u);
  CHECK(layout.at(28u, 4u) == 19u);
  CHECK(layout.at(32u, 1u) == 4u);
  CHECK(layout.at(33u, 1u) == 6u);
  CHECK(layout.at(34u, 1u) == 1u);
  CHECK(layout.at(35u, 1u) == 1u);
  CHECK(layout.at(36u, 4u) == share_device_id);
  CHECK(layout.at(40u, 8u) == 0u);
  ContextSetup decoded;
  CHECK_STATUS(decoded.decode(encoded.bytes(), 48u), Status::ok);
  CHECK_EQ(decoded.size(), 48u);
  CHECK_EQ(decoded.options().major(), std::uint8_t{4u});
  CHECK_EQ(decoded.options().minor(), std::uint8_t{6u});
  CHECK(decoded.options().profile() == Profile::core);
  CHECK_EQ(decoded.options().flags(), GlContextOptions::debug);
  CHECK_EQ(decoded.options().share_context(), share_device_id);
  CHECK_EQ(decoded.reply_resource(), 19u);
  Encoded again(decoded);
  CHECK(std::memcmp(again.bytes(), encoded.bytes(), 48u) == 0); }

/* A minor-0 setup is 32 bytes, means the default options, and cannot carry
 * any other; every sub-API may use the new minor with the defaults. */
TEST(context_setup, the_previous_minor_is_32_bytes_of_default_options)
{ const ContextSetup old(ProtocolVersion(1u, 0u), SubApi::opengl, 17u, 16u, 18u, 5u, 19u);
  CHECK_EQ(old.size(), 32u);
  CHECK(old.options().defaults());
  Encoded encoded(old);
  ContextSetup decoded;
  CHECK_STATUS(decoded.decode(encoded.bytes(), 32u), Status::ok);
  CHECK(decoded.options().defaults());
  CHECK_EQ(decoded.size(), 32u);
  const ContextSetup old_with_options(ProtocolVersion(1u, 0u), SubApi::opengl, 17u, 16u, 18u, 5u, 19u, core_46);
  CHECK_STATUS(old_with_options.encode(Encoded(old).bytes(), ContextSetup::encoded_size), Status::invalid_parameter);
  for (SubApi api : {SubApi::vulkan, SubApi::opengl, SubApi::opencl}) {
      Encoded plain(with_options(GlContextOptions(), api));
      ContextSetup read;
      CHECK_STATUS(read.decode(plain.bytes(), 48u), Status::ok);
      CHECK(read.options().defaults());
  } }

TEST(context_setup_decode, a_length_that_is_not_its_minors_is_malformed)
{ Encoded current(with_options(core_46));
  Encoded old(sample());
  ContextSetup decoded;
  CHECK_STATUS(decoded.decode(current.bytes(), 32u), Status::malformed);
  CHECK_STATUS(decoded.decode(current.bytes(), 47u), Status::malformed);
  CHECK_STATUS(decoded.decode(old.bytes(), 48u), Status::malformed);
  CHECK_STATUS(decoded.decode(old.bytes(), 33u), Status::malformed);
  /* The previous minor's setup relabelled as the new one is too short. */
  std::uint8_t relabelled[ContextSetup::encoded_size] = {};
  std::memcpy(relabelled, old.bytes(), 32u);
  relabelled[6] = 1u;
  CHECK(decoded.decode(relabelled, 32u) != Status::ok);
  /* Nothing was taken from any of them. */
  CHECK_EQ(decoded.ring_resource(), 0u); }

TEST(context_setup_decode, options_the_contract_forbids_are_malformed)
{ struct Case { std::size_t offset; std::uint8_t value; };
  const Case cases[] = {
      {32u, 5u},    /* major 5 */
      {33u, 7u},    /* 4.7 */
      {34u, 3u},    /* profile 3 */
      {35u, 0x11u}, /* an undefined flag */
      {40u, 1u},    /* reserved */
      {47u, 0x80u}, /* reserved */
  };
  for (const Case &c : cases) {
      Encoded encoded(with_options(core_46));
      encoded.bytes()[c.offset] = c.value;
      ContextSetup decoded;
      CHECK_STATUS(decoded.decode(encoded.bytes(), 48u), Status::malformed);
  }
  for (SubApi api : {SubApi::vulkan, SubApi::opencl}) {
      Encoded encoded(with_options(GlContextOptions(), api));
      encoded.bytes()[36] = 1u;   /* a share context on a context that shares nothing */
      ContextSetup decoded;
      CHECK_STATUS(decoded.decode(encoded.bytes(), 48u), Status::malformed);
      CHECK_STATUS(with_options(core_46, api).encode(encoded.bytes(), ContextSetup::encoded_size),
                   Status::invalid_parameter);
  } }

TEST(context_setup_decode, a_setup_with_options_survives_the_hostile_sweep)
{ Encoded encoded(with_options(core_46));
  const av::HostileSweep sweep(rejects, encoded.bytes(), 48u);
  sweep.run();
  CHECK(sweep.cases_run() > 48u * 8u);
  /* The base fields are judged as they are without options - 16 free bits of
   * major, 32 of doorbell, 31 + 31 + 30 of the three distinct resource ids and
   * 4 of the ring order - and 15 flips of the minor still ask for a 48-byte
   * setup, only the one clearing it to 0 asking for 32. The options are judged
   * against the versions OpenGL has: no flip of major 4 leaves a version that
   * exists, two flips of minor 6 stay at or under 4.6's ceiling, clearing the
   * core profile to "none" is the one profile flip that is defined, and four
   * flips of the flags stay inside the four WGL flags. The share context is
   * the context's own business, 32 bits. The eight trailing bytes must be
   * zero. Every shorter input is short of its version's 48, all-zero and
   * all-ones carry no magic, and the untouched setup is the last acceptance. */
  const std::uint32_t acceptable =
      16u + 32u + 31u + 31u + 30u + 4u + 15u + 2u + 1u + 4u + 32u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }

TEST(context_setup_encode, a_setup_with_options_needs_48_bytes)
{ GuardedBuffer buffer(47u);
  CHECK_STATUS(with_options(core_46).encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact()); }


/* What a setup can carry is judged before it is written: options only at the
 * options minor, and only for OpenGL, and only valid ones. */
TEST(context_setup, options_fit_only_a_gl_setup_of_the_options_minor)
{ CHECK(with_options(core_46).options_fit());
  CHECK(with_options(GlContextOptions()).options_fit());
  CHECK(with_options(GlContextOptions(), SubApi::vulkan).options_fit());
  CHECK(!with_options(core_46, SubApi::vulkan).options_fit());
  CHECK(!with_options(core_46, SubApi::opencl).options_fit());
  CHECK(!with_options(GlContextOptions(4u, 7u, Profile::core, 0u, 0u)).options_fit());
  CHECK(sample().options_fit());
  CHECK(!ContextSetup(ProtocolVersion(1u, 0u), SubApi::opengl, 17u, 16u, 18u, 5u, 19u, core_46).options_fit()); }
