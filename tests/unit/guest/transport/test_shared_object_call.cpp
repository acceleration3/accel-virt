#include "core/support/status.hpp"
#include "guest/transport/shared_object_call.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/transport/mock_transport.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/shared_object_answer.hpp"
#include "proto/submission/shared_object_request.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::CommandHeader;
using av::MockTransport;
using av::SharedObjectAnswer;
using av::SharedObjectCall;
using av::SharedObjectKind;
using av::SharedObjectRequest;
using av::Status;

namespace {

constexpr std::uint64_t object_id = 0x0000'1234'5678'9ABCull;
constexpr std::uint64_t wire_handle = 0x0000'7000'0000'0042ull;
constexpr std::uint64_t memory_bytes = 4u * 4096u;

class Answer {
public:
    Answer(std::uint32_t status, std::uint32_t reserved, std::uint64_t handle) : bytes_{}
    {
        std::memcpy(bytes_, &status, sizeof status);
        std::memcpy(bytes_ + 4u, &reserved, sizeof reserved);
        std::memcpy(bytes_ + 8u, &handle, sizeof handle);
    }
    const std::uint8_t *bytes() const { return bytes_; }

private:
    std::uint8_t bytes_[SharedObjectAnswer::encoded_size + 1u];
};

std::uint32_t u32_at(const std::uint8_t *bytes, std::size_t at)
{
    std::uint32_t value = 0u;
    std::memcpy(&value, bytes + at, sizeof value);
    return value;
}

std::uint64_t u64_at(const std::uint8_t *bytes, std::size_t at)
{
    std::uint64_t value = 0u;
    std::memcpy(&value, bytes + at, sizeof value);
    return value;
}

}

TEST(shared_object_call_share, sends_the_contracts_body_with_the_handle_the_guest_already_holds)
{ MockTransport    transport;
  SharedObjectCall call(transport);
  const Answer     done(SharedObjectAnswer::done, 0u, 0u);
  transport.set_reply(done.bytes(), SharedObjectAnswer::encoded_size);
  CHECK_STATUS(call.share(object_id, SharedObjectKind::memory, memory_bytes, wire_handle), Status::ok);
  CHECK_EQ(call.device_status(), std::uint32_t{SharedObjectAnswer::done});
  CHECK_EQ(transport.last_command(), CommandHeader::share_object_id);
  CHECK_EQ(transport.last_length(), SharedObjectRequest::encoded_size);
  const std::uint8_t *const body = transport.last_payload();
  CHECK_EQ(u64_at(body, 0u), object_id);
  CHECK_EQ(u32_at(body, 8u), std::uint32_t{SharedObjectKind::memory});
  CHECK_EQ(u32_at(body, 12u), 0u);
  CHECK_EQ(u64_at(body, 16u), wire_handle);
  CHECK_EQ(u64_at(body, 24u), memory_bytes);
  CHECK_EQ(transport.end_calls(), 1u); }

TEST(shared_object_call_open, answers_the_handle_the_device_made_and_carries_no_handle_of_its_own)
{ MockTransport    transport;
  SharedObjectCall call(transport);
  const Answer     opened(SharedObjectAnswer::done, 0u, wire_handle);
  transport.set_reply(opened.bytes(), SharedObjectAnswer::encoded_size);
  std::uint64_t    made = 0u;
  CHECK_STATUS(call.open(object_id, SharedObjectKind::memory, memory_bytes, 0u, made), Status::ok);
  CHECK_EQ(made, wire_handle);
  CHECK_EQ(transport.last_command(), CommandHeader::open_shared_object_id);
  CHECK_EQ(u64_at(transport.last_payload(), 16u), 0u);
  CHECK_EQ(u64_at(transport.last_payload(), 24u), memory_bytes); }

TEST(shared_object_call_release, names_the_object_and_the_open_it_lets_go_of)
{ MockTransport    transport;
  SharedObjectCall call(transport);
  const Answer     done(SharedObjectAnswer::done, 0u, 0u);
  transport.set_reply(done.bytes(), SharedObjectAnswer::encoded_size);
  CHECK_STATUS(call.release(object_id, SharedObjectKind::monitored_fence, 0u, 0u), Status::ok);
  CHECK_EQ(transport.last_command(), CommandHeader::release_shared_object_id);
  CHECK_EQ(u32_at(transport.last_payload(), 8u), std::uint32_t{SharedObjectKind::monitored_fence});
  CHECK_EQ(u64_at(transport.last_payload(), 16u), 0u);
  CHECK_EQ(u64_at(transport.last_payload(), 24u), 0u); }

TEST(shared_object_call, an_open_and_a_release_name_what_they_are_made_into)
{ MockTransport    transport;
  SharedObjectCall call(transport);
  const Answer     opened(SharedObjectAnswer::done, 0u, wire_handle);
  transport.set_reply(opened.bytes(), SharedObjectAnswer::encoded_size);
  std::uint64_t    made = 0u;
  CHECK_STATUS(call.open(object_id, SharedObjectKind::memory, memory_bytes, 0x55u, made), Status::ok);
  CHECK_EQ(u64_at(transport.last_payload(), 16u), 0x55u);
  const Answer done(SharedObjectAnswer::done, 0u, 0u);
  transport.set_reply(done.bytes(), SharedObjectAnswer::encoded_size);
  CHECK_STATUS(call.release(object_id, SharedObjectKind::memory, memory_bytes, wire_handle), Status::ok);
  CHECK_EQ(transport.last_command(), CommandHeader::release_shared_object_id);
  CHECK_EQ(u64_at(transport.last_payload(), 16u), wire_handle); }

TEST(shared_object_call, a_body_the_wire_refuses_is_not_sent)
{ MockTransport    transport;
  SharedObjectCall call(transport);
  std::uint64_t    made = 9u;
  CHECK_STATUS(call.share(0u, SharedObjectKind::memory, memory_bytes, wire_handle), Status::invalid_parameter);
  CHECK_STATUS(call.open(object_id, 0u, memory_bytes, 0u, made), Status::invalid_parameter);
  CHECK_STATUS(call.open(object_id, SharedObjectKind::native_fence + 1u, 0u, 0u, made), Status::invalid_parameter);
  CHECK_STATUS(call.share(object_id, SharedObjectKind::memory, 0u, wire_handle), Status::invalid_parameter);
  CHECK_STATUS(call.open(object_id, SharedObjectKind::native_fence, memory_bytes, 0u, made),
               Status::invalid_parameter);
  CHECK_STATUS(call.open(object_id, SharedObjectKind::memory, 4097u, 0u, made), Status::invalid_parameter);
  CHECK_EQ(made, 0u);
  CHECK_EQ(transport.sends(), 0u); }

TEST(shared_object_call, a_refusal_is_the_devices_status_and_no_handle)
{ const std::uint32_t refusals[] = {SharedObjectAnswer::refused_request, SharedObjectAnswer::unknown_object,
                                    SharedObjectAnswer::mismatched_object,
                                    SharedObjectAnswer::executor_refused, SharedObjectAnswer::no_room,
                                    SharedObjectAnswer::conflict};
  for (const std::uint32_t refusal : refusals) {
      MockTransport    transport;
      SharedObjectCall call(transport);
      const Answer     refused(refusal, 0u, 0u);
      transport.set_reply(refused.bytes(), SharedObjectAnswer::encoded_size);
      std::uint64_t made = 9u;
      CHECK_STATUS(call.open(object_id, SharedObjectKind::memory, memory_bytes, 0u, made),
                   Status::invalid_parameter);
      CHECK_EQ(made, 0u);
      CHECK_EQ(call.device_status(), refusal);
      CHECK_STATUS(call.share(object_id, SharedObjectKind::memory, memory_bytes, wire_handle),
                   Status::invalid_parameter);
      CHECK_EQ(call.device_status(), refusal);
      CHECK_STATUS(call.release(object_id, SharedObjectKind::memory, memory_bytes, 0u), Status::ok); } }

TEST(shared_object_call, hostile_answers_make_nothing)
{ struct Case { const char *what; std::uint32_t status; std::uint32_t reserved; std::uint64_t handle;
                std::size_t length; Status expected; };
  const Case cases[] = {
      {"empty: a device older than these commands", 0u, 0u, 0u, 0u, Status::not_supported},
      {"one byte short", SharedObjectAnswer::done, 0u, wire_handle, SharedObjectAnswer::encoded_size - 1u,
       Status::malformed},
      {"one byte long", SharedObjectAnswer::done, 0u, wire_handle, SharedObjectAnswer::encoded_size + 1u,
       Status::malformed},
      {"a status past the last one", SharedObjectAnswer::conflict + 1u, 0u, 0u,
       SharedObjectAnswer::encoded_size, Status::malformed},
      {"a reserved word that is not 0", SharedObjectAnswer::done, 1u, wire_handle,
       SharedObjectAnswer::encoded_size, Status::malformed},
      {"a refusal carrying a handle", SharedObjectAnswer::no_room, 0u, wire_handle,
       SharedObjectAnswer::encoded_size, Status::malformed},
      {"done with no handle at all", SharedObjectAnswer::done, 0u, 0u, SharedObjectAnswer::encoded_size,
       Status::malformed},
  };
  for (const Case &one : cases) {
      MockTransport    transport;
      SharedObjectCall call(transport);
      const Answer     answer(one.status, one.reserved, one.handle);
      transport.set_reply(answer.bytes(), one.length);
      std::uint64_t made = 9u;
      if (call.open(object_id, SharedObjectKind::memory, memory_bytes, 0u, made) != one.expected)
          av::TestRegistry::instance().record_failure(__FILE__, __LINE__, one.what);
      CHECK_EQ(made, 0u); } }

TEST(shared_object_call, a_share_or_a_release_answered_with_a_handle_is_malformed)
{ MockTransport    transport;
  SharedObjectCall call(transport);
  const Answer     handed(SharedObjectAnswer::done, 0u, wire_handle);
  transport.set_reply(handed.bytes(), SharedObjectAnswer::encoded_size);
  CHECK_STATUS(call.share(object_id, SharedObjectKind::memory, memory_bytes, wire_handle), Status::malformed);
  CHECK_STATUS(call.release(object_id, SharedObjectKind::memory, memory_bytes, 0u), Status::malformed); }

TEST(shared_object_call, a_call_that_fails_makes_nothing)
{ MockTransport    transport;
  SharedObjectCall call(transport);
  const Answer     opened(SharedObjectAnswer::done, 0u, wire_handle);
  transport.set_reply(opened.bytes(), SharedObjectAnswer::encoded_size);
  transport.fail_next();
  std::uint64_t made = 9u;
  CHECK_STATUS(call.open(object_id, SharedObjectKind::memory, memory_bytes, 0u, made), Status::invalid_state);
  CHECK_EQ(made, 0u);
  transport.fail_next();
  CHECK_STATUS(call.share(object_id, SharedObjectKind::memory, memory_bytes, wire_handle),
               Status::invalid_state);
  transport.fail_next();
  CHECK_STATUS(call.release(object_id, SharedObjectKind::memory, memory_bytes, 0u), Status::invalid_state); }
