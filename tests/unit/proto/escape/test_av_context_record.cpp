/* The private-data record a process puts on D3DKMTCreateContextVirtual to ask
 * for an av context. The kernel side decodes it from memory a process wrote,
 * so every field is judged as hostile input, the way ContextSetup judges the
 * wire (feature 043). */
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/escape/av_context_record.hpp"
#include "proto/context/sub_api.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

using av::AvContextRecord;
using av::Status;
using av::SubApi;

namespace {

constexpr std::uint32_t reply_bytes = 0x10000u;

/* Named by allocation 2, with a 64 KiB reply slot, an order the wire allows,
 * one sub-API. */
AvContextRecord sample()
{
    return AvContextRecord(SubApi::vulkan, 16u, 2u, reply_bytes);
}

class Encoded {
public:
    explicit Encoded(const AvContextRecord &record) { status_ = record.encode(bytes_, sizeof bytes_); }
    std::uint8_t *bytes() { return bytes_; }
    Status        status() const { return status_; }

private:
    std::uint8_t bytes_[AvContextRecord::encoded_size] = {};
    Status       status_ = Status::ok;
};

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    AvContextRecord record;
    return record.decode(bytes, length) != Status::ok;
}

/* Encode a record the constructor accepts but the wire rules forbid, then check
 * decode refuses it - the caller side does not validate, the kernel side does. */
bool rejects_encoded(const AvContextRecord &record)
{
    std::uint8_t bytes[AvContextRecord::encoded_size] = {};
    if (record.encode(bytes, sizeof bytes) != Status::ok) return true;
    return rejects(bytes, sizeof bytes);
}

}  // namespace

TEST(av_context_record, a_well_formed_record_round_trips)
{ Encoded encoded(sample());
  CHECK_STATUS(encoded.status(), Status::ok);
  AvContextRecord decoded;
  CHECK_STATUS(decoded.decode(encoded.bytes(), AvContextRecord::encoded_size), Status::ok);
  CHECK_EQ(decoded.sub_api(), SubApi::vulkan);
  CHECK_EQ(decoded.ring_order(), std::uint8_t{16u});
  CHECK_EQ(decoded.ring(), 2u);
  CHECK_EQ(decoded.reply_bytes(), reply_bytes);
  /* Where the contract puts the name and the size. */
  CHECK_EQ(encoded.bytes()[12], 2u);
  CHECK_EQ(encoded.bytes()[18], 1u);
  std::uint8_t again[AvContextRecord::encoded_size] = {};
  CHECK_STATUS(decoded.encode(again, sizeof again), Status::ok);
  CHECK(std::memcmp(again, encoded.bytes(), sizeof again) == 0); }

/* Each known sub-API decodes: OpenGL, OpenCL and Direct3D as well as Vulkan. */
TEST(av_context_record, every_known_sub_api_decodes)
{ for (SubApi api : {SubApi::vulkan, SubApi::opengl, SubApi::opencl, SubApi::d3d}) {
      AvContextRecord record(api, 16u, 2u, reply_bytes);
      Encoded encoded(record);
      CHECK_STATUS(encoded.status(), Status::ok);
      AvContextRecord decoded;
      CHECK_STATUS(decoded.decode(encoded.bytes(), AvContextRecord::encoded_size), Status::ok);
      CHECK(decoded.sub_api() == api);
  } }

/* The reply slot is locked guest RAM for the context's life: at least a byte
 * - every context reads answers - and no more than the protocol's bound. */
TEST(av_context_record, a_reply_slot_is_sized_within_its_bounds)
{ for (std::uint32_t bytes : {1u, 4096u, AvContextRecord::max_reply_bytes}) {
      Encoded encoded(AvContextRecord(SubApi::opencl, 12u, 2u, bytes));
      AvContextRecord decoded;
      CHECK_STATUS(decoded.decode(encoded.bytes(), AvContextRecord::encoded_size), Status::ok);
      CHECK_EQ(decoded.reply_bytes(), bytes);
  }
  CHECK(rejects_encoded(AvContextRecord(SubApi::opencl, 12u, 2u, 0u)));
  CHECK(rejects_encoded(AvContextRecord(SubApi::opencl, 12u, 2u, AvContextRecord::max_reply_bytes + 1u)));
  CHECK(rejects_encoded(AvContextRecord(SubApi::opencl, 12u, 2u, UINT32_MAX))); }

TEST(av_context_record, a_short_record_is_refused)
{ Encoded encoded(sample());
  for (std::size_t length = 0u; length < AvContextRecord::encoded_size; length++)
      CHECK(rejects(encoded.bytes(), length)); }

TEST(av_context_record, a_bad_magic_is_refused)
{ Encoded encoded(sample());
  encoded.bytes()[0] ^= 0xFFu;
  CHECK(rejects(encoded.bytes(), AvContextRecord::encoded_size)); }

/* Exactly one sub-API bit; zero, two, or an undefined value is refused. The
 * sub_api field is the u32 at offset 4, past the magic. */
TEST(av_context_record, a_sub_api_that_is_not_one_known_bit_is_refused)
{ for (std::uint32_t api : {0u, 3u, 16u, 7u}) {
      std::uint8_t bytes[AvContextRecord::encoded_size] = {};
      CHECK_STATUS(sample().encode(bytes, sizeof bytes), Status::ok);
      std::memcpy(bytes + 4u, &api, sizeof api);
      CHECK(rejects(bytes, sizeof bytes));
  } }

/* The wire's ring order range, 12..24. */
TEST(av_context_record, an_order_outside_the_wire_range_is_refused)
{ for (std::uint8_t order : {std::uint8_t{0u}, std::uint8_t{11u}, std::uint8_t{25u}, std::uint8_t{255u}}) {
      std::uint8_t bytes[AvContextRecord::encoded_size] = {};
      CHECK_STATUS(AvContextRecord(SubApi::vulkan, 16u, 2u, reply_bytes).encode(bytes, sizeof bytes),
                   Status::ok);
      bytes[8] = order;
      CHECK(rejects(bytes, sizeof bytes));
  } }

/* A context with no name is one no driver can resolve. */
TEST(av_context_record, a_context_named_by_no_allocation_is_refused)
{ CHECK(rejects_encoded(AvContextRecord(SubApi::vulkan, 16u, 0u, reply_bytes)));
  CHECK(!rejects_encoded(AvContextRecord(SubApi::vulkan, 16u, 1u, reply_bytes))); }

/* The padding after the order and the word after the reply size. */
TEST(av_context_record, a_reserved_byte_that_is_not_zero_is_refused)
{ for (std::size_t at : {std::size_t{9u}, std::size_t{11u}, std::size_t{20u}, std::size_t{23u}}) {
      std::uint8_t bytes[AvContextRecord::encoded_size] = {};
      CHECK_STATUS(sample().encode(bytes, sizeof bytes), Status::ok);
      bytes[at] = 1u;
      CHECK(rejects(bytes, sizeof bytes));
  } }

/* ---- an OpenGL context's options, in the reserved tail (feature 050) ---- */

namespace {

using av::GlContextOptions;
using Profile = av::GlContextOptions::Profile;

constexpr std::uint32_t share_handle = 0x40000242u;

AvContextRecord gl_record(const GlContextOptions &options)
{
    return AvContextRecord(SubApi::opengl, 20u, 2u, reply_bytes, options);
}

/* The options' eight bytes at offset 24 of an OpenGL record, `tail` given. */
bool rejects_tail(SubApi api, const std::uint8_t tail[8])
{
    std::uint8_t bytes[AvContextRecord::encoded_size] = {};
    if (AvContextRecord(api, 20u, 2u, reply_bytes).encode(bytes, sizeof bytes) != Status::ok) return false;
    std::memcpy(bytes + 24u, tail, 8u);
    return rejects(bytes, sizeof bytes);
}

}  // namespace

TEST(av_context_record, gl_options_round_trip_in_the_reserved_tail)
{ const GlContextOptions options(4u, 6u, Profile::core,
                                 GlContextOptions::debug | GlContextOptions::lose_context_on_reset, share_handle);
  Encoded encoded(gl_record(options));
  CHECK_STATUS(encoded.status(), Status::ok);
  /* Where the contract puts each field. */
  const std::uint8_t expected_tail[8] = {4u, 6u, 1u, 0x09u, 0x42u, 0x02u, 0x00u, 0x40u};
  CHECK(std::memcmp(encoded.bytes() + 24u, expected_tail, sizeof expected_tail) == 0);
  AvContextRecord decoded;
  CHECK_STATUS(decoded.decode(encoded.bytes(), AvContextRecord::encoded_size), Status::ok);
  const GlContextOptions got = decoded.options();
  CHECK_EQ(got.major(), std::uint8_t{4u});
  CHECK_EQ(got.minor(), std::uint8_t{6u});
  CHECK(got.profile() == Profile::core);
  CHECK_EQ(got.flags(), std::uint8_t{0x09u});
  CHECK_EQ(got.share_context(), share_handle);
  CHECK(!got.defaults());
  CHECK(got.valid());
  std::uint8_t again[AvContextRecord::encoded_size] = {};
  CHECK_STATUS(decoded.encode(again, sizeof again), Status::ok);
  CHECK(std::memcmp(again, encoded.bytes(), sizeof again) == 0); }

/* All eight zero is 043's record: the host's default context, no sharing. */
TEST(av_context_record, an_all_zero_tail_is_the_default_options)
{ for (SubApi api : {SubApi::vulkan, SubApi::opengl, SubApi::opencl}) {
      Encoded encoded(AvContextRecord(api, 20u, 2u, reply_bytes));
      CHECK(std::memcmp(encoded.bytes() + 24u, "\0\0\0\0\0\0\0\0", 8u) == 0);
      AvContextRecord decoded;
      CHECK_STATUS(decoded.decode(encoded.bytes(), AvContextRecord::encoded_size), Status::ok);
      CHECK(decoded.options().defaults());
      CHECK_EQ(decoded.options().major(), std::uint8_t{0u});
      CHECK_EQ(decoded.options().share_context(), 0u);
  }
  CHECK(GlContextOptions().defaults());
  CHECK(GlContextOptions().valid()); }

/* Every version a major has, and each profile and flag, is accepted. */
TEST(av_context_record, every_gl_version_profile_and_flag_the_contract_names_decodes)
{ struct Version { std::uint8_t major; std::uint8_t highest_minor; };
  const Version versions[] = {{1u, 5u}, {2u, 1u}, {3u, 3u}, {4u, 6u}};
  for (const Version &version : versions)
      for (std::uint8_t minor = 0u; minor <= version.highest_minor; minor++) {
          AvContextRecord decoded;
          Encoded encoded(gl_record(GlContextOptions(version.major, minor, Profile::none, 0u, 0u)));
          CHECK_STATUS(decoded.decode(encoded.bytes(), AvContextRecord::encoded_size), Status::ok);
          CHECK_EQ(decoded.options().minor(), minor);
      }
  for (Profile profile : {Profile::none, Profile::core, Profile::compatibility})
      for (std::uint8_t flags = 0u; flags < 16u; flags++) {
          AvContextRecord decoded;
          Encoded encoded(gl_record(GlContextOptions(3u, 3u, profile, flags, share_handle)));
          CHECK_STATUS(decoded.decode(encoded.bytes(), AvContextRecord::encoded_size), Status::ok);
          CHECK(decoded.options().profile() == profile);
          CHECK_EQ(decoded.options().flags(), flags);
      } }

TEST(av_context_record, gl_options_the_contract_forbids_are_refused)
{ struct Case { const char *what; std::uint8_t tail[8]; };
  const Case cases[] = {
      {"major 5", {5u, 0u, 1u, 0u, 0u, 0u, 0u, 0u}},
      {"major 255", {255u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}},
      {"4.7", {4u, 7u, 1u, 0u, 0u, 0u, 0u, 0u}},
      {"3.4", {3u, 4u, 1u, 0u, 0u, 0u, 0u, 0u}},
      {"2.2", {2u, 2u, 0u, 0u, 0u, 0u, 0u, 0u}},
      {"1.6", {1u, 6u, 0u, 0u, 0u, 0u, 0u, 0u}},
      {"profile 3", {4u, 6u, 3u, 0u, 0u, 0u, 0u, 0u}},
      {"profile 255", {4u, 6u, 255u, 0u, 0u, 0u, 0u, 0u}},
      {"flag bit 4", {4u, 6u, 1u, 0x10u, 0u, 0u, 0u, 0u}},
      {"flag bit 7", {4u, 6u, 1u, 0x80u, 0u, 0u, 0u, 0u}},
      {"major 0 with a minor", {0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u}},
      {"major 0 with a profile", {0u, 0u, 1u, 0u, 0u, 0u, 0u, 0u}},
      {"major 0 with a flag", {0u, 0u, 0u, 1u, 0u, 0u, 0u, 0u}},
      {"major 0 with a share", {0u, 0u, 0u, 0u, 1u, 0u, 0u, 0u}},
  };
  for (const Case &one : cases) {
      CHECK(rejects_tail(SubApi::opengl, one.tail));
      CHECK(!GlContextOptions(one.tail[0], one.tail[1], static_cast<Profile>(one.tail[2]), one.tail[3],
                              one.tail[4] | (std::uint32_t{one.tail[5]} << 8u))
                 .valid());
  } }

/* Options belong to OpenGL. An OpenCL record carries one - the GL context
 * whose objects it shares, the tail's last four bytes (cl_khr_gl_sharing) -
 * and any other non-zero byte, on it or on a Vulkan record, is refused, even
 * one an OpenGL record may carry. */
TEST(av_context_record, an_option_on_a_vulkan_record_or_an_opencl_one_past_its_share_is_refused)
{ for (std::size_t at = 0u; at < 8u; at++) {
      std::uint8_t tail[8] = {};
      tail[at] = 1u;
      CHECK(rejects_tail(SubApi::vulkan, tail));
      CHECK_EQ(rejects_tail(SubApi::opencl, tail), at < 4u);
  }
  const std::uint8_t valid_for_gl[8] = {4u, 6u, 1u, 0u, 0u, 0u, 0u, 0u};
  CHECK(!rejects_tail(SubApi::opengl, valid_for_gl));
  CHECK(rejects_tail(SubApi::vulkan, valid_for_gl)); }

/* Any byte of a valid OpenGL record flipped either decodes to a record whose
 * encoding is exactly those bytes, or is refused. */
TEST(av_context_record, a_byte_flip_sweep_decodes_exactly_or_is_refused)
{ Encoded encoded(gl_record(GlContextOptions(3u, 3u, Profile::compatibility, GlContextOptions::robust_access,
                                              share_handle)));
  std::uint32_t refused = 0u;
  for (std::size_t at = 0u; at < AvContextRecord::encoded_size; at++)
      for (std::uint32_t bit = 0u; bit < 8u; bit++) {
          std::uint8_t bytes[AvContextRecord::encoded_size] = {};
          std::memcpy(bytes, encoded.bytes(), sizeof bytes);
          bytes[at] = static_cast<std::uint8_t>(bytes[at] ^ (1u << bit));
          AvContextRecord decoded;
          if (decoded.decode(bytes, sizeof bytes) != Status::ok) {
              refused++;
              continue;
          }
          std::uint8_t again[AvContextRecord::encoded_size] = {};
          CHECK_STATUS(decoded.encode(again, sizeof again), Status::ok);
          CHECK(std::memcmp(again, bytes, sizeof again) == 0);
      }
  /* The magic's 32 bits, the reserved padding's 24 and word's 32, and the
   * flags' high 4 at least. */
  CHECK(refused >= 92u); }
