/* One guest context on one host API, judged by what the device will act on:
 * which of the ABI's four results comes back, and how long the answer was. */
#include "device/abi/av_executor.h"
#include "executor/context_session.hpp"
#include "harness/wire/wire_arena_probe.hpp"
#include "proto/wire/wire_arena.hpp"
#include "executor/gpu_clock_hold.hpp"
#include "executor/import_table.hpp"
#include "executor/imported_blob.hpp"
#include "executor/host_memory_source.hpp"
#include "executor/source_memory.hpp"
#include "executor/shim/host_heap.hpp"
#include "harness/executor/mock_memory_exporter.hpp"
#include "proto/resources/host_memory_request.hpp"
#include "harness/executor/context_session_probe.hpp"
#include "harness/executor/mock_session_backend.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstddef>
#include <cstdint>
#include <unistd.h>

using av::ContextSession;
using av::ContextSessionProbe;
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
           std::uint32_t{AV_EXECUTED});
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
           std::uint32_t{AV_UNKNOWN});
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
           std::uint32_t{AV_UNDECODABLE});
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
           std::uint32_t{AV_UNDECODABLE});
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
           std::uint32_t{AV_EXECUTED});
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
           std::uint32_t{AV_EXECUTED});
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
           std::uint32_t{AV_DEVICE_LOST});
  CHECK_EQ(session.execute(MockSessionBackend::echo_id, payload, sizeof payload, nullptr, 0u,
                           answer),
           std::uint32_t{AV_DEVICE_LOST});
  /* The second command never reached the backend. */
  CHECK_EQ(backend.ran(), 1u);
  CHECK(session.invariants_hold()); }

TEST(context_session, a_session_with_no_backend_runs_nothing)
{ ContextSession session;
  CHECK(!session.live());
  std::size_t answer = 1u;
  CHECK_EQ(session.execute(MockSessionBackend::echo_id, payload, sizeof payload, nullptr, 0u,
                           answer),
           std::uint32_t{AV_DEVICE_LOST});
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
           std::uint32_t{AV_DEVICE_LOST});
  session.release();
  CHECK(session.invariants_hold());
  CHECK(!session.live()); }


namespace {

constexpr std::uint32_t domain = 3u;
constexpr std::uint64_t key = 0x10u;

/* A session drawing from a memory source of one coherent, exportable type,
 * with one page allocated under `key` in its domain - the device's hold. */
class Drawing {
public:
    Drawing() : exporter_(), heap_(), source_(exporter_, heap_), memory_{-1, 0u, 0u}
    {
        exporter_.add_type(av::MockMemoryExporter::host_visible | av::MockMemoryExporter::host_coherent, true);
        bound_.session().draw_from(source_, domain);
        static_cast<void>(source_.allocate(domain, key, 4096u, upload(), memory_));
    }
    ~Drawing()
    {
        bound_.session().release();
        if (memory_.fd >= 0) close(memory_.fd);
    }
    static av::HostMemoryRequest upload() { return av::HostMemoryRequest(av::HostMemoryRequest::Kind::upload, 0u); }
    Bound                  &bound() { return bound_; }
    ContextSession         &session() { return bound_.session(); }
    MockSessionBackend     &backend() { return bound_.backend(); }
    av::MockMemoryExporter &exporter() { return exporter_; }
    av::HostMemorySource   &source() { return source_; }

private:
    av::MockMemoryExporter exporter_;
    av::HostHeap           heap_;
    av::HostMemorySource   source_;
    AvHostMemory           memory_;
    Bound                  bound_;
};

}  // namespace

TEST(context_session_import, memory_the_backend_takes_is_held_until_released)
{ Drawing drawing;
  CHECK_EQ(drawing.session().imports(), 0u);
  CHECK_EQ(drawing.session().import_blob(5u, key), std::uint32_t{AV_EXECUTED});
  CHECK_EQ(drawing.session().imports(), 1u);
  CHECK_EQ(drawing.backend().adopted(), 1u);
  CHECK(drawing.session().invariants_hold());

  drawing.session().release_blob(5u);
  CHECK_EQ(drawing.session().imports(), 0u);
  CHECK_EQ(drawing.backend().abandoned(), 1u);
  CHECK(drawing.session().invariants_hold()); }

/* The import holds the memory: the device letting go of its key leaves it
 * alive until the import goes, and then it is freed. */
TEST(context_session_import, an_import_keeps_memory_the_device_let_go_of)
{ Drawing drawing;
  CHECK_EQ(drawing.session().import_blob(5u, key), std::uint32_t{AV_EXECUTED});
  drawing.source().free(domain, key);
  CHECK_EQ(drawing.exporter().live(), 1u);
  CHECK_EQ(drawing.source().live(), 1u);
  drawing.session().release_blob(5u);
  CHECK_EQ(drawing.exporter().live(), 0u);
  CHECK_EQ(drawing.source().live(), 0u); }

/* The guest names imported memory by what the backend recorded for the guest,
 * not by what the host API holds: an id never imported answers nothing, an
 * import answers its own, and a released one nothing again. */
TEST(context_session_import, answers_the_handle_the_guest_names_an_import_by)
{ Drawing drawing;
  CHECK_EQ(drawing.session().imported_handle(5u), 0u);
  CHECK_EQ(drawing.session().import_blob(5u, key), std::uint32_t{AV_EXECUTED});
  CHECK_EQ(drawing.session().import_blob(6u, key), std::uint32_t{AV_EXECUTED});
  CHECK_EQ(drawing.session().imported_handle(5u), 5u + MockSessionBackend::wire_offset);
  CHECK_EQ(drawing.session().imported_handle(6u), 6u + MockSessionBackend::wire_offset);
  CHECK_EQ(drawing.session().imported_handle(7u), 0u);
  CHECK(drawing.session().invariants_hold());

  drawing.session().release_blob(5u);
  CHECK_EQ(drawing.session().imported_handle(5u), 0u);
  CHECK_EQ(drawing.session().imported_handle(6u), 6u + MockSessionBackend::wire_offset);

  /* A refused import leaves nothing to name. */
  drawing.backend().set_adopts(false);
  CHECK_EQ(drawing.session().import_blob(8u, key), std::uint32_t{AV_UNDECODABLE});
  CHECK_EQ(drawing.session().imported_handle(8u), 0u);
  CHECK(drawing.session().invariants_hold()); }

/* Release is idempotent: the device may release an id whose import failed. */
TEST(context_session_import, releasing_one_it_never_took_is_nothing)
{ Drawing drawing;
  drawing.session().release_blob(5u);
  CHECK_EQ(drawing.backend().abandoned(), 0u);
  CHECK(drawing.session().invariants_hold()); }

/* An id already imported is refused rather than silently torn down, and the
 * reference the refused one took goes back. */
TEST(context_session_import, the_same_id_twice_is_refused)
{ Drawing drawing;
  CHECK_EQ(drawing.session().import_blob(5u, key), std::uint32_t{AV_EXECUTED});
  CHECK_EQ(drawing.session().import_blob(5u, key), std::uint32_t{AV_UNDECODABLE});
  CHECK_EQ(drawing.session().imports(), 1u);
  CHECK_EQ(drawing.backend().adopted(), 1u);
  drawing.session().release_blob(5u);
  drawing.source().free(domain, key);
  CHECK_EQ(drawing.exporter().live(), 0u); }

/* A key the source has no memory under - never allocated, freed, or another
 * domain's - never reaches the backend. */
TEST(context_session_import, a_key_with_no_memory_never_reaches_the_backend)
{ Drawing drawing;
  CHECK_EQ(drawing.session().import_blob(5u, key + 1u), std::uint32_t{AV_UNDECODABLE});
  AvHostMemory elsewhere = {-1, 0u, 0u};
  CHECK_STATUS(drawing.source().allocate(domain + 1u, key + 2u, 4096u, Drawing::upload(), elsewhere), Status::ok);
  CHECK_EQ(drawing.session().import_blob(5u, key + 2u), std::uint32_t{AV_UNDECODABLE});
  drawing.source().free(domain, key);
  CHECK_EQ(drawing.session().import_blob(5u, key), std::uint32_t{AV_UNDECODABLE});
  CHECK_EQ(drawing.backend().adopted(), 0u);
  CHECK_EQ(drawing.session().imports(), 0u);
  drawing.source().free(domain + 1u, key + 2u);
  close(elsewhere.fd); }

/* An API that will not take the memory is not a fault: the device is told no,
 * the slot goes back, and so does the memory's reference. */
TEST(context_session_import, a_backend_that_will_not_take_it_costs_no_slot)
{ Drawing drawing;
  drawing.backend().set_adopts(false);
  CHECK_EQ(drawing.session().import_blob(5u, key), std::uint32_t{AV_UNDECODABLE});
  CHECK_EQ(drawing.session().imports(), 0u);
  drawing.backend().set_adopts(true);
  CHECK_EQ(drawing.session().import_blob(5u, key), std::uint32_t{AV_EXECUTED});
  CHECK_EQ(drawing.session().imports(), 1u);
  drawing.session().release_blob(5u);
  drawing.source().free(domain, key);
  CHECK_EQ(drawing.exporter().live(), 0u); }

/* A table that is full refuses the next import and gives its reference back. */
TEST(context_session_import, a_full_table_refuses_and_holds_nothing_more)
{ Drawing drawing;
  for (std::uint64_t id = 1u; id <= av::ImportTable::max_imports; id++)
      CHECK_EQ(drawing.session().import_blob(id, key), std::uint32_t{AV_EXECUTED});
  CHECK_EQ(drawing.session().import_blob(av::ImportTable::max_imports + 1u, key), std::uint32_t{AV_UNDECODABLE});
  CHECK_EQ(drawing.session().imports(), av::ImportTable::max_imports);
  drawing.session().release();
  drawing.source().free(domain, key);
  CHECK_EQ(drawing.exporter().live(), 0u); }

/* A guest that never releases leaks nothing: destroying the session gives back
 * what it never did, to the backend and to the source. */
TEST(context_session_import, releasing_the_session_gives_back_what_the_guest_did_not)
{ Drawing drawing;
  for (std::uint64_t id = 1u; id <= 3u; id++)
      CHECK_EQ(drawing.session().import_blob(id, key), std::uint32_t{AV_EXECUTED});
  CHECK_EQ(drawing.session().imports(), 3u);
  drawing.source().free(domain, key);
  CHECK_EQ(drawing.exporter().live(), 1u);
  drawing.session().release();
  CHECK_EQ(drawing.backend().abandoned(), 3u);
  CHECK_EQ(drawing.session().imports(), 0u);
  CHECK_EQ(drawing.exporter().live(), 0u);
  CHECK(drawing.session().invariants_hold()); }

/* A session with no backend, or none to draw from, imports nothing. */
TEST(context_session_import, a_session_with_no_backend_or_source_imports_nothing)
{ ContextSession session;
  CHECK_EQ(session.import_blob(5u, key), std::uint32_t{AV_UNDECODABLE});
  CHECK_EQ(session.imports(), 0u);
  session.release_blob(5u);
  CHECK(session.invariants_hold());
  Bound bound;
  CHECK_EQ(bound.session().import_blob(5u, key), std::uint32_t{AV_UNDECODABLE});
  CHECK_EQ(bound.backend().adopted(), 0u); }

/* Nothing is handed to a host device that is gone: the backend would be
 * adopting memory into a device that will never run. */
TEST(context_session_import, a_lost_device_imports_nothing)
{ Drawing drawing;
  std::size_t answer = 0u;
  CHECK_EQ(drawing.session().execute(MockSessionBackend::lose_id, payload, sizeof payload, nullptr, 0u, answer),
           std::uint32_t{AV_DEVICE_LOST});
  CHECK_EQ(drawing.session().import_blob(5u, key), std::uint32_t{AV_UNDECODABLE});
  CHECK_EQ(drawing.backend().adopted(), 0u);
  CHECK_EQ(drawing.session().imports(), 0u);
  CHECK(drawing.session().invariants_hold()); }

/* A table whose count and slots disagree is one the session cannot release
 * whole. */
TEST(context_session_invariants, an_import_table_that_breaks_its_own_breaks_them)
{ Drawing drawing;
  CHECK_EQ(drawing.session().import_blob(5u, key), std::uint32_t{AV_EXECUTED});
  ContextSessionProbe probe(drawing.session());
  av::ImportedBlob *const dropped = probe.imports().drop(5u);
  CHECK(!drawing.session().invariants_hold());
  /* Back as the session would have it, so the rig releases it whole. */
  av::ImportedBlob *taken = nullptr;
  av::SourceMemory &memory = *dropped->give_back();
  CHECK_STATUS(probe.imports().take(5u, memory, taken), Status::ok);
  CHECK(drawing.session().invariants_hold()); }

/* Imports with no source are references nobody can let go of. */
TEST(context_session_invariants, imports_with_no_source_break_them)
{ Drawing drawing;
  CHECK_EQ(drawing.session().import_blob(5u, key), std::uint32_t{AV_EXECUTED});
  ContextSessionProbe probe(drawing.session());
  probe.drop_source();
  CHECK(!drawing.session().invariants_hold());
  drawing.session().draw_from(drawing.source(), domain);
  CHECK(drawing.session().invariants_hold()); }

/* A fault belongs to a host device; a session with none has nothing to be
 * faulted on, and the next context bound to the slot would inherit the fault. */
TEST(context_session_invariants, a_fault_with_no_backend_breaks_them)
{ ContextSession session;
  ContextSessionProbe(session).set_faulted(true);
  CHECK(!session.invariants_hold()); }

/* Imports with no backend are imports nobody can abandon. */
TEST(context_session_invariants, imports_with_no_backend_break_them)
{ Drawing drawing;
  CHECK_EQ(drawing.session().import_blob(5u, key), std::uint32_t{AV_EXECUTED});
  ContextSessionProbe probe(drawing.session());
  probe.drop_backend();
  CHECK(!drawing.session().invariants_hold());
  /* Given back by hand, since a session with no backend releases nothing. */
  av::ImportedBlob *const dropped = probe.imports().drop(5u);
  drawing.source().release(*dropped->give_back());
  CHECK(drawing.session().invariants_hold()); }

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

/* A session whose host device was lost has faulted: it refuses every
 * command, and the root shares and opens nothing through it (feature 053). */
TEST(context_session, says_when_a_lost_device_faulted_it)
{ Bound bound;
  CHECK(!bound.session().faulted());
  std::size_t answer = 0u;
  CHECK_EQ(bound.session().execute(MockSessionBackend::lose_id, nullptr, 0u, nullptr, 0u, answer),
           std::uint32_t{AV_DEVICE_LOST});
  CHECK(bound.session().faulted());
  bound.session().release();
  CHECK(!bound.session().faulted()); }

/* ContextSession::hold_clocks: the session tells its GPU's clock hold of every
 * command it runs. A hold the host could not open is told and does nothing, so
 * the session answers exactly as one with no hold - and a released session
 * forgets the hold, so a later one on the slot is not tied to it. */
TEST(context_session_hold_clocks, a_session_holding_clocks_runs_its_commands_as_before)
{ Bound bound;
  CHECK_STATUS(bound.status(), Status::ok);
  av::GpuClockHold clocks;
  CHECK(!clocks.open(-1));
  bound.session().hold_clocks(clocks);
  std::uint8_t reply[16] = {};
  std::size_t  answer = 0u;
  CHECK_EQ(bound.session().execute(MockSessionBackend::echo_id, payload, sizeof payload, reply, sizeof reply, answer),
           std::uint32_t{AV_EXECUTED});
  CHECK_EQ(answer, sizeof payload);
  CHECK_EQ(reply[7], payload[7]);
  CHECK_EQ(bound.backend().ran(), 1u);
  bound.session().release();
  CHECK(!bound.session().live());
  CHECK(bound.session().invariants_hold()); }

namespace {

void count_wake(void *context) { ++*static_cast<std::uint32_t *>(context); }

}  // namespace

/* The library's wake reaches the backend that will call it; a session with
 * no backend has nobody to give it to. */
TEST(context_session, a_signal_is_given_to_its_backend)
{ ContextSession unbound;
  std::uint32_t  woken = 0u;
  unbound.bind_signal(&count_wake, &woken);
  Bound bound;
  CHECK_STATUS(bound.status(), Status::ok);
  CHECK(!bound.backend().signal());
  bound.session().bind_signal(&count_wake, &woken);
  CHECK(bound.backend().signal());
  CHECK(bound.backend().signal());
  CHECK_EQ(woken, 2u); }

/* A device lost under an idle session - another session on it found the loss
 * - is answered at the next poll, and for good: the session refuses every
 * command after, as one a command of its own found lost. */
TEST(context_session_poll, a_device_lost_while_idle_is_lost_from_the_next_poll_on)
{ Bound bound;
  ContextSession &session = bound.session();
  CHECK_EQ(session.poll(), std::uint32_t{AV_POLL_IDLE});
  CHECK(!session.faulted());
  bound.backend().lose_elsewhere();
  CHECK_EQ(session.poll(), std::uint32_t{AV_POLL_LOST});
  CHECK(session.faulted());
  CHECK_EQ(session.poll(), std::uint32_t{AV_POLL_LOST});
  std::size_t answer = 0u;
  CHECK_EQ(session.execute(0x10001u, payload, sizeof payload, nullptr, 0u, answer), std::uint32_t{AV_DEVICE_LOST});
  CHECK(session.invariants_hold());
  /* A session with no backend has nothing to poll and nothing to lose. */
  ContextSession unbound;
  CHECK_EQ(unbound.poll(), std::uint32_t{AV_POLL_IDLE});
  CHECK_EQ(unbound.lose_device(), std::uint32_t{AV_UNKNOWN}); }

TEST(context_session_lose_device, the_backend_loses_its_host_device_where_the_host_can)
{ Bound bound;
  ContextSession &session = bound.session();
  bound.backend().set_losable(false);
  CHECK_EQ(session.lose_device(), std::uint32_t{AV_UNKNOWN});
  CHECK_EQ(session.poll(), std::uint32_t{AV_POLL_IDLE});
  bound.backend().set_losable(true);
  CHECK_EQ(session.lose_device(), std::uint32_t{AV_EXECUTED});
  CHECK_EQ(session.poll(), std::uint32_t{AV_POLL_LOST});
  CHECK(session.invariants_hold()); }

/* Docs/protocol.md, "Long commands": an answer longer than the device's reply
 * is kept, whole, until the next command, and answer_part hands it on from
 * any offset; an answer nobody asked for is not kept. */
TEST(context_session, an_answer_longer_than_the_capacity_is_kept_for_answer_part)
{ Bound bound;
  ContextSession &session = bound.session();
  std::uint8_t big[300];
  for (std::size_t index = 0u; index < sizeof big; index++) big[index] = static_cast<std::uint8_t>(index);
  std::uint8_t reply[16] = {};
  std::size_t  answer = 0u;
  CHECK_EQ(session.execute(MockSessionBackend::echo_id, big, sizeof big, reply, sizeof reply, answer),
           std::uint32_t{AV_EXECUTED});
  CHECK_EQ(answer, sizeof big);
  std::uint8_t  part[100] = {};
  std::uint64_t length = 0u;
  CHECK_EQ(session.answer_part(250u, part, sizeof part, length), 50u);
  CHECK_EQ(length, 300u);
  CHECK_EQ(part[0], 250u);
  CHECK_EQ(part[49], static_cast<std::uint8_t>(299u));
  CHECK_EQ(session.answer_part(0u, part, sizeof part, length), 100u);
  CHECK_EQ(part[99], 99u);
  CHECK_EQ(session.answer_part(300u, part, sizeof part, length), 0u);
  CHECK_EQ(session.answer_part(0u, nullptr, sizeof part, length), 0u);
  /* The next command drops it: one that fits keeps nothing. */
  CHECK_EQ(session.execute(MockSessionBackend::echo_id, big, 4u, reply, sizeof reply, answer),
           std::uint32_t{AV_EXECUTED});
  CHECK_EQ(session.answer_part(0u, part, sizeof part, length), 0u);
  CHECK_EQ(length, 0u);
  /* Nor is a long answer nobody asked for. */
  CHECK_EQ(session.execute(MockSessionBackend::echo_id, big, sizeof big, nullptr, 0u, answer),
           std::uint32_t{AV_EXECUTED});
  CHECK_EQ(answer, sizeof big);
  CHECK_EQ(session.answer_part(0u, part, sizeof part, length), 0u);
  CHECK_EQ(length, 0u);
  session.release();
  CHECK_EQ(session.answer_part(0u, part, sizeof part, length), 0u); }

/* A command that decodes into more than the block - a source of megabytes -
 * takes room past it for as long as it runs, and is not starved. */
TEST(context_session, a_command_needing_more_than_the_block_decodes_past_it)
{ Bound bound;
  ContextSession &session = bound.session();
  std::size_t answer = 0u;
  CHECK_EQ(session.execute(MockSessionBackend::roomy_id, payload, sizeof payload, nullptr, 0u, answer),
           std::uint32_t{AV_EXECUTED});
  CHECK_EQ(session.starved(), 0u);
  CHECK(session.invariants_hold()); }
