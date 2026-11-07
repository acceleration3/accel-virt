#include "core/log/driver_log.hpp"
#include "core/trace/state_writer.hpp"
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
#include "proto/submission/channel_counter.hpp"
#include "proto/submission/channel_stats_report.hpp"
#include "harness/executor/context_session_probe.hpp"
#include "harness/executor/imported_blob_probe.hpp"
#include "harness/executor/mock_session_backend.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
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
    Bound() : sink_(), lines_(sink_, "t: "), block_{}, session_(lines_)
    {
        status_ = session_.initialize(backend_, block_, sizeof block_);
    }
    av::Status          status() const { return status_; }
    MockSessionBackend &backend() { return backend_; }
    ContextSession     &session() { return session_; }
    std::uint8_t       *block() { return block_; }

private:
    av::RecordingLog   sink_;
    av::DriverLog      lines_;
    std::uint8_t       block_[block_bytes];
    MockSessionBackend backend_;
    ContextSession     session_;
    av::Status         status_ = av::Status::ok;
};

}

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
  for (const std::uint8_t byte : reply) CHECK_EQ(byte, std::uint8_t{0xAB}); }

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
  CHECK_EQ(backend.ran(), 1u);
  CHECK(session.invariants_hold()); }

TEST(context_session, a_session_with_no_backend_runs_nothing)
{ av::RecordingLog sink;
  av::DriverLog    lines(sink, "t: ");
  ContextSession session(lines);
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

}

TEST(context_session_import, memory_the_backend_takes_is_held_until_released)
{ Drawing drawing;
  CHECK_EQ(drawing.session().imports().count(), 0u);
  CHECK_EQ(drawing.session().import_blob(5u, key), std::uint32_t{AV_EXECUTED});
  CHECK_EQ(drawing.session().imports().count(), 1u);
  CHECK_EQ(drawing.backend().adopted(), 1u);
  CHECK(drawing.session().invariants_hold());

  drawing.session().release_blob(5u);
  CHECK_EQ(drawing.session().imports().count(), 0u);
  CHECK_EQ(drawing.backend().abandoned(), 1u);
  CHECK(drawing.session().invariants_hold()); }

TEST(context_session_import, an_import_keeps_memory_the_device_let_go_of)
{ Drawing drawing;
  CHECK_EQ(drawing.session().import_blob(5u, key), std::uint32_t{AV_EXECUTED});
  drawing.source().free(domain, key);
  CHECK_EQ(drawing.exporter().live(), 1u);
  CHECK_EQ(drawing.source().live(), 1u);
  drawing.session().release_blob(5u);
  CHECK_EQ(drawing.exporter().live(), 0u);
  CHECK_EQ(drawing.source().live(), 0u); }

TEST(context_session_import, answers_the_handle_the_guest_names_an_import_by)
{ Drawing drawing;
  CHECK_EQ(drawing.session().imported_handle(5u), 0u);
  CHECK_EQ(drawing.session().import_blob(5u, key), std::uint32_t{AV_EXECUTED});
  CHECK_EQ(drawing.session().import_blob(6u, key), std::uint32_t{AV_EXECUTED});
  CHECK_EQ(drawing.session().imported_handle(5u), 1u + MockSessionBackend::wire_offset);
  CHECK_EQ(drawing.session().imported_handle(6u), 2u + MockSessionBackend::wire_offset);
  CHECK_EQ(drawing.session().imported_handle(7u), 0u);
  CHECK(drawing.session().invariants_hold());

  drawing.session().release_blob(5u);
  CHECK_EQ(drawing.session().imported_handle(5u), 0u);
  CHECK_EQ(drawing.session().imported_handle(6u), 2u + MockSessionBackend::wire_offset);

  drawing.backend().set_adopts(false);
  CHECK_EQ(drawing.session().import_blob(8u, key), std::uint32_t{AV_UNDECODABLE});
  CHECK_EQ(drawing.session().imported_handle(8u), 0u);
  CHECK(drawing.session().invariants_hold()); }

TEST(context_session_import, releasing_one_it_never_took_is_nothing)
{ Drawing drawing;
  drawing.session().release_blob(5u);
  CHECK_EQ(drawing.backend().abandoned(), 0u);
  CHECK(drawing.session().invariants_hold()); }

TEST(context_session_import, the_same_id_twice_is_refused)
{ Drawing drawing;
  CHECK_EQ(drawing.session().import_blob(5u, key), std::uint32_t{AV_EXECUTED});
  CHECK_EQ(drawing.session().import_blob(5u, key), std::uint32_t{AV_UNDECODABLE});
  CHECK_EQ(drawing.session().imports().count(), 1u);
  CHECK_EQ(drawing.backend().adopted(), 1u);
  drawing.session().release_blob(5u);
  drawing.source().free(domain, key);
  CHECK_EQ(drawing.exporter().live(), 0u); }

TEST(context_session_import, a_key_with_no_memory_never_reaches_the_backend)
{ Drawing drawing;
  CHECK_EQ(drawing.session().import_blob(5u, key + 1u), std::uint32_t{AV_UNDECODABLE});
  AvHostMemory elsewhere = {-1, 0u, 0u};
  CHECK_STATUS(drawing.source().allocate(domain + 1u, key + 2u, 4096u, Drawing::upload(), elsewhere), Status::ok);
  CHECK_EQ(drawing.session().import_blob(5u, key + 2u), std::uint32_t{AV_UNDECODABLE});
  drawing.source().free(domain, key);
  CHECK_EQ(drawing.session().import_blob(5u, key), std::uint32_t{AV_UNDECODABLE});
  CHECK_EQ(drawing.backend().adopted(), 0u);
  CHECK_EQ(drawing.session().imports().count(), 0u);
  drawing.source().free(domain + 1u, key + 2u);
  close(elsewhere.fd); }

TEST(context_session_import, a_backend_that_will_not_take_it_costs_no_slot)
{ Drawing drawing;
  drawing.backend().set_adopts(false);
  CHECK_EQ(drawing.session().import_blob(5u, key), std::uint32_t{AV_UNDECODABLE});
  CHECK_EQ(drawing.session().imports().count(), 0u);
  drawing.backend().set_adopts(true);
  CHECK_EQ(drawing.session().import_blob(5u, key), std::uint32_t{AV_EXECUTED});
  CHECK_EQ(drawing.session().imports().count(), 1u);
  drawing.session().release_blob(5u);
  drawing.source().free(domain, key);
  CHECK_EQ(drawing.exporter().live(), 0u); }

TEST(context_session_import, imports_past_any_old_bound_are_all_held_and_given_back)
{ Drawing drawing;
  constexpr std::uint64_t many = 5000u;
  for (std::uint64_t id = 1u; id <= many; id++)
      CHECK_EQ(drawing.session().import_blob(id, key), std::uint32_t{AV_EXECUTED});
  CHECK_EQ(drawing.session().imports().count(), many);
  drawing.session().release();
  drawing.source().free(domain, key);
  CHECK_EQ(drawing.exporter().live(), 0u); }

TEST(context_session_import, releasing_the_session_gives_back_what_the_guest_did_not)
{ Drawing drawing;
  for (std::uint64_t id = 1u; id <= 3u; id++)
      CHECK_EQ(drawing.session().import_blob(id, key), std::uint32_t{AV_EXECUTED});
  CHECK_EQ(drawing.session().imports().count(), 3u);
  drawing.source().free(domain, key);
  CHECK_EQ(drawing.exporter().live(), 1u);
  drawing.session().release();
  CHECK_EQ(drawing.backend().abandoned(), 3u);
  CHECK_EQ(drawing.session().imports().count(), 0u);
  CHECK_EQ(drawing.exporter().live(), 0u);
  CHECK(drawing.session().invariants_hold()); }

TEST(context_session_release, shuts_its_backend_down_after_its_imports)
{ Drawing drawing;
  for (std::uint64_t id = 1u; id <= 2u; id++)
      CHECK_EQ(drawing.session().import_blob(id, key), std::uint32_t{AV_EXECUTED});
  CHECK_EQ(drawing.backend().shut_downs(), 0u);
  drawing.session().release();
  CHECK_EQ(drawing.backend().shut_downs(), 1u);
  CHECK_EQ(drawing.backend().abandoned_before_shut_down(), 2u);
  CHECK(drawing.session().invariants_hold());
  drawing.session().release();
  CHECK_EQ(drawing.backend().shut_downs(), 1u); }


TEST(context_session_import, a_session_with_no_backend_or_source_imports_nothing)
{ av::RecordingLog sink;
  av::DriverLog    lines(sink, "t: ");
  ContextSession session(lines);
  CHECK_EQ(session.import_blob(5u, key), std::uint32_t{AV_UNDECODABLE});
  CHECK_EQ(session.imports().count(), 0u);
  session.release_blob(5u);
  CHECK(session.invariants_hold());
  Bound bound;
  CHECK_EQ(bound.session().import_blob(5u, key), std::uint32_t{AV_UNDECODABLE});
  CHECK_EQ(bound.backend().adopted(), 0u); }

TEST(context_session_import, a_lost_device_imports_nothing)
{ Drawing drawing;
  std::size_t answer = 0u;
  CHECK_EQ(drawing.session().execute(MockSessionBackend::lose_id, payload, sizeof payload, nullptr, 0u, answer),
           std::uint32_t{AV_DEVICE_LOST});
  CHECK_EQ(drawing.session().import_blob(5u, key), std::uint32_t{AV_UNDECODABLE});
  CHECK_EQ(drawing.backend().adopted(), 0u);
  CHECK_EQ(drawing.session().imports().count(), 0u);
  CHECK(drawing.session().invariants_hold()); }

TEST(context_session_invariants, an_import_table_that_breaks_its_own_breaks_them)
{ Drawing drawing;
  CHECK_EQ(drawing.session().import_blob(5u, key), std::uint32_t{AV_EXECUTED});
  ContextSessionProbe probe(drawing.session());
  av::ImportedBlob *const held = probe.imports().find(5u);
  CHECK(held != nullptr);
  av::ImportedBlobProbe(*held).set_id(6u);
  CHECK(!drawing.session().invariants_hold());
  av::ImportedBlobProbe(*held).set_id(5u);
  CHECK(drawing.session().invariants_hold()); }

TEST(context_session_invariants, imports_with_no_source_break_them)
{ Drawing drawing;
  CHECK_EQ(drawing.session().import_blob(5u, key), std::uint32_t{AV_EXECUTED});
  ContextSessionProbe probe(drawing.session());
  probe.drop_source();
  CHECK(!drawing.session().invariants_hold());
  drawing.session().draw_from(drawing.source(), domain);
  CHECK(drawing.session().invariants_hold()); }

TEST(context_session_invariants, a_fault_with_no_backend_breaks_them)
{ av::RecordingLog sink;
  av::DriverLog    lines(sink, "t: ");
  ContextSession session(lines);
  ContextSessionProbe(session).set_faulted(true);
  CHECK(!session.invariants_hold()); }

TEST(context_session_invariants, imports_with_no_backend_break_them)
{ Drawing drawing;
  CHECK_EQ(drawing.session().import_blob(5u, key), std::uint32_t{AV_EXECUTED});
  ContextSessionProbe probe(drawing.session());
  probe.drop_backend();
  CHECK(!drawing.session().invariants_hold());
  av::SourceMemory *const dropped = probe.imports().forget(5u);
  CHECK(dropped != nullptr);
  drawing.source().release(*dropped);
  CHECK(drawing.session().invariants_hold()); }

TEST(context_session_invariants, an_arena_used_past_its_room_breaks_it)
{ Bound bound;
  CHECK(bound.session().invariants_hold());
  av::ContextSessionProbe session(bound.session());
  av::WireArenaProbe arena(session.arena());
  arena.set_used(block_bytes + 1u);
  CHECK(!bound.session().invariants_hold()); }

TEST(context_session, says_when_a_lost_device_faulted_it)
{ Bound bound;
  CHECK(!bound.session().faulted());
  std::size_t answer = 0u;
  CHECK_EQ(bound.session().execute(MockSessionBackend::lose_id, nullptr, 0u, nullptr, 0u, answer),
           std::uint32_t{AV_DEVICE_LOST});
  CHECK(bound.session().faulted());
  bound.session().release();
  CHECK(!bound.session().faulted()); }

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

}

TEST(context_session, a_signal_is_given_to_its_backend)
{ av::RecordingLog sink;
  av::DriverLog    lines(sink, "t: ");
  ContextSession unbound(lines);
  std::uint32_t  woken = 0u;
  unbound.bind_signal(&count_wake, &woken);
  Bound bound;
  CHECK_STATUS(bound.status(), Status::ok);
  CHECK(!bound.backend().signal());
  bound.session().bind_signal(&count_wake, &woken);
  CHECK(bound.backend().signal());
  CHECK(bound.backend().signal());
  CHECK_EQ(woken, 2u); }

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
  av::RecordingLog sink;
  av::DriverLog    lines(sink, "t: ");
  ContextSession unbound(lines);
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
  CHECK_EQ(session.execute(MockSessionBackend::echo_id, big, 4u, reply, sizeof reply, answer),
           std::uint32_t{AV_EXECUTED});
  CHECK_EQ(session.answer_part(0u, part, sizeof part, length), 0u);
  CHECK_EQ(length, 0u);
  CHECK_EQ(session.execute(MockSessionBackend::echo_id, big, sizeof big, nullptr, 0u, answer),
           std::uint32_t{AV_EXECUTED});
  CHECK_EQ(answer, sizeof big);
  CHECK_EQ(session.answer_part(0u, part, sizeof part, length), 0u);
  CHECK_EQ(length, 0u);
  session.release();
  CHECK_EQ(session.answer_part(0u, part, sizeof part, length), 0u); }

TEST(context_session, a_command_needing_more_than_the_block_decodes_past_it)
{ Bound bound;
  ContextSession &session = bound.session();
  std::size_t answer = 0u;
  CHECK_EQ(session.execute(MockSessionBackend::roomy_id, payload, sizeof payload, nullptr, 0u, answer),
           std::uint32_t{AV_EXECUTED});
  CHECK_EQ(session.starved(), 0u);
  CHECK(session.invariants_hold()); }

TEST(context_session, a_session_reports_its_channel_only_once_the_guest_has_sent_statistics)
{ Bound bound;
  ContextSession &session = bound.session();
  CHECK_STATUS(bound.status(), Status::ok);
  char            text[2048] = {};
  av::StateWriter before(text, sizeof text);
  session.report_channel(before);
  CHECK(std::strstr(before.dump(), "channel/") == nullptr);
  av::ChannelStatsReport report;
  report.set(av::ChannelCounter::split_transfers, 5u);
  report.set(av::ChannelCounter::ring_waits, 9u);
  std::uint8_t body[av::ChannelStatsReport::encoded_size] = {};
  CHECK_STATUS(report.encode(body, sizeof body), Status::ok);
  session.channel_stats(4u, body, sizeof body);
  char            after_text[2048] = {};
  av::StateWriter after(after_text, sizeof after_text);
  session.report_channel(after);
  CHECK(std::strstr(after.dump(), "channel/4 reports=1 rejected=0 split_transfers=5") != nullptr);
  CHECK(std::strstr(after.dump(), "ring_waits=9") != nullptr);
  CHECK(session.invariants_hold()); }

TEST(context_session, a_malformed_statistics_report_is_counted_as_rejected_in_the_channel_report)
{ Bound bound;
  ContextSession &session = bound.session();
  CHECK_STATUS(bound.status(), Status::ok);
  const std::uint8_t garbage[3] = {1u, 2u, 3u};
  session.channel_stats(4u, garbage, sizeof garbage);
  char            text[2048] = {};
  av::StateWriter writer(text, sizeof text);
  session.report_channel(writer);
  CHECK(std::strstr(writer.dump(), "reports=0 rejected=1") != nullptr); }

TEST(context_session_release, waits_for_the_gpu_before_any_blob_or_source_memory_goes)
{ Drawing drawing;
  for (std::uint64_t id = 1u; id <= 2u; id++)
      CHECK_EQ(drawing.session().import_blob(id, key), std::uint32_t{AV_EXECUTED});
  drawing.source().free(domain, key);
  CHECK_EQ(drawing.backend().drains(), 0u);
  drawing.session().release();
  CHECK_EQ(drawing.backend().drains(), 1u);
  CHECK_EQ(drawing.backend().abandoned_before_drain(), 0u);
  CHECK_EQ(drawing.backend().abandoned(), 2u);
  CHECK_EQ(drawing.exporter().live(), 0u);
  drawing.session().release();
  CHECK_EQ(drawing.backend().drains(), 1u); }
