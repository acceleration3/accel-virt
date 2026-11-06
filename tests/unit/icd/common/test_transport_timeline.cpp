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
#include "icd/common/transport_timeline.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/escape/context_views_record.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>

using av::GuestConnection;
using av::MockKmt;
using av::PciIdentity;
using av::Status;
using av::SubApi;
using av::TransportResult;
using av::icd::ChannelShape;
using av::icd::HostChannel;
using av::icd::TransportTimeline;

namespace {

const PciIdentity avgpu(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);
constexpr std::uint32_t command_id = 0x30001u;

class Rig {
public:
    Rig(bool page = true)
        : kmt_(), platform_(), connection_(kmt_), log_(), lines_(platform_, log_, "avtest: "), trace_(),
          lock_(trace_, 'S'),
          channel_(platform_, lines_, ChannelShape(SubApi::opengl, 12u, 4096u, 4096u, 8192u, 0u), lock_)
    {
        kmt_.add_adapter(avgpu, 3200u);
        if (!page) kmt_.answer_views_up_to(av::ContextViewsRecord::without_fence);
        std::uint32_t adapter = 0u, count = 0u;
        CHECK_STATUS(kmt_.enumerate_adapters(&adapter, 1u, count), Status::ok);
        CHECK_STATUS(connection_.open(adapter), Status::ok);
        CHECK_STATUS(channel_.open(connection_), Status::ok);
    }
    ~Rig()
    {
        channel_.close(false);
        CHECK_EQ(kmt_.misuses(), 0u);
    }

    HostChannel &channel() { return channel_; }
    MockKmt     &kmt() { return kmt_; }

    void send()
    {
        av::WireWriter writer = channel_.wire().request_writer();
        writer.write_zeros(8u);
        CHECK(channel_.wire().send_async(command_id, writer) == TransportResult::ok);
    }

private:
    MockKmt                 kmt_;
    av::MockPlatform        platform_;
    GuestConnection         connection_;
    av::RecordingLog        log_;
    av::KeyedLog            lines_;
    av::LockTrace           trace_;
    av::RecordingThreadLock lock_;
    HostChannel             channel_;
};

}

TEST(transport_timeline, a_new_timeline_records_the_first_fence_and_covers_and_completes_none)
{ Rig               rig;
  TransportTimeline timeline(rig.channel());
  CHECK_EQ(timeline.recording(), 1u);
  CHECK_EQ(timeline.covering(), 0u);
  CHECK_EQ(timeline.completed(), 0u); }

TEST(transport_timeline, a_held_command_is_covered_before_it_is_published_and_recorded_after)
{ Rig               rig;
  TransportTimeline timeline(rig.channel());
  rig.send();
  CHECK_EQ(timeline.recording(), 1u);
  CHECK_EQ(timeline.covering(), 1u);
  uint64_t target = 0u;
  CHECK(timeline.publish(timeline.covering(), target));
  CHECK_EQ(target, 1u);
  CHECK_EQ(timeline.recording(), 2u);
  CHECK_EQ(timeline.covering(), 1u);
  CHECK(timeline.await(target));
  CHECK(timeline.completed() >= 1u); }

TEST(transport_timeline, waiting_for_a_fence_publishes_what_holds_it_and_waits_for_the_gpu)
{ Rig               rig;
  TransportTimeline timeline(rig.channel());
  rig.send();
  const std::uint32_t waits = rig.kmt().calls(MockKmt::Call::wait_for_context);
  CHECK(timeline.wait(timeline.covering()));
  CHECK_EQ(timeline.recording(), 2u);
  CHECK(rig.kmt().calls(MockKmt::Call::wait_for_context) >= waits);
  CHECK(timeline.completed() >= 1u); }

TEST(transport_timeline, a_fence_past_the_published_one_is_clamped_to_it)
{ Rig               rig;
  TransportTimeline timeline(rig.channel());
  rig.send();
  uint64_t target = 0u;
  CHECK(timeline.publish(99u, target));
  CHECK_EQ(target, 1u);
  uint64_t none = 7u;
  CHECK(timeline.publish(0u, none));
  CHECK_EQ(none, 0u); }

TEST(transport_timeline, awaiting_a_fence_nothing_published_fails_and_leaves_completion_alone)
{ Rig               rig;
  TransportTimeline timeline(rig.channel());
  CHECK(!timeline.await(5u));
  CHECK_EQ(timeline.completed(), 0u); }

TEST(transport_timeline, without_a_progress_page_completion_is_what_was_waited_and_forgetting_resets_it)
{ Rig               rig(false);
  TransportTimeline timeline(rig.channel());
  rig.send();
  CHECK(timeline.wait(timeline.covering()));
  CHECK_EQ(timeline.completed(), 1u);
  timeline.forget();
  CHECK_EQ(timeline.completed(), 0u);
  CHECK(timeline.await(1u));
  CHECK_EQ(timeline.completed(), 1u); }
