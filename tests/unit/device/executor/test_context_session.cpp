/* One guest context on one host API, judged by what the device will act on:
 * which of the ABI's four results comes back, and how long the answer was. */
#include "device/abi/rvk_executor.h"
#include "device/executor/context_session.hpp"
#include "harness/wire/wire_arena_probe.hpp"
#include "proto/wire/wire_arena.hpp"
#include "device/executor/decode_arena.hpp"
#include "device/executor/import_table.hpp"
#include "device/executor/imported_blob.hpp"
#include "harness/executor/context_session_probe.hpp"
#include "harness/executor/imported_blob_probe.hpp"
#include "harness/executor/mock_session_backend.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstddef>
#include <cstdint>
#include <unistd.h>

using av::ContextSession;
using av::ContextSessionProbe;
using av::ImportedBlobProbe;
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


namespace {

std::uint8_t blob_backing[av::ImportedBlob::page_bytes];

RvkBlobHandle blob_handle(std::uint64_t size = av::ImportedBlob::page_bytes)
{ RvkBlobHandle handle = {};
  handle.kind = RVK_BLOB_HOST_POINTER;
  handle.pointer = blob_backing;
  handle.fd = -1;
  handle.size = size;
  return handle; }

}  // namespace

TEST(context_session_import, a_blob_the_backend_takes_is_held_until_released)
{ Bound bound;
  CHECK_EQ(bound.session().imports(), 0u);
  CHECK_EQ(bound.session().import_blob(5u, blob_handle()), std::uint32_t{RVK_EXECUTED});
  CHECK_EQ(bound.session().imports(), 1u);
  CHECK_EQ(bound.backend().adopted(), 1u);

  bound.session().release_blob(5u);
  CHECK_EQ(bound.session().imports(), 0u);
  CHECK_EQ(bound.backend().abandoned(), 1u);
  CHECK(bound.session().invariants_hold()); }

/* The guest names imported memory by what the backend recorded for the guest,
 * not by what the host API holds: an id never imported answers nothing, an
 * import answers its own, and a released one nothing again. */
TEST(context_session_import, answers_the_handle_the_guest_names_an_import_by)
{ Bound bound;
  CHECK_EQ(bound.session().imported_handle(5u), 0u);
  CHECK_EQ(bound.session().import_blob(5u, blob_handle()), std::uint32_t{RVK_EXECUTED});
  CHECK_EQ(bound.session().import_blob(6u, blob_handle()), std::uint32_t{RVK_EXECUTED});
  CHECK_EQ(bound.session().imported_handle(5u), 5u + MockSessionBackend::wire_offset);
  CHECK_EQ(bound.session().imported_handle(6u), 6u + MockSessionBackend::wire_offset);
  CHECK_EQ(bound.session().imported_handle(7u), 0u);
  CHECK(bound.session().invariants_hold());

  bound.session().release_blob(5u);
  CHECK_EQ(bound.session().imported_handle(5u), 0u);
  CHECK_EQ(bound.session().imported_handle(6u), 6u + MockSessionBackend::wire_offset);

  /* A refused import leaves nothing to name. */
  bound.backend().set_adopts(false);
  CHECK_EQ(bound.session().import_blob(8u, blob_handle()), std::uint32_t{RVK_UNDECODABLE});
  CHECK_EQ(bound.session().imported_handle(8u), 0u);
  CHECK(bound.session().invariants_hold()); }

/* Release is idempotent: the device may release an id whose import failed. */
TEST(context_session_import, releasing_one_it_never_took_is_nothing)
{ Bound bound;
  bound.session().release_blob(5u);
  CHECK_EQ(bound.backend().abandoned(), 0u);
  CHECK(bound.session().invariants_hold()); }

/* An id already imported is refused rather than silently torn down. */
TEST(context_session_import, the_same_id_twice_is_refused)
{ Bound bound;
  CHECK_EQ(bound.session().import_blob(5u, blob_handle()), std::uint32_t{RVK_EXECUTED});
  CHECK_EQ(bound.session().import_blob(5u, blob_handle()), std::uint32_t{RVK_UNDECODABLE});
  CHECK_EQ(bound.session().imports(), 1u);
  CHECK_EQ(bound.backend().adopted(), 1u); }

TEST(context_session_import, a_handle_the_abi_refuses_never_reaches_the_backend)
{ Bound bound;
  CHECK_EQ(bound.session().import_blob(5u, blob_handle(1u)), std::uint32_t{RVK_UNDECODABLE});
  CHECK_EQ(bound.backend().adopted(), 0u);
  CHECK_EQ(bound.session().imports(), 0u); }

/* An API that cannot take this kind is not a fault: the device is told no and
 * the guest is copied through instead, and the slot goes back. */
TEST(context_session_import, a_backend_that_will_not_take_it_costs_no_slot)
{ Bound bound;
  bound.backend().set_adopts(false);
  CHECK_EQ(bound.session().import_blob(5u, blob_handle()), std::uint32_t{RVK_UNDECODABLE});
  CHECK_EQ(bound.session().imports(), 0u);
  bound.backend().set_adopts(true);
  CHECK_EQ(bound.session().import_blob(5u, blob_handle()), std::uint32_t{RVK_EXECUTED});
  CHECK_EQ(bound.session().imports(), 1u); }

/* A guest that never releases leaks nothing: destroying the session gives back
 * what it never did, which is what an imported host pointer needs, since the
 * device unmaps it afterwards. */
TEST(context_session_import, releasing_the_session_gives_back_what_the_guest_did_not)
{ Bound bound;
  for (std::uint64_t id = 1u; id <= 3u; id++) {
      CHECK_EQ(bound.session().import_blob(id, blob_handle()), std::uint32_t{RVK_EXECUTED});
  }
  CHECK_EQ(bound.session().imports(), 3u);
  bound.session().release();
  CHECK_EQ(bound.backend().abandoned(), 3u);
  CHECK_EQ(bound.session().imports(), 0u);
  CHECK(bound.session().invariants_hold()); }

TEST(context_session_import, a_session_with_no_backend_imports_nothing)
{ ContextSession session;
  CHECK_EQ(session.import_blob(5u, blob_handle()), std::uint32_t{RVK_UNDECODABLE});
  CHECK_EQ(session.imports(), 0u);
  session.release_blob(5u);
  CHECK(session.invariants_hold()); }

/* Nothing is handed to a host device that is gone, a blob included: the
 * backend would be adopting memory into a device that will never run. */
TEST(context_session_import, a_lost_device_imports_nothing)
{ Bound bound;
  std::size_t answer = 0u;
  CHECK_EQ(bound.session().execute(MockSessionBackend::lose_id, payload, sizeof payload, nullptr,
                                   0u, answer),
           std::uint32_t{RVK_DEVICE_LOST});
  CHECK_EQ(bound.session().import_blob(5u, blob_handle()), std::uint32_t{RVK_UNDECODABLE});
  CHECK_EQ(bound.backend().adopted(), 0u);
  CHECK_EQ(bound.session().imports(), 0u);
  CHECK(bound.session().invariants_hold()); }

/* A session holding a blob the ABI would refuse is one whose backend was handed
 * something take never let through. */
TEST(context_session_invariants, an_import_that_breaks_its_own_breaks_them)
{ Bound bound;
  CHECK_EQ(bound.session().import_blob(5u, blob_handle()), std::uint32_t{RVK_EXECUTED});
  ContextSessionProbe probe(bound.session());
  ImportedBlobProbe(*probe.imports().find(5u)).set_size(1u);
  CHECK(!bound.session().invariants_hold()); }

/* A fault belongs to a host device; a session with none has nothing to be
 * faulted on, and the next context bound to the slot would inherit the fault. */
TEST(context_session_invariants, a_fault_with_no_backend_breaks_them)
{ ContextSession session;
  ContextSessionProbe(session).set_faulted(true);
  CHECK(!session.invariants_hold()); }

/* Imports with no backend are blobs nobody can abandon, so an imported host
 * pointer would outlive the mapping the device keeps for it. */
TEST(context_session_invariants, imports_with_no_backend_break_them)
{ Bound bound;
  CHECK_EQ(bound.session().import_blob(5u, blob_handle()), std::uint32_t{RVK_EXECUTED});
  ContextSessionProbe(bound.session()).drop_backend();
  CHECK(!bound.session().invariants_hold()); }

/* The one clause a public call cannot break: the arena's mark past its room. It
 * is what would let a decode hand out memory beyond the block, so it is proven
 * to be noticed rather than assumed never to happen. */
TEST(context_session_invariants, an_arena_used_past_its_room_breaks_it)
{ Bound bound;
  CHECK(bound.session().invariants_hold());
  av::ContextSessionProbe session(bound.session());
  av::WireArenaProbe arena(session.arena());
  arena.set_used(block_bytes + 1u);
  CHECK(!bound.session().invariants_hold()); }

/* Feature 053: a shared object reaches the backend only through a session the
 * root handed out, and a faulted one answers no to everything - the same rule
 * every command it runs is held to. */
TEST(context_session, the_shared_object_calls_reach_the_backend_of_a_live_session)
{ Bound bound;
  CHECK_STATUS(bound.status(), Status::ok);
  MockSessionBackend &backend = bound.backend();
  std::uint32_t handle_kind = 0u;
  int           fd = -1;
  CHECK(bound.session().export_object(RVK_SHARED_MEMORY, 0x40u, handle_kind, fd));
  CHECK_EQ(handle_kind, RVK_SHARED_HANDLE_DMA_BUF);
  CHECK(fd >= 0);
  CHECK_EQ(backend.exported_objects(), 1u);

  std::uint64_t made = 0u;
  CHECK(bound.session().import_object(RVK_SHARED_MEMORY, handle_kind, fd, 4096u, made));
  CHECK(made != 0u);
  CHECK_EQ(backend.imported_objects(), 1u);
  bound.session().release_object(RVK_SHARED_MEMORY, made);
  CHECK_EQ(backend.released_objects(), 1u);
  ::close(fd);
  CHECK(bound.session().invariants_hold()); }

TEST(context_session, a_backend_that_shares_nothing_is_a_refusal_and_not_a_fault)
{ Bound bound;
  bound.backend().set_shares(false);
  std::uint32_t handle_kind = 9u;
  int           fd = 9;
  CHECK(!bound.session().export_object(RVK_SHARED_MEMORY, 0x40u, handle_kind, fd));
  CHECK_EQ(handle_kind, 0u);
  CHECK_EQ(fd, -1);
  std::uint64_t made = 9u;
  CHECK(!bound.session().import_object(RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, 3, 4096u, made));
  CHECK_EQ(made, 0u);
  CHECK(bound.session().invariants_hold()); }

TEST(context_session, a_session_that_was_never_bound_shares_nothing_and_releases_nothing)
{ ContextSession session;
  std::uint32_t  handle_kind = 9u;
  int            fd = 9;
  CHECK(!session.export_object(RVK_SHARED_MEMORY, 0x40u, handle_kind, fd));
  CHECK_EQ(fd, -1);
  std::uint64_t made = 9u;
  CHECK(!session.import_object(RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, 3, 4096u, made));
  CHECK_EQ(made, 0u);
  session.release_object(RVK_SHARED_MEMORY, 0x40u);
  CHECK(session.invariants_hold()); }

/* A faulted session refuses the shared-object calls as it refuses every
 * command: what it was running is gone, and nothing it names can be trusted. */
TEST(context_session, a_faulted_session_shares_nothing)
{ Bound bound;
  ContextSessionProbe(bound.session()).set_faulted(true);
  std::uint32_t handle_kind = 9u;
  int           fd = 9;
  CHECK(!bound.session().export_object(RVK_SHARED_MEMORY, 0x40u, handle_kind, fd));
  std::uint64_t made = 9u;
  CHECK(!bound.session().import_object(RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, 3, 4096u, made));
  CHECK_EQ(made, 0u);
  CHECK_EQ(bound.backend().exported_objects(), 0u);
  ContextSessionProbe(bound.session()).set_faulted(false); }
