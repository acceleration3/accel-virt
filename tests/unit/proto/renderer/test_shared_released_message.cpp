/* QEMU tells the renderer the guest let go of a shared object: the object id,
 * eight bytes and nothing beside it - 0 for every object, which a reset sends
 * because the guest's ids start again. */
#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/renderer/shared_released_message.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ByteLayout;
using av::GuardedBuffer;
using av::SharedReleasedMessage;
using av::Status;

namespace {

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    SharedReleasedMessage message;
    const Status status = message.decode(bytes, length, SharedReleasedMessage::descriptors);
    if (status == Status::ok) {
        std::uint8_t again[SharedReleasedMessage::encoded_size] = {};
        if (message.encode(again, sizeof again) != Status::ok || std::memcmp(again, bytes, sizeof again) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}  // namespace

TEST(shared_released_message_encode, is_the_object_id)
{ GuardedBuffer bytes(SharedReleasedMessage::encoded_size);
  CHECK_STATUS(SharedReleasedMessage(0x1122334455667788u).encode(bytes.data(), bytes.size()), Status::ok);
  const ByteLayout layout(bytes.data(), bytes.size());
  CHECK(layout.at(0u, 4u) == 0x55667788u);
  CHECK(layout.at(4u, 4u) == 0x11223344u);
  CHECK_EQ(SharedReleasedMessage::descriptors, 0u);
  CHECK(bytes.guards_intact()); }

/* Object 0 is every object - a reset - so it travels like any other. */
TEST(shared_released_message, round_trips_any_id_zero_included)
{ const std::uint64_t ids[] = {0u, 1u, 0xFFFFFFFFFFFFFFFFu};
  for (std::uint64_t id : ids) {
      std::uint8_t bytes[SharedReleasedMessage::encoded_size] = {};
      CHECK_STATUS(SharedReleasedMessage(id).encode(bytes, sizeof bytes), Status::ok);
      SharedReleasedMessage decoded(7u);
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes, 0u), Status::ok);
      CHECK(decoded.object_id() == id);
  } }

TEST(shared_released_message_encode, a_short_or_missing_destination_is_too_large)
{ GuardedBuffer buffer(SharedReleasedMessage::encoded_size - 1u);
  CHECK_STATUS(SharedReleasedMessage(1u).encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact());
  CHECK_STATUS(SharedReleasedMessage(1u).encode(nullptr, SharedReleasedMessage::encoded_size), Status::too_large); }

/* A descriptor on a message that takes none would be leaked by a receiver
 * that trusted the kind. */
TEST(shared_released_message_decode, any_descriptor_other_length_or_no_bytes_is_malformed_and_changes_nothing)
{ std::uint8_t bytes[SharedReleasedMessage::encoded_size + 1u] = {};
  CHECK_STATUS(SharedReleasedMessage(9u).encode(bytes, sizeof bytes), Status::ok);
  SharedReleasedMessage decoded(5u);
  CHECK_STATUS(decoded.decode(bytes, SharedReleasedMessage::encoded_size, 1u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, SharedReleasedMessage::encoded_size - 1u, 0u), Status::malformed);
  CHECK_STATUS(decoded.decode(bytes, SharedReleasedMessage::encoded_size + 1u, 0u), Status::malformed);
  CHECK_STATUS(decoded.decode(nullptr, SharedReleasedMessage::encoded_size, 0u), Status::malformed);
  CHECK(decoded.object_id() == 5u); }

TEST(shared_released_message_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[SharedReleasedMessage::encoded_size] = {};
  CHECK_STATUS(SharedReleasedMessage(3u).encode(bytes, sizeof bytes), Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 64u); }
