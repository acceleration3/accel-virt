/* What a shared-object command answers, judged from the guest's side: the ICD
 * decodes what the device wrote, and a hostile or older device is exactly as
 * foreign to it as a guest is to the device. A refusal carrying a handle is
 * the case that matters - a guest that believed one would go on to name an
 * object nothing ever made. */
#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/shared_object_answer.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ByteLayout;
using av::GuardedBuffer;
using av::SharedObjectAnswer;
using av::Status;

namespace {

void encode(const SharedObjectAnswer &answer, std::uint8_t *bytes)
{
    CHECK_STATUS(answer.encode(bytes, SharedObjectAnswer::encoded_size), Status::ok);
}

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    SharedObjectAnswer answer;
    const Status       status = answer.decode(bytes, length);
    if (status == Status::ok) {
        std::uint8_t again[SharedObjectAnswer::encoded_size] = {};
        if (answer.encode(again, sizeof again) != Status::ok ||
            std::memcmp(again, bytes, sizeof again) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}  // namespace

TEST(shared_object_answer_encode, lays_out_the_status_and_handle_with_the_reserved_word_zero)
{ std::uint8_t bytes[SharedObjectAnswer::encoded_size];
  std::memset(bytes, 0xEE, sizeof bytes);
  encode(SharedObjectAnswer(SharedObjectAnswer::done, 0x1122334455667788u), bytes);
  const ByteLayout layout(bytes, sizeof bytes);
  CHECK(layout.at(0u, 4u) == SharedObjectAnswer::done);
  CHECK(layout.at(4u, 4u) == 0u);
  CHECK(layout.at(8u, 8u) == 0x1122334455667788u); }

TEST(shared_object_answer_encode, a_short_destination_is_too_large_and_untouched)
{ GuardedBuffer buffer(SharedObjectAnswer::encoded_size - 1u);
  CHECK_STATUS(SharedObjectAnswer(SharedObjectAnswer::done, 1u).encode(buffer.data(), buffer.size()),
               Status::too_large);
  CHECK(buffer.guards_intact()); }

TEST(shared_object_answer_decode, an_open_that_worked_carries_the_handle_the_guest_will_name)
{ std::uint8_t bytes[SharedObjectAnswer::encoded_size] = {};
  encode(SharedObjectAnswer(SharedObjectAnswer::done, 0xABCDEFu), bytes);
  SharedObjectAnswer decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  CHECK(decoded.succeeded());
  CHECK_EQ(decoded.wire_handle(), 0xABCDEFu); }

TEST(shared_object_answer_decode, a_share_or_release_that_worked_carries_no_handle_and_is_still_success)
{ std::uint8_t bytes[SharedObjectAnswer::encoded_size] = {};
  encode(SharedObjectAnswer(SharedObjectAnswer::done, 0u), bytes);
  SharedObjectAnswer decoded(SharedObjectAnswer::no_room, 0u);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
  CHECK(decoded.succeeded());
  CHECK_EQ(decoded.wire_handle(), 0u); }

TEST(shared_object_answer_decode, every_refusal_the_device_may_give_decodes_and_carries_no_handle)
{ const std::uint32_t refusals[] = {
      SharedObjectAnswer::refused_request, SharedObjectAnswer::unknown_object,
      SharedObjectAnswer::mismatched_object, SharedObjectAnswer::executor_refused,
      SharedObjectAnswer::no_room, SharedObjectAnswer::conflict};
  for (const std::uint32_t status : refusals) {
      std::uint8_t bytes[SharedObjectAnswer::encoded_size] = {};
      encode(SharedObjectAnswer(status, 0u), bytes);
      SharedObjectAnswer decoded;
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
      CHECK(!decoded.succeeded());
      CHECK_EQ(decoded.status(), status);
      CHECK_EQ(decoded.wire_handle(), 0u);
  } }

TEST(shared_object_answer_decode, a_refusal_carrying_a_handle_is_malformed_and_left_unread)
{ std::uint8_t bytes[SharedObjectAnswer::encoded_size] = {};
  encode(SharedObjectAnswer(SharedObjectAnswer::done, 7u), bytes);
  const std::uint32_t status = SharedObjectAnswer::unknown_object;
  std::memcpy(bytes, &status, sizeof status);
  SharedObjectAnswer decoded(SharedObjectAnswer::done, 5u);
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::malformed);
  CHECK_EQ(decoded.wire_handle(), 5u); }

TEST(shared_object_answer_decode, a_status_the_device_has_no_name_for_is_malformed)
{ const std::uint32_t unknown[] = {SharedObjectAnswer::conflict + 1u, 100u, 0x80000000u, 0xFFFFFFFFu};
  for (const std::uint32_t status : unknown) {
      std::uint8_t bytes[SharedObjectAnswer::encoded_size] = {};
      encode(SharedObjectAnswer(SharedObjectAnswer::done, 0u), bytes);
      std::memcpy(bytes, &status, sizeof status);
      SharedObjectAnswer decoded;
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::malformed);
  } }

TEST(shared_object_answer_decode, a_non_zero_reserved_word_is_malformed)
{ std::uint8_t bytes[SharedObjectAnswer::encoded_size] = {};
  encode(SharedObjectAnswer(SharedObjectAnswer::done, 1u), bytes);
  bytes[4] = 1u;
  SharedObjectAnswer decoded;
  CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::malformed); }

TEST(shared_object_answer_decode, a_buffer_shorter_than_the_answer_is_malformed)
{ std::uint8_t bytes[SharedObjectAnswer::encoded_size] = {};
  encode(SharedObjectAnswer(SharedObjectAnswer::done, 1u), bytes);
  for (std::size_t length = 0u; length < SharedObjectAnswer::encoded_size; length++) {
      SharedObjectAnswer decoded;
      CHECK_STATUS(decoded.decode(bytes, length), Status::malformed);
  } }

TEST(shared_object_answer_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[SharedObjectAnswer::encoded_size] = {};
  encode(SharedObjectAnswer(SharedObjectAnswer::done, 0x4000u), bytes);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 128u); }
