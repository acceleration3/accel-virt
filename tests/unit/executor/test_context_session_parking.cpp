#include "harness/executor/generous_budget.hpp"
#include "core/log/driver_log.hpp"
#include "device/abi/av_executor.h"
#include "executor/context_session.hpp"
#include "executor/host_memory_source.hpp"
#include "executor/shim/host_heap.hpp"
#include "harness/executor/mock_memory_exporter.hpp"
#include "harness/executor/mock_session_backend.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "proto/resources/host_memory_request.hpp"
#include "proto/submission/blob_role.hpp"
#include "proto/wire/guest_bytes.hpp"

#include <cstdint>
#include <unistd.h>

using av::BlobRole;
using av::ContextSession;
using av::MockSessionBackend;
using av::Status;

namespace av {

struct ContextSessionProbe {
    static void park(ContextSession &session, std::uint64_t blob_id)
    {
        session.parked_[session.parked_count_++] = ContextSession::Parked{blob_id, 1u};
    }
};

}

using av::ContextSessionProbe;

namespace {

constexpr std::uint32_t domain = 3u;
constexpr std::uint64_t first_key = 0x10u;
constexpr std::uint32_t keys = ContextSession::parked_most + 2u;

BlobRole arena_at(std::uint32_t slot)
{
    return BlobRole(BlobRole::Kind::arena_segment, std::uint64_t{slot} << av::GuestBytes::slot_shift);
}

class Parking {
public:
    Parking() : sink_(), lines_(sink_, "t: "), exporter_(), heap_(), source_(exporter_, heap_), session_(lines_), block_{}, memory_{}
    {
        exporter_.add_type(av::MockMemoryExporter::host_visible | av::MockMemoryExporter::host_coherent, true);
        CHECK_STATUS(session_.initialize(backend_, block_, sizeof block_), Status::ok);
        session_.draw_from(source_, domain);
        const av::HostMemoryRequest upload(av::HostMemoryRequest::Kind::upload, 0u);
        for (std::uint32_t at = 0u; at < keys; at++) {
            memory_[at] = AvHostMemory{-1, 0u, 0u};
            static_cast<void>(source_.allocate(domain, first_key + at, 4096u, upload, memory_[at]));
        }
    }
    ~Parking()
    {
        static_cast<void>(session_.release(av::GenerousBudget().budget())); session_.vacate();
        for (const AvHostMemory &made : memory_)
            if (made.fd >= 0) close(made.fd);
    }

    av::RecordingLog   &log() { return sink_; }
    ContextSession     &session() { return session_; }
    MockSessionBackend &backend() { return backend_; }

private:
    av::RecordingLog        sink_;
    av::DriverLog           lines_;
    av::MockMemoryExporter  exporter_;
    av::HostHeap            heap_;
    av::HostMemorySource    source_;
    MockSessionBackend      backend_;
    ContextSession          session_;
    std::uint8_t            block_[4096];
    AvHostMemory            memory_[keys];
};

}

TEST(context_session_parking, an_arena_segment_released_over_idle_gpu_work_is_retired_at_once)
{ Parking parking;
  CHECK_EQ(parking.session().import_blob(5u, first_key, arena_at(1u)), std::uint32_t{AV_EXECUTED});
  parking.backend().set_progress(4u, 4u);
  parking.session().release_blob(5u);
  CHECK_EQ(parking.session().parked(), 0u);
  CHECK_EQ(parking.backend().abandoned(), 1u);
  CHECK_EQ(parking.session().imports().count(), 0u);
  CHECK(parking.session().invariants_hold()); }

TEST(context_session_parking, an_arena_segment_released_under_pending_gpu_work_waits_for_the_mark)
{ Parking parking;
  CHECK_EQ(parking.session().import_blob(5u, first_key, arena_at(1u)), std::uint32_t{AV_EXECUTED});
  parking.backend().set_progress(7u, 3u);
  parking.session().release_blob(5u);
  CHECK_EQ(parking.session().parked(), 1u);
  CHECK_EQ(parking.backend().abandoned(), 0u);
  CHECK_EQ(parking.session().imports().count(), 1u);
  CHECK(parking.session().invariants_hold());
  parking.backend().set_progress(9u, 6u);
  parking.session().end_batch();
  CHECK_EQ(parking.session().parked(), 1u);
  CHECK_EQ(parking.backend().abandoned(), 0u);
  parking.backend().set_progress(9u, 7u);
  CHECK_EQ(parking.session().poll(), std::uint32_t{AV_POLL_IDLE});
  CHECK_EQ(parking.session().parked(), 0u);
  CHECK_EQ(parking.backend().abandoned(), 1u);
  CHECK_EQ(parking.session().imports().count(), 0u);
  CHECK(parking.session().invariants_hold()); }

TEST(context_session_parking, an_end_of_batch_also_retires_what_the_gpu_has_passed)
{ Parking parking;
  CHECK_EQ(parking.session().import_blob(5u, first_key, arena_at(1u)), std::uint32_t{AV_EXECUTED});
  parking.backend().set_progress(2u, 1u);
  parking.session().release_blob(5u);
  parking.backend().set_progress(2u, 2u);
  parking.session().end_batch();
  CHECK_EQ(parking.session().parked(), 0u);
  CHECK_EQ(parking.backend().abandoned(), 1u); }

TEST(context_session_parking, a_plain_import_is_never_parked)
{ Parking parking;
  CHECK_EQ(parking.session().import_blob(5u, first_key), std::uint32_t{AV_EXECUTED});
  parking.backend().set_progress(7u, 3u);
  parking.session().release_blob(5u);
  CHECK_EQ(parking.session().parked(), 0u);
  CHECK_EQ(parking.backend().abandoned(), 1u); }

TEST(context_session_parking, releasing_a_parked_segment_again_parks_it_no_further)
{ Parking parking;
  CHECK_EQ(parking.session().import_blob(5u, first_key, arena_at(1u)), std::uint32_t{AV_EXECUTED});
  parking.backend().set_progress(7u, 3u);
  parking.session().release_blob(5u);
  parking.session().release_blob(5u);
  CHECK_EQ(parking.session().parked(), 1u);
  parking.backend().set_progress(7u, 7u);
  parking.session().end_batch();
  CHECK_EQ(parking.backend().abandoned(), 1u);
  parking.session().release_blob(5u);
  CHECK_EQ(parking.backend().abandoned(), 1u); }

TEST(context_session_parking, only_so_many_segments_wait_and_the_rest_retire)
{ Parking parking;
  parking.backend().set_progress(100u, 1u);
  for (std::uint32_t at = 0u; at < ContextSession::parked_most + 1u; at++) {
      CHECK_EQ(parking.session().import_blob(at + 1u, first_key + at, arena_at(at + 1u)), std::uint32_t{AV_EXECUTED});
      parking.session().release_blob(at + 1u);
  }
  CHECK_EQ(parking.session().parked(), ContextSession::parked_most);
  CHECK_EQ(parking.backend().abandoned(), 1u);
  CHECK(parking.session().invariants_hold()); }

TEST(context_session_parking, a_teardown_wide_enough_for_every_segment_a_channel_holds_parks_all_of_them)
{ Parking parking;
  parking.backend().set_progress(100u, 1u);
  for (std::uint32_t at = 0u; at < av::ClientBlobArena::max_segments + av::ClientBlobArena::max_regions + 1u; at++) {
      CHECK_EQ(parking.session().import_blob(at + 1u, first_key + at, arena_at(at + 1u)), std::uint32_t{AV_EXECUTED});
      parking.session().release_blob(at + 1u);
  }
  CHECK_EQ(parking.session().parked(), ContextSession::parked_most);
  CHECK_EQ(parking.backend().abandoned(), 0u);
  CHECK(parking.session().invariants_hold()); }

TEST(context_session_parking, an_arena_segment_whose_handle_is_taken_is_unbound_and_retired_not_abandoned_plain)
{ Parking parking;
  parking.backend().set_one_handle(true);
  CHECK_EQ(parking.session().import_blob(1u, first_key), std::uint32_t{AV_EXECUTED});
  CHECK_EQ(parking.session().import_blob(2u, first_key + 1u, arena_at(1u)), std::uint32_t{AV_UNDECODABLE});
  CHECK_EQ(parking.backend().unbound(), 1u);
  CHECK_EQ(parking.backend().unbound_when_retired(), 1u);
  CHECK_EQ(parking.backend().abandoned(), 1u);
  CHECK_EQ(parking.session().imported_handle(2u), 0u);
  CHECK(parking.session().invariants_hold()); }

TEST(context_session_parking, releasing_the_session_retires_what_still_waits)
{ Parking parking;
  CHECK_EQ(parking.session().import_blob(5u, first_key, arena_at(1u)), std::uint32_t{AV_EXECUTED});
  parking.backend().set_progress(7u, 3u);
  parking.session().release_blob(5u);
  CHECK_EQ(parking.session().parked(), 1u);
  static_cast<void>(parking.session().release(av::GenerousBudget().budget())); parking.session().vacate();
  CHECK_EQ(parking.session().parked(), 0u);
  CHECK_EQ(parking.backend().abandoned(), 1u);
  CHECK(parking.session().invariants_hold()); }

TEST(context_session_parking, a_released_segment_stops_resolving_at_once_while_its_memory_waits_for_the_gpu)
{ Parking parking;
  CHECK_EQ(parking.session().import_blob(5u, first_key, arena_at(1u)), std::uint32_t{AV_EXECUTED});
  parking.backend().set_progress(7u, 3u);
  CHECK_EQ(parking.backend().unbound(), 0u);
  parking.session().release_blob(5u);
  CHECK_EQ(parking.backend().unbound(), 1u);
  CHECK_EQ(parking.backend().abandoned(), 0u);
  CHECK_EQ(parking.session().parked(), 1u);
  parking.backend().set_progress(7u, 7u);
  parking.session().end_batch();
  CHECK_EQ(parking.backend().abandoned(), 1u);
  CHECK(parking.backend().unbound_when_retired() >= 1u);
  CHECK(parking.session().invariants_hold()); }

TEST(context_session_parking, a_segment_released_over_idle_gpu_work_is_unbound_before_it_is_retired)
{ Parking parking;
  CHECK_EQ(parking.session().import_blob(5u, first_key, arena_at(1u)), std::uint32_t{AV_EXECUTED});
  parking.backend().set_progress(4u, 4u);
  parking.session().release_blob(5u);
  CHECK(parking.backend().unbound_when_retired() >= 1u);
  CHECK_EQ(parking.backend().abandoned(), 1u); }

TEST(context_session_parking, a_plain_import_is_never_unbound_as_an_arena_segment)
{ Parking parking;
  CHECK_EQ(parking.session().import_blob(5u, first_key), std::uint32_t{AV_EXECUTED});
  parking.session().release_blob(5u);
  CHECK_EQ(parking.backend().unbound(), 0u); }

TEST(context_session_parking, a_backend_that_will_not_adopt_a_blob_is_logged_once_with_the_blob_and_its_status)
{ Parking parking;
  parking.backend().set_adopts(false);
  CHECK_EQ(parking.session().import_blob(5u, first_key, arena_at(1u)), std::uint32_t{AV_UNDECODABLE});
  CHECK_EQ(parking.log().lines(), 1u);
  CHECK_EQ(parking.log().holding("would not adopt blob 5 as the arena segment at 67108864"), 1u);
  CHECK(parking.session().invariants_hold()); }

TEST(context_session_parking, the_invariant_fails_for_a_parked_id_that_is_no_import)
{ Parking parking;
  CHECK_EQ(parking.session().import_blob(5u, first_key, arena_at(1u)), std::uint32_t{AV_EXECUTED});
  CHECK(parking.session().invariants_hold());
  ContextSessionProbe::park(parking.session(), 99u);
  CHECK(!parking.session().invariants_hold()); }

TEST(context_session_parking, the_invariant_fails_for_a_parked_import_that_is_not_an_arena_segment)
{ Parking parking;
  CHECK_EQ(parking.session().import_blob(5u, first_key), std::uint32_t{AV_EXECUTED});
  ContextSessionProbe::park(parking.session(), 5u);
  CHECK(!parking.session().invariants_hold()); }
