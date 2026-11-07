#include "core/support/status.hpp"
#include "device/fence/device_pulse.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/fence/fence_reporter.hpp"
#include "device/recording/wire_recorder.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "device/fence/fence_window.hpp"
#include "device/memory/blob_window.hpp"
#include "device/threading/doorbell.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/recording/recorded_file.hpp"
#include "proto/submission/fault_code.hpp"
#include "proto/submission/fence_page.hpp"

#include <cstdint>
#include <cstring>

using av::BlobWindow;
using av::ByteLayout;
using av::FaultCode;
using av::FenceInterrupts;
using av::FencePage;
using av::FenceReporter;
using av::WireRecorder;
using av::FenceWindow;
using av::HostViolation;
using av::MockHost;
using av::Status;

namespace av {

struct FenceReporterProbe {
    static void pend_unprepared(FenceReporter &reporter) { reporter.pending_ = true; }
    static void lose_progress(FenceReporter &reporter) { reporter.completed_ = 5u; reporter.progress_ = 0u; }
    static void lose_page(FenceReporter &reporter) { reporter.page_ = FenceWindow(); }
    static void raise_past_progress(FenceReporter &reporter) { reporter.raised_ = reporter.progress_ + 5u; }
    static void fault_without_code(FenceReporter &reporter) { reporter.faulted_ = true; reporter.fault_ = FaultCode::none; }
    static void fault_loudly(FenceReporter &reporter)
    {
        reporter.faulted_ = true;
        reporter.fault_ = FaultCode::device_lost;
        reporter.quiet_ = false;
    }
    static void open_unprepared(FenceReporter &reporter) { reporter.quiet_ = false; }
};

}

namespace {

constexpr std::uint32_t line = 6u;

class Fixture {
public:
    Fixture() : bytes_{}, pointer_(bytes_), length_(sizeof bytes_), interrupts_(host_), recorder_(host_), reporter_(host_, interrupts_, recorder_, 3u)
    {
        std::memset(bytes_, 0xEE, sizeof bytes_);
        static_cast<void>(interrupts_.initialize());
        static_cast<void>(interrupts_.connect());
    }

    MockHost      &host() { return host_; }
    FenceReporter &reporter() { return reporter_; }
    WireRecorder  &recorder() { return recorder_; }
    FenceWindow    window() const { return FenceWindow(BlobWindow(&pointer_, &length_, 1u, length_)); }
    std::uint8_t  *bytes() { return bytes_; }
    FencePage      page() const
    {
        FencePage page;
        static_cast<void>(page.decode(bytes_, FencePage::encoded_size));
        return page;
    }

    template <typename Body>
    void on_worker(Body body)
    {
        struct Call {
            FenceReporter *reporter;
            Body          *body;
        } call{&reporter_, &body};
        void *thread = nullptr;
        CHECK_STATUS(host_.start_thread([](void *context) {
                         auto *const c = static_cast<Call *>(context);
                         (*c->body)(*c->reporter);
                     },
                                        &call, thread),
                     Status::ok);
        host_.join_thread(thread);
        CHECK(reporter_.invariants_hold());
    }

private:
    std::uint8_t    bytes_[64];
    void           *pointer_;
    std::uint64_t   length_;
    MockHost        host_;
    FenceInterrupts interrupts_;
    WireRecorder    recorder_;
    FenceReporter   reporter_;
};

}

TEST(fence_reporter_prepare, writes_a_fresh_page)
{ Fixture fixture;
  CHECK_STATUS(fixture.reporter().prepare(fixture.window(), line), Status::ok);
  CHECK(fixture.reporter().invariants_hold());
  const ByteLayout layout(fixture.bytes(), FencePage::encoded_size);
  CHECK(layout.at(0u, 4u) == 0x00465641u);
  CHECK(layout.at(4u, 4u) == 0u);
  CHECK(layout.at(8u, 8u) == 0u);
  CHECK(layout.at(16u, 8u) == 0u);
  CHECK(layout.at(24u, 8u) == 0u);
  CHECK(layout.at(32u, 8u) == 0u);
  CHECK(layout.at(FencePage::beat_at, 8u) == 0u);
  CHECK_EQ(fixture.bytes()[FencePage::encoded_size], 0xEEu);
  CHECK_STATUS(fixture.reporter().prepare(fixture.window(), line), Status::invalid_state); }

TEST(fence_reporter_prepare, a_blob_too_small_for_a_page_or_a_line_past_the_queues_writes_nothing)
{ Fixture fixture;
  void               *pointer = fixture.bytes();
  const std::uint64_t length = 31u;
  CHECK_STATUS(fixture.reporter().prepare(FenceWindow(BlobWindow(&pointer, &length, 1u, length)), line),
               Status::invalid_parameter);
  CHECK_STATUS(fixture.reporter().prepare(fixture.window(), av::Doorbell::pages), Status::invalid_parameter);
  CHECK_EQ(fixture.bytes()[0], 0xEEu);
  CHECK(!FenceWindow().invariants_hold());
  CHECK_STATUS(FenceWindow().publish(FencePage()), Status::invalid_parameter);
  CHECK_STATUS(FenceWindow().publish_beat(1u), Status::invalid_parameter);
  void               *pointer_short = fixture.bytes();
  const std::uint64_t short_length = FencePage::beat_at;
  const FenceWindow   no_beat(BlobWindow(&pointer_short, &short_length, 1u, short_length));
  CHECK_STATUS(no_beat.publish(FencePage(1u, 1u)), Status::invalid_parameter);
  CHECK_STATUS(no_beat.publish_beat(1u), Status::invalid_parameter);
  CHECK_EQ(fixture.bytes()[0], 0xEEu); }

TEST(fence_reporter_complete, each_completion_counts_on_the_device_pulse_it_beats_by)
{ Fixture fixture;
  av::DevicePulse pulse(fixture.host());
  CHECK_EQ(fixture.reporter().device_completions(), std::uint64_t{0});
  CHECK_STATUS(fixture.reporter().prepare(fixture.window(), line), Status::ok);
  fixture.reporter().beat_by(pulse);
  fixture.on_worker([](FenceReporter &reporter) {
      reporter.complete(1u);
      reporter.complete(4u);
  });
  CHECK_EQ(pulse.completions(), std::uint64_t{2});
  CHECK_EQ(fixture.reporter().device_completions(), std::uint64_t{2}); }

TEST(fence_reporter_complete, publishes_each_completion_and_signals_once)
{ Fixture fixture;
  CHECK_STATUS(fixture.reporter().prepare(fixture.window(), line), Status::ok);
  fixture.on_worker([](FenceReporter &reporter) {
      reporter.complete(1u);
      reporter.complete(4u);
      reporter.complete(9u);
      reporter.signal();
      reporter.signal();
  });
  CHECK_EQ(fixture.page().completed_fence(), 9u);
  CHECK_EQ(fixture.page().progress(), 3u);
  CHECK(!fixture.page().faulted());
  CHECK_EQ(fixture.reporter().completed(), 9u);
  CHECK_EQ(fixture.reporter().progress(), 3u);
  CHECK_EQ(fixture.host().raised(line), 1u);
  fixture.on_worker([](FenceReporter &reporter) { reporter.signal(); });
  CHECK_EQ(fixture.host().raised(line), 1u); }

TEST(fence_reporter_complete, a_page_the_guest_scribbled_on_is_rewritten_from_the_device_counters)
{ Fixture fixture;
  CHECK_STATUS(fixture.reporter().prepare(fixture.window(), line), Status::ok);
  fixture.on_worker([](FenceReporter &reporter) { reporter.complete(2u); });
  std::memset(fixture.bytes(), 0xFF, 32u);
  fixture.on_worker([](FenceReporter &reporter) { reporter.complete(3u); });
  CHECK_EQ(fixture.page().completed_fence(), 3u);
  CHECK_EQ(fixture.page().progress(), 2u); }

TEST(fence_reporter_complete, an_unprepared_reporter_writes_and_raises_nothing)
{ Fixture fixture;
  fixture.on_worker([](FenceReporter &reporter) {
      reporter.complete(1u);
      reporter.signal();
  });
  CHECK_EQ(fixture.bytes()[0], 0xEEu);
  CHECK_EQ(fixture.host().raised(line), 0u);
  CHECK_STATUS(fixture.reporter().prepare(fixture.window(), line), Status::ok);
  fixture.reporter().withdraw();
  CHECK(fixture.reporter().invariants_hold());
  fixture.on_worker([](FenceReporter &reporter) { reporter.complete(1u); });
  CHECK_EQ(fixture.page().completed_fence(), 0u);
  CHECK_STATUS(fixture.reporter().prepare(fixture.window(), line), Status::ok); }

TEST(fence_reporter_fault, publishes_the_code_with_the_last_completion_and_signals_once)
{ Fixture fixture;
  CHECK_STATUS(fixture.reporter().prepare(fixture.window(), line), Status::ok);
  fixture.on_worker([](FenceReporter &reporter) {
      reporter.complete(4u);
      reporter.signal();
      reporter.heartbeat();
      reporter.fault(FaultCode::undecodable);
      reporter.signal();
  });
  CHECK(fixture.reporter().faulted());
  CHECK(fixture.page().faulted());
  CHECK_EQ(fixture.page().fault_code(), static_cast<std::uint32_t>(FaultCode::undecodable));
  CHECK_EQ(fixture.page().completed_fence(), 4u);
  CHECK_EQ(fixture.page().progress(), 2u);
  CHECK_EQ(fixture.host().raised(line), 2u); }

TEST(fence_reporter_fault, a_faulted_context_reports_nothing_more)
{ Fixture fixture;
  CHECK_STATUS(fixture.reporter().prepare(fixture.window(), line), Status::ok);
  fixture.on_worker([](FenceReporter &reporter) {
      reporter.fault(FaultCode::ring_tail);
      reporter.signal();
      reporter.complete(9u);
      reporter.heartbeat();
      reporter.fault(FaultCode::device_lost);
      reporter.signal();
  });
  CHECK_EQ(fixture.page().fault_code(), static_cast<std::uint32_t>(FaultCode::ring_tail));
  CHECK_EQ(fixture.page().completed_fence(), 0u);
  CHECK_EQ(fixture.page().progress(), 0u);
  CHECK_EQ(fixture.host().raised(line), 1u);
  CHECK_STATUS(fixture.reporter().prepare(fixture.window(), line), Status::invalid_state); }

TEST(fence_reporter_beat, moves_the_beat_word_alone_and_the_worker_never_writes_it)
{ Fixture fixture;
  CHECK_STATUS(fixture.reporter().prepare(fixture.window(), line), Status::ok);
  fixture.reporter().beat();
  fixture.reporter().beat();
  CHECK_EQ(fixture.page().beat(), 2u);
  CHECK_EQ(fixture.reporter().beats(), 2u);
  CHECK_EQ(fixture.page().progress(), 0u);
  CHECK_EQ(fixture.host().raised(line), 0u);
  std::memset(fixture.bytes() + FencePage::beat_at, 0x77, 8u);
  fixture.on_worker([](FenceReporter &reporter) {
      reporter.complete(3u);
      reporter.heartbeat();
  });
  const ByteLayout layout(fixture.bytes(), FencePage::encoded_size);
  CHECK(layout.at(FencePage::beat_at, 8u) == 0x7777777777777777u);
  CHECK_EQ(fixture.page().completed_fence(), 3u);
  fixture.reporter().beat();
  CHECK_EQ(fixture.page().beat(), 3u);
  CHECK(fixture.reporter().invariants_hold()); }

TEST(fence_reporter_beat, an_unprepared_withdrawn_or_faulted_page_is_not_beaten)
{ Fixture fixture;
  fixture.reporter().beat();
  CHECK_EQ(fixture.bytes()[FencePage::beat_at], 0xEEu);
  CHECK_EQ(fixture.reporter().beats(), 0u);
  CHECK_STATUS(fixture.reporter().prepare(fixture.window(), line), Status::ok);
  fixture.reporter().beat();
  fixture.reporter().withdraw();
  fixture.reporter().beat();
  CHECK_EQ(fixture.page().beat(), 1u);
  CHECK(fixture.reporter().invariants_hold());
  CHECK_STATUS(fixture.reporter().prepare(fixture.window(), line), Status::ok);
  CHECK_EQ(fixture.page().beat(), 0u);
  fixture.reporter().beat();
  fixture.on_worker([](FenceReporter &reporter) { reporter.fault(FaultCode::device_lost); });
  fixture.reporter().beat();
  CHECK_EQ(fixture.page().beat(), 1u);
  CHECK(fixture.page().faulted()); }

TEST(fence_reporter_at_work, says_what_the_worker_last_said_until_a_fault)
{ Fixture fixture;
  CHECK(!fixture.reporter().working());
  CHECK_STATUS(fixture.reporter().prepare(fixture.window(), line), Status::ok);
  fixture.on_worker([](FenceReporter &reporter) { reporter.at_work(true); });
  CHECK(fixture.reporter().working());
  fixture.on_worker([](FenceReporter &reporter) { reporter.at_work(false); });
  CHECK(!fixture.reporter().working());
  fixture.on_worker([](FenceReporter &reporter) {
      reporter.at_work(true);
      reporter.fault(FaultCode::device_lost);
  });
  CHECK(!fixture.reporter().working()); }

TEST(fence_reporter_heartbeat, moves_progress_without_an_interrupt)
{ Fixture fixture;
  CHECK_STATUS(fixture.reporter().prepare(fixture.window(), line), Status::ok);
  fixture.on_worker([](FenceReporter &reporter) {
      reporter.heartbeat();
      reporter.heartbeat();
      reporter.signal();
  });
  CHECK_EQ(fixture.page().progress(), 2u);
  CHECK_EQ(fixture.page().completed_fence(), 0u);
  CHECK(!fixture.page().faulted());
  CHECK_EQ(fixture.host().raised(line), 0u); }

TEST(fence_reporter_fault, an_unprepared_reporter_writes_nothing_and_preparing_starts_healthy)
{ Fixture fixture;
  fixture.on_worker([](FenceReporter &reporter) {
      reporter.fault(FaultCode::no_executor);
      reporter.heartbeat();
      reporter.signal();
  });
  CHECK(!fixture.reporter().faulted());
  CHECK_EQ(fixture.bytes()[0], 0xEEu);
  CHECK_STATUS(fixture.reporter().prepare(fixture.window(), line), Status::ok);
  CHECK(!fixture.page().faulted()); }

TEST(fence_reporter_raise, raises_on_progress_alone_with_the_completion_where_it_was)
{ Fixture fixture;
  CHECK_STATUS(fixture.reporter().prepare(fixture.window(), line), Status::ok);
  fixture.on_worker([](FenceReporter &reporter) {
      reporter.complete(7u);
      reporter.signal();
      reporter.raise();
      reporter.raise();
      reporter.signal();
      reporter.signal();
  });
  CHECK_EQ(fixture.page().completed_fence(), 7u);
  CHECK_EQ(fixture.page().progress(), 3u);
  CHECK(!fixture.page().faulted());
  CHECK_EQ(fixture.reporter().completed(), 7u);
  CHECK_EQ(fixture.reporter().progress(), 3u);
  CHECK_EQ(fixture.page().raised(), 2u);
  CHECK_EQ(fixture.reporter().raised(), 2u);
  CHECK_EQ(fixture.host().raised(line), 2u); }

TEST(fence_reporter_raise, the_pages_after_a_raise_keep_its_count)
{ Fixture fixture;
  CHECK_STATUS(fixture.reporter().prepare(fixture.window(), line), Status::ok);
  fixture.on_worker([](FenceReporter &reporter) {
      reporter.raise();
      reporter.complete(1u);
      reporter.heartbeat();
  });
  CHECK_EQ(fixture.page().raised(), 1u);
  CHECK_EQ(fixture.page().completed_fence(), 1u);
  CHECK_EQ(fixture.page().progress(), 3u);
  fixture.on_worker([](FenceReporter &reporter) { reporter.fault(FaultCode::device_lost); });
  CHECK(fixture.page().faulted());
  CHECK_EQ(fixture.page().raised(), 1u); }

TEST(fence_reporter_raise, wakes_a_context_that_has_completed_nothing)
{ Fixture fixture;
  CHECK_STATUS(fixture.reporter().prepare(fixture.window(), line), Status::ok);
  fixture.on_worker([](FenceReporter &reporter) {
      reporter.raise();
      reporter.signal();
  });
  CHECK_EQ(fixture.page().completed_fence(), 0u);
  CHECK_EQ(fixture.page().progress(), 1u);
  CHECK_EQ(fixture.host().raised(line), 1u); }

TEST(fence_reporter_raise, an_unprepared_or_faulted_reporter_writes_and_raises_nothing)
{ Fixture fixture;
  fixture.on_worker([](FenceReporter &reporter) {
      reporter.raise();
      reporter.signal();
  });
  CHECK_EQ(fixture.bytes()[0], 0xEEu);
  CHECK_EQ(fixture.host().raised(line), 0u);
  CHECK_STATUS(fixture.reporter().prepare(fixture.window(), line), Status::ok);
  fixture.on_worker([](FenceReporter &reporter) {
      reporter.raise();
      reporter.fault(FaultCode::device_lost);
      reporter.signal();
      reporter.raise();
      reporter.signal();
  });
  CHECK_EQ(fixture.page().progress(), 1u);
  CHECK_EQ(fixture.page().fault_code(), static_cast<std::uint32_t>(FaultCode::device_lost));
  CHECK_EQ(fixture.reporter().progress(), 1u);
  CHECK_EQ(fixture.host().raised(line), 1u); }

TEST(fence_reporter, prepares_on_the_main_loop_and_reports_on_a_worker)
{ Fixture fixture;
  fixture.reporter().complete(1u);
  fixture.reporter().signal();
  fixture.reporter().heartbeat();
  fixture.reporter().raise();
  fixture.reporter().fault(FaultCode::ring_header);
  for (int count = 0; count < 5; count++) CHECK(fixture.host().claim(HostViolation::worker_required));
  fixture.on_worker([](FenceReporter &reporter) {
      reporter.withdraw();
  });
  CHECK(fixture.host().claim(HostViolation::main_loop_required)); }

TEST(fence_reporter_invariants, pending_before_preparing_or_a_completion_without_progress_is_detected)
{ Fixture pending;
  av::FenceReporterProbe::pend_unprepared(pending.reporter());
  CHECK(!pending.reporter().invariants_hold());
  Fixture lost;
  CHECK_STATUS(lost.reporter().prepare(lost.window(), line), Status::ok);
  av::FenceReporterProbe::lose_progress(lost.reporter());
  CHECK(!lost.reporter().invariants_hold());
  Fixture raised;
  CHECK_STATUS(raised.reporter().prepare(raised.window(), line), Status::ok);
  av::FenceReporterProbe::raise_past_progress(raised.reporter());
  CHECK(!raised.reporter().invariants_hold());
  Fixture pageless;
  CHECK_STATUS(pageless.reporter().prepare(pageless.window(), line), Status::ok);
  av::FenceReporterProbe::lose_page(pageless.reporter());
  CHECK(!pageless.reporter().invariants_hold());
  Fixture codeless;
  CHECK_STATUS(codeless.reporter().prepare(codeless.window(), line), Status::ok);
  av::FenceReporterProbe::fault_without_code(codeless.reporter());
  CHECK(!codeless.reporter().invariants_hold());
  Fixture unprepared;
  av::FenceReporterProbe::fault_without_code(unprepared.reporter());
  CHECK(!unprepared.reporter().invariants_hold());
  Fixture loud;
  CHECK_STATUS(loud.reporter().prepare(loud.window(), line), Status::ok);
  av::FenceReporterProbe::fault_loudly(loud.reporter());
  CHECK(!loud.reporter().invariants_hold());
  Fixture open;
  av::FenceReporterProbe::open_unprepared(open.reporter());
  CHECK(!open.reporter().invariants_hold()); }

TEST(fence_reporter_complete, records_the_fence_it_told_the_guest_about)
{ Fixture fixture;
  CHECK_STATUS(fixture.recorder().open("fences.av", 0u, "fence", av::AtCeiling::stop,
                                       av::GpuIdentity(), av::ProtocolVersion(1u, 1u)),
               Status::ok);
  CHECK_STATUS(fixture.reporter().prepare(fixture.window(), line), Status::ok);
  fixture.on_worker([](FenceReporter &reporter) {
      reporter.complete(4u);
      reporter.heartbeat();
      reporter.complete(9u);
  });
  CHECK(fixture.reporter().invariants_hold());
  av::RecordedFile   file(fixture.host(), "fences.av");
  av::RecordHeader   record;
  CHECK(file.expect(av::RecordKind::fence_completed, record) != nullptr);
  CHECK(record.fence() == 4u);
  CHECK_EQ(record.context(), 3u);
  CHECK_EQ(record.length(), 0u);
  CHECK(file.expect(av::RecordKind::fence_completed, record) != nullptr);
  CHECK(record.fence() == 9u);
  CHECK_EQ(file.remaining(), 0u);
  fixture.recorder().close(); }

TEST(fence_reporter_complete, a_faulted_context_completes_nothing_and_records_nothing)
{ Fixture fixture;
  CHECK_STATUS(fixture.recorder().open("fences.av", 0u, "fence", av::AtCeiling::stop,
                                       av::GpuIdentity(), av::ProtocolVersion(1u, 1u)),
               Status::ok);
  CHECK_STATUS(fixture.reporter().prepare(fixture.window(), line), Status::ok);
  fixture.on_worker([](FenceReporter &reporter) {
      reporter.fault(FaultCode::device_lost);
      reporter.complete(4u);
  });
  CHECK(fixture.reporter().invariants_hold());
  CHECK_EQ(av::RecordedFile(fixture.host(), "fences.av").remaining(), 0u);
  fixture.recorder().close(); }

TEST(fence_window, a_window_with_nothing_behind_it_reads_nothing)
{ FencePage page;
  CHECK(FenceWindow().read(page) != Status::ok);
  CHECK(!FenceWindow().invariants_hold()); }
