/* One guest context on one host API, judged by what the device will act on:
 * which of the ABI's four results comes back, and how long the answer was. */
#include "device/abi/rvk_executor.h"
#include "device/executor/context_session.hpp"
#include "device/executor/decode_arena.hpp"
#include "harness/executor/mock_session_backend.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstddef>
#include <cstdint>

using av::ContextSession;
using av::MockSessionBackend;
using av::Status;

namespace {

constexpr std::uint8_t payload[] = {1, 2, 3, 4, 5, 6, 7, 8};
constexpr std::size_t  block_bytes = 4096u;

class Bound {
public:
    Bound() : block_{}
    {
        status_ = session_.initialize(backend_, block_, sizeof block_);
    }
    av::Status          status() const { return status_; }
    MockSessionBackend &backend() { return backend_; }
    ContextSession     &session() { return session_; }
    std::uint8_t       *block() { return block_; }

private:
    std::uint8_t       block_[block_bytes];
    MockSessionBackend backend_;
    ContextSession     session_;
    av::Status         status_ = av::Status::ok;
};

}  // namespace

TEST(context_session, a_command_it_knows_runs_and_answers)
{ Bound bound;
  MockSessionBackend &backend = bound.backend();
  ContextSession &session = bound.session();
  (void)backend;
  CHECK_STATUS(bound.status(), Status::ok);
  CHECK(session.live());

  std::uint8_t reply[16] = {};
  std::size_t  answer = 0u;
  CHECK_EQ(session.execute(MockSessionBackend::echo_id, payload, sizeof payload, reply,
                           sizeof reply, answer),
           std::uint32_t{RVK_EXECUTED});
  CHECK_EQ(answer, sizeof payload);
  for (std::size_t i = 0u; i < sizeof payload; i++) CHECK_EQ(reply[i], payload[i]);
  CHECK(session.invariants_hold()); }

/* An id the profile did not select is skipped by the device, not faulted, so
 * it must not be confused with a frame that failed to decode. */
TEST(context_session, an_id_it_does_not_know_is_unknown_not_undecodable)
{ Bound bound;
  MockSessionBackend &backend = bound.backend();
  ContextSession &session = bound.session();
  (void)backend;
  CHECK_STATUS(bound.status(), Status::ok);
  std::size_t answer = 1u;
  CHECK_EQ(session.execute(0xDEADu, payload, sizeof payload, nullptr, 0u, answer),
           std::uint32_t{RVK_UNKNOWN});
  CHECK_EQ(answer, 0u);
  CHECK_EQ(backend.ran(), 0u); }

TEST(context_session, a_payload_that_does_not_decode_is_undecodable_and_starves_nothing)
{ Bound bound;
  MockSessionBackend &backend = bound.backend();
  ContextSession &session = bound.session();
  (void)backend;
  CHECK_STATUS(bound.status(), Status::ok);
  std::size_t answer = 1u;
  CHECK_EQ(session.execute(MockSessionBackend::malformed_id, payload, sizeof payload, nullptr, 0u,
                           answer),
           std::uint32_t{RVK_UNDECODABLE});
  CHECK_EQ(answer, 0u);
  CHECK_EQ(session.starved(), 0u); }

/* The distinction the whole refusal count exists for: the guest is told the
 * same thing, the host can tell them apart. */
TEST(context_session, a_command_the_block_will_not_hold_is_undecodable_and_counted)
{ Bound bound;
  MockSessionBackend &backend = bound.backend();
  ContextSession &session = bound.session();
  (void)backend;
  CHECK_STATUS(bound.status(), Status::ok);
  std::size_t answer = 0u;
  CHECK_EQ(session.execute(MockSessionBackend::greedy_id, payload, sizeof payload, nullptr, 0u,
                           answer),
           std::uint32_t{RVK_UNDECODABLE});
  CHECK_EQ(session.starved(), 1u);
  CHECK(session.invariants_hold()); }

/* Contract A7 at the ABI: an answer longer than the capacity reports its
 * length, and the caller's block is left as it was. */
TEST(context_session, an_answer_longer_than_the_capacity_reports_its_length)
{ Bound bound;
  MockSessionBackend &backend = bound.backend();
  ContextSession &session = bound.session();
  backend.set_answer_bytes(64u);
  CHECK_STATUS(bound.status(), Status::ok);

  std::uint8_t reply[16];
  for (std::uint8_t &byte : reply) byte = 0xAB;
  std::size_t answer = 0u;
  CHECK_EQ(session.execute(MockSessionBackend::long_answer_id, payload, sizeof payload, reply,
                           sizeof reply, answer),
           std::uint32_t{RVK_EXECUTED});
  CHECK_EQ(answer, 64u);
  /* Nothing was written: a partial answer is worse than none, because the
   * guest cannot tell it is partial. */
  for (const std::uint8_t byte : reply) CHECK_EQ(byte, std::uint8_t{0xAB}); }

/* Every command that did not ask for an answer is handed a null reply and a
 * capacity of zero, which must not be a failure. */
TEST(context_session, a_command_with_nowhere_to_answer_still_runs)
{ Bound bound;
  MockSessionBackend &backend = bound.backend();
  ContextSession &session = bound.session();
  (void)backend;
  CHECK_STATUS(bound.status(), Status::ok);
  std::size_t answer = 0u;
  CHECK_EQ(session.execute(MockSessionBackend::echo_id, payload, sizeof payload, nullptr, 0u,
                           answer),
           std::uint32_t{RVK_EXECUTED});
  CHECK_EQ(answer, sizeof payload);
  CHECK_EQ(backend.ran(), 1u); }

/* Once the host device is gone nothing more runs on it, and the session says
 * so without asking again. */
TEST(context_session, a_lost_device_is_lost_for_every_command_after_it)
{ Bound bound;
  MockSessionBackend &backend = bound.backend();
  ContextSession &session = bound.session();
  (void)backend;
  CHECK_STATUS(bound.status(), Status::ok);
  std::size_t answer = 0u;
  CHECK_EQ(session.execute(MockSessionBackend::lose_id, payload, sizeof payload, nullptr, 0u,
                           answer),
           std::uint32_t{RVK_DEVICE_LOST});
  CHECK_EQ(session.execute(MockSessionBackend::echo_id, payload, sizeof payload, nullptr, 0u,
                           answer),
           std::uint32_t{RVK_DEVICE_LOST});
  /* The second command never reached the backend. */
  CHECK_EQ(backend.ran(), 1u);
  CHECK(session.invariants_hold()); }

TEST(context_session, a_session_with_no_backend_runs_nothing)
{ ContextSession session;
  CHECK(!session.live());
  std::size_t answer = 1u;
  CHECK_EQ(session.execute(MockSessionBackend::echo_id, payload, sizeof payload, nullptr, 0u,
                           answer),
           std::uint32_t{RVK_DEVICE_LOST});
  CHECK_EQ(answer, 0u);
  CHECK(session.invariants_hold()); }

TEST(context_session, binding_twice_is_refused_and_releasing_gives_it_back)
{ Bound bound;
  MockSessionBackend &backend = bound.backend();
  ContextSession &session = bound.session();
  (void)backend;
  CHECK_STATUS(bound.status(), Status::ok);
  CHECK_STATUS(session.initialize(backend, bound.block(), block_bytes), Status::invalid_state);
  session.release();
  CHECK(!session.live());
  CHECK_STATUS(session.initialize(backend, bound.block(), block_bytes), Status::ok);
  CHECK(session.invariants_hold()); }

/* A release after a fault gives back a session that runs again, because the
 * device reuses the slot for the next context. */
TEST(context_session, a_released_session_is_not_still_faulted)
{ Bound bound;
  MockSessionBackend &backend = bound.backend();
  ContextSession &session = bound.session();
  (void)backend;
  CHECK_STATUS(bound.status(), Status::ok);
  std::size_t answer = 0u;
  CHECK_EQ(session.execute(MockSessionBackend::lose_id, payload, sizeof payload, nullptr, 0u,
                           answer),
           std::uint32_t{RVK_DEVICE_LOST});
  session.release();
  CHECK(session.invariants_hold());
  CHECK(!session.live()); }
