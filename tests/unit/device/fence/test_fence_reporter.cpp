/* One context's completions as the guest sees them: the fence page, written
 * only from the device's counters, and one interrupt per service. */
#include "core/support/status.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/fence/fence_reporter.hpp"
#include "device/fence/fence_window.hpp"
#include "device/memory/blob_window.hpp"
#include "device/threading/doorbell.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"
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
using av::FenceWindow;
using av::HostViolation;
using av::MockHost;
using av::Status;

namespace av {

struct FenceReporterProbe {
    static void pend_unprepared(FenceReporter &reporter) { reporter.pending_ = true; }
    static void lose_progress(FenceReporter &reporter) { reporter.completed_ = 5u; reporter.progress_ = 0u; }
    static void lose_page(FenceReporter &reporter) { reporter.page_ = FenceWindow(); }
    static void fault_without_code(FenceReporter &reporter) { reporter.faulted_ = true; reporter.fault_ = FaultCode::none; }
};

}  // namespace av

namespace {

constexpr std::uint32_t line = 6u;

class Fixture {
public:
    Fixture() : bytes_{}, pointer_(bytes_), length_(sizeof bytes_), interrupts_(host_), reporter_(host_, interrupts_)
    {
        std::memset(bytes_, 0xEE, sizeof bytes_);
        static_cast<void>(interrupts_.initialize());
        static_cast<void>(interrupts_.connect());
    }

    MockHost      &host() { return host_; }
    FenceReporter &reporter() { return reporter_; }
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
    FenceReporter   reporter_;
};

}  // namespace

TEST(fence_reporter_prepare, writes_a_fresh_page)
{ Fixture fixture;
  CHECK_STATUS(fixture.reporter().prepare(fixture.window(), line), Status::ok);
  CHECK(fixture.reporter().invariants_hold());
  const ByteLayout layout(fixture.bytes(), 32u);
  CHECK(layout.at(0u, 4u) == 0x464B5652u);
  CHECK(layout.at(4u, 4u) == 0u);
  CHECK(layout.at(8u, 8u) == 0u);
  CHECK(layout.at(16u, 8u) == 0u);
  CHECK(layout.at(24u, 8u) == 0u);
  CHECK_EQ(fixture.bytes()[32], 0xEEu);
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
  CHECK_STATUS(FenceWindow().publish(FencePage()), Status::invalid_parameter); }

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

TEST(fence_reporter, prepares_on_the_main_loop_and_reports_on_a_worker)
{ Fixture fixture;
  fixture.reporter().complete(1u);
  fixture.reporter().signal();
  fixture.reporter().heartbeat();
  fixture.reporter().fault(FaultCode::ring_header);
  for (int count = 0; count < 4; count++) CHECK(fixture.host().claim(HostViolation::worker_required));
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
  CHECK(!unprepared.reporter().invariants_hold()); }
