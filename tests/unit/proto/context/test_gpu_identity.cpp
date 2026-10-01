#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>

using av::ByteLayout;
using av::GpuIdentity;
using av::Status;
using av::WireReader;
using av::WireWriter;

namespace {

const std::uint8_t uuid[GpuIdentity::uuid_bytes] = {
    0x5f, 0x3c, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 0xe0, 0xe1};

GpuIdentity sample()
{
    return GpuIdentity(0x10DEu, 0x2684u, GpuIdentity::discrete_gpu, uuid,
                       24ull << 30, 64ull << 30, "NVIDIA GeForce RTX 4090");
}

class Encoded {
public:
    explicit Encoded(const GpuIdentity &identity)
    {
        WireWriter writer(bytes_, sizeof bytes_);
        identity.encode(writer);
        status_ = writer.status();
    }
    std::uint8_t *bytes() { return bytes_; }
    Status status() const { return status_; }

private:
    std::uint8_t bytes_[GpuIdentity::encoded_size] = {};
    Status       status_ = Status::ok;
};

Status decode(GpuIdentity &identity, const std::uint8_t *bytes)
{
    WireReader reader(bytes, GpuIdentity::encoded_size);
    return identity.decode(reader);
}

}  // namespace

TEST(gpu_identity_encode, places_every_field_at_its_offset)
{ Encoded encoded(sample());
  CHECK_STATUS(encoded.status(), Status::ok);
  const ByteLayout layout(encoded.bytes(), GpuIdentity::encoded_size);
  CHECK(layout.at(0u, 4u) == 0x10DEu);
  CHECK(layout.at(4u, 4u) == 0x2684u);
  CHECK(layout.at(8u, 4u) == 2u);
  CHECK(layout.at(12u, 4u) == 0u);
  CHECK(std::memcmp(encoded.bytes() + 16u, uuid, sizeof uuid) == 0);
  CHECK(layout.at(32u, 8u) == (24ull << 30));
  CHECK(layout.at(40u, 8u) == (64ull << 30));
  CHECK(std::memcmp(encoded.bytes() + 48u, "NVIDIA GeForce RTX 4090", 24u) == 0);
  CHECK(layout.at(72u, 8u) == 0u); }

TEST(gpu_identity, round_trips_through_its_encoding)
{ Encoded encoded(sample());
  GpuIdentity decoded;
  CHECK_STATUS(decode(decoded, encoded.bytes()), Status::ok);
  CHECK_EQ(decoded.vendor_id(), 0x10DEu);
  CHECK_EQ(decoded.device_id(), 0x2684u);
  CHECK_EQ(decoded.device_type(), GpuIdentity::discrete_gpu);
  CHECK(std::memcmp(decoded.uuid(), uuid, sizeof uuid) == 0);
  CHECK(decoded.local_memory() == (24ull << 30));
  CHECK(decoded.shared_memory() == (64ull << 30));
  CHECK(std::strcmp(decoded.name(), "NVIDIA GeForce RTX 4090") == 0);
  Encoded again(decoded);
  CHECK(std::memcmp(again.bytes(), encoded.bytes(), GpuIdentity::encoded_size) == 0); }

TEST(gpu_identity, a_long_name_is_truncated_and_terminated)
{ const GpuIdentity identity(1u, 2u, GpuIdentity::cpu, uuid, 0u, 0u,
                            "An extraordinarily long adapter name that does not fit at all");
  CHECK_EQ(std::strlen(identity.name()), GpuIdentity::name_bytes - 1u);
  Encoded encoded(identity);
  GpuIdentity decoded;
  CHECK_STATUS(decode(decoded, encoded.bytes()), Status::ok); }

TEST(gpu_identity, a_default_identity_is_valid_and_empty)
{ const GpuIdentity identity;
  CHECK_EQ(identity.name()[0], 0);
  Encoded encoded(identity);
  GpuIdentity decoded;
  CHECK_STATUS(decode(decoded, encoded.bytes()), Status::ok); }

TEST(gpu_identity_decode, rejects_an_unknown_device_type)
{ Encoded encoded(sample());
  encoded.bytes()[8] = 5u;
  GpuIdentity decoded;
  CHECK_STATUS(decode(decoded, encoded.bytes()), Status::malformed); }

TEST(gpu_identity_decode, rejects_non_zero_reserved)
{ Encoded encoded(sample());
  encoded.bytes()[13] = 1u;
  GpuIdentity decoded;
  CHECK_STATUS(decode(decoded, encoded.bytes()), Status::malformed); }

TEST(gpu_identity_decode, rejects_a_name_without_a_terminator)
{ Encoded encoded(sample());
  std::memset(encoded.bytes() + 48u, 'A', GpuIdentity::name_bytes);
  GpuIdentity decoded;
  CHECK_STATUS(decode(decoded, encoded.bytes()), Status::malformed); }

TEST(gpu_identity_decode, rejects_bytes_after_the_terminator)
{ Encoded encoded(sample());
  encoded.bytes()[48u + 40u] = 'x';
  GpuIdentity decoded;
  CHECK_STATUS(decode(decoded, encoded.bytes()), Status::malformed); }

TEST(gpu_identity_decode, a_rejected_identity_leaves_the_destination_unchanged)
{ Encoded encoded(sample());
  GpuIdentity decoded;
  CHECK_STATUS(decode(decoded, encoded.bytes()), Status::ok);
  const GpuIdentity other(7u, 8u, GpuIdentity::integrated_gpu, uuid, 1u, 2u, "other");
  Encoded bad(other);
  bad.bytes()[8] = 9u;
  CHECK_STATUS(decode(decoded, bad.bytes()), Status::malformed);
  CHECK_EQ(decoded.vendor_id(), 0x10DEu); }

TEST(gpu_identity_decode, a_short_reader_is_malformed)
{ Encoded encoded(sample());
  WireReader reader(encoded.bytes(), GpuIdentity::encoded_size - 1u);
  GpuIdentity decoded;
  CHECK_STATUS(decoded.decode(reader), Status::malformed); }

/* A context started on a GPU is on the same one while the UUID matches, what
 * else the identity says notwithstanding. */
TEST(gpu_identity, the_same_gpu_is_the_one_with_the_same_uuid)
{ const GpuIdentity started = sample();
  const GpuIdentity renamed(0x10DEu, 0x2684u, GpuIdentity::discrete_gpu, uuid, 1u, 1u, "renamed");
  CHECK(started.same_gpu(renamed));
  std::uint8_t other[GpuIdentity::uuid_bytes] = {};
  other[0] = 1u;
  const GpuIdentity elsewhere(0x10DEu, 0x2684u, GpuIdentity::discrete_gpu, other, 24ull << 30, 64ull << 30,
                              "NVIDIA GeForce RTX 4090");
  CHECK(!started.same_gpu(elsewhere));
  CHECK(!started.same_gpu(GpuIdentity())); }
