#include "core/log/keyed_log.hpp"
#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/common/host_channel.hpp"
#include "icd/common/locked_timeline.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/escape/context_views_record.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>

using av::GuestConnection;
using av::LockTrace;
using av::MockKmt;
using av::PciIdentity;
using av::RecordingThreadLock;
using av::Status;
using av::SubApi;
using av::TransportResult;
using av::WireWriter;
using av::icd::ChannelShape;
using av::icd::HostChannel;
using av::icd::LockedTimeline;

namespace {

const PciIdentity avgpu(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);
constexpr std::uint8_t  ring_order = 12u;
constexpr std::uint32_t reply_bytes = 4096u;
constexpr std::size_t   staging_bytes = 4096u;
constexpr std::uint32_t command_id = 0x10001u;

class Fixture {
public:
    explicit Fixture(std::uint16_t views)
        : connection_(kmt_), lines_(platform_, log_, "avtest: "), lock_(trace_, 'S'),
          channel_(platform_, lines_,
                   ChannelShape(SubApi::d3d, ring_order, reply_bytes, staging_bytes, 2u * staging_bytes, 0u), lock_),
          timeline_(channel_, lock_)
    {
        kmt_.add_adapter(avgpu, 3200u);
        kmt_.open_host_window(MockKmt::rig_window_bytes);
        kmt_.answer_views_up_to(views);
        std::uint32_t adapter = 0u, count = 0u;
        CHECK_STATUS(kmt_.enumerate_adapters(&adapter, 1u, count), Status::ok);
        CHECK_STATUS(connection_.open(adapter), Status::ok);
        CHECK_STATUS(channel_.open(connection_), Status::ok);
    }
    ~Fixture()
    {
        channel_.close(false);
        CHECK_EQ(kmt_.misuses(), 0u);
    }

    MockKmt        &kmt() { return kmt_; }
    LockedTimeline &timeline() { return timeline_; }
    LockTrace      &trace() { return trace_; }
    HostChannel    &channel() { return channel_; }
    std::uint32_t   submits() { return kmt_.calls(MockKmt::Call::submit); }

    void record()
    {
        WireWriter writer = channel_.wire().request_writer();
        writer.write_zeros(8u);
        CHECK(channel_.wire().send_async(command_id, writer) == TransportResult::ok);
    }

private:
    MockKmt             kmt_;
    av::MockPlatform    platform_;
    GuestConnection     connection_;
    av::RecordingLog    log_;
    av::KeyedLog        lines_;
    LockTrace           trace_;
    RecordingThreadLock lock_;
    HostChannel         channel_;
    LockedTimeline      timeline_;
};

}

TEST(locked_timeline, a_wait_for_the_batch_being_recorded_sends_it_and_sees_it_finish)
{ Fixture fixture(av::ContextViewsRecord::with_fence);
  CHECK_EQ(fixture.timeline().recording(), 1u);
  CHECK_EQ(fixture.timeline().covering(), 0u);
  CHECK_EQ(fixture.timeline().completed(), 0u);
  fixture.record();
  CHECK_EQ(fixture.timeline().recording(), 1u);
  CHECK_EQ(fixture.timeline().covering(), 1u);
  fixture.trace().clear();
  const std::uint32_t submits = fixture.submits();
  CHECK(fixture.timeline().wait(1u));
  CHECK_EQ(fixture.submits(), submits + 1u);
  CHECK_EQ(fixture.timeline().covering(), 1u);
  CHECK(fixture.timeline().completed() >= 1u);
  CHECK_EQ(fixture.timeline().recording(), 2u);
  const std::uint32_t waits = fixture.kmt().calls(MockKmt::Call::wait_for_context);
  CHECK(fixture.timeline().wait(1u));
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::wait_for_context), waits);
  CHECK(std::strcmp(fixture.trace().text(), "SsSs") == 0);
  CHECK(fixture.channel().invariants_hold()); }

TEST(locked_timeline, a_batch_with_nothing_recorded_waits_for_the_newest_published)
{ Fixture fixture(av::ContextViewsRecord::with_fence);
  CHECK(fixture.timeline().wait(1u));
  CHECK_EQ(fixture.submits(), 0u);
  fixture.record();
  CHECK(fixture.channel().flush() == TransportResult::ok);
  CHECK(fixture.timeline().wait(2u));
  CHECK(fixture.timeline().completed() >= 1u);
  CHECK(fixture.channel().invariants_hold()); }

TEST(locked_timeline, with_no_fence_page_what_was_waited_for_is_done_until_it_is_forgotten)
{ Fixture fixture(av::ContextViewsRecord::without_fence);
  fixture.record();
  CHECK(fixture.timeline().wait(1u));
  CHECK_EQ(fixture.timeline().completed(), 1u);
  fixture.record();
  CHECK(fixture.channel().flush() == TransportResult::ok);
  CHECK_EQ(fixture.timeline().completed(), 1u);
  CHECK(fixture.timeline().wait(2u));
  CHECK_EQ(fixture.timeline().completed(), 2u);
  fixture.timeline().forget();
  CHECK_EQ(fixture.timeline().completed(), 0u); }

TEST(locked_timeline, a_lost_context_cannot_be_waited_on)
{ Fixture fixture(av::ContextViewsRecord::with_fence);
  fixture.record();
  fixture.kmt().fail_first(MockKmt::Call::wait_for_context, 1u, 0xC0000001u);
  CHECK(!fixture.timeline().wait(1u));
  CHECK(!fixture.timeline().wait(1u)); }

TEST(locked_timeline, a_batch_that_cannot_be_sent_cannot_be_waited_for)
{ Fixture fixture(av::ContextViewsRecord::with_fence);
  fixture.record();
  fixture.kmt().fail_first(MockKmt::Call::submit, 1u, 0xC0000001u);
  CHECK(!fixture.timeline().wait(1u));
  CHECK(fixture.channel().faulted());
  CHECK_EQ(fixture.timeline().completed(), 0u); }
