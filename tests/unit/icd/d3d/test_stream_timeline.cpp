/* A session's GPU timeline over its stream and transport
 * (docs/mapped-buffers-plan.md, 3.3): commands recorded now go out in the
 * batch the transport publishes next; the fence page says which the GPU has
 * finished; a wait for the batch still being recorded sends it first, under
 * the stream's lock; and against a driver with no fence page, what a wait
 * returned for is known done. */
#include "core/support/status.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "icd/d3d/d3d_command_stream.hpp"
#include "icd/d3d/stream_timeline.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/escape/context_views_record.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"

#include <cstdint>
#include <cstring>

using av::GuestConnection;
using av::KmtTransport;
using av::LockTrace;
using av::MockKmt;
using av::PciIdentity;
using av::RecordingThreadLock;
using av::Status;
using av::SubApi;
using av::d3d::D3dCommandStream;
using av::d3d::StreamTimeline;
using namespace av::d3dproto;

namespace {

const PciIdentity av(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);
constexpr std::uint8_t  ring_order = 20u;
constexpr std::uint32_t reply_bytes = 4096u;
constexpr std::size_t   staging_bytes = 1024u;
constexpr std::size_t   block_bytes = D3dCommandStream::command_room + 4096u;

class Fixture {
public:
    explicit Fixture(std::uint16_t views)
        : connection_(kmt_), transport_(connection_, SubApi::d3d, ring_order, reply_bytes, staging_, staging_bytes),
          stream_(transport_), lock_(trace_, 'S'), timeline_(stream_, transport_, lock_)
    {
        kmt_.add_adapter(av, 3200u);
        kmt_.answer_views_up_to(views);
        std::uint32_t adapters[1] = {};
        std::uint32_t count = 0u;
        static_cast<void>(kmt_.enumerate_adapters(adapters, 1u, count));
        CHECK_STATUS(connection_.open(adapters[0]), Status::ok);
        CHECK_STATUS(transport_.initialize(), Status::ok);
        transport_.hold_in(block_, block_bytes);
    }
    ~Fixture() { transport_.teardown(); }

    MockKmt          &kmt() { return kmt_; }
    D3dCommandStream &stream() { return stream_; }
    StreamTimeline   &timeline() { return timeline_; }
    LockTrace        &trace() { return trace_; }

private:
    MockKmt             kmt_;
    GuestConnection     connection_;
    std::uint8_t        staging_[staging_bytes] = {};
    KmtTransport        transport_;
    std::uint8_t        block_[block_bytes] = {};
    D3dCommandStream    stream_;
    LockTrace           trace_;
    RecordingThreadLock lock_;
    StreamTimeline      timeline_;
};

}  // namespace

/* The batch being recorded is the next to be published; a wait for it sends
 * it, and returns once the page shows it finished. */
TEST(stream_timeline, a_wait_for_the_batch_being_recorded_sends_it_and_sees_it_finish)
{ Fixture fixture(av::ContextViewsRecord::with_fence);
  CHECK_EQ(fixture.timeline().recording(), 1u);
  CHECK_EQ(fixture.timeline().completed(), 0u);
  CHECK(av_d3d_Draw(fixture.stream(), 3u, 0u));
  const std::uint32_t submits = fixture.kmt().calls(MockKmt::Call::submit);
  CHECK(fixture.timeline().wait(1u));
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::submit), submits + 1u);
  CHECK_EQ(fixture.stream().recorded_bytes(), 0u);
  CHECK(fixture.timeline().completed() >= 1u);
  CHECK_EQ(fixture.timeline().recording(), 2u);
  /* One finished already asks nothing more. */
  const std::uint32_t waits = fixture.kmt().calls(MockKmt::Call::wait_for_context);
  CHECK(fixture.timeline().wait(1u));
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::wait_for_context), waits);
  /* Each wait under the stream's lock, taken and let go. */
  CHECK(std::strcmp(fixture.trace().text(), "SsSs") == 0); }

/* A fence whose batch had nothing recorded in it is waited for as the newest
 * published; nothing published at all is nothing to wait for. */
TEST(stream_timeline, a_batch_with_nothing_recorded_waits_for_the_newest_published)
{ Fixture fixture(av::ContextViewsRecord::with_fence);
  CHECK(fixture.timeline().wait(1u));   /* nothing ever sent */
  CHECK_EQ(fixture.kmt().calls(MockKmt::Call::submit), 0u);
  CHECK(av_d3d_Draw(fixture.stream(), 3u, 0u));
  CHECK_STATUS(fixture.stream().flush(), Status::ok);
  CHECK(fixture.timeline().wait(2u));   /* 2 is still being recorded, empty */
  CHECK(fixture.timeline().completed() >= 1u); }

/* With no fence page the page says nothing, but what a wait returned for is
 * known done, so retired memory still comes back. */
TEST(stream_timeline, with_no_fence_page_what_was_waited_for_is_done)
{ Fixture fixture(av::ContextViewsRecord::without_fence);
  CHECK(av_d3d_Draw(fixture.stream(), 3u, 0u));
  CHECK(fixture.timeline().wait(1u));
  CHECK_EQ(fixture.timeline().completed(), 1u);
  CHECK(av_d3d_Draw(fixture.stream(), 3u, 0u));
  CHECK_STATUS(fixture.stream().flush(), Status::ok);
  CHECK_EQ(fixture.timeline().completed(), 1u);   /* sent, not waited for */
  CHECK(fixture.timeline().wait(2u));
  CHECK_EQ(fixture.timeline().completed(), 2u); }

/* A context the host lost cannot be waited on. */
TEST(stream_timeline, a_lost_context_cannot_be_waited_on)
{ Fixture fixture(av::ContextViewsRecord::with_fence);
  CHECK(av_d3d_Draw(fixture.stream(), 3u, 0u));
  fixture.kmt().fail_first(MockKmt::Call::wait_for_context, 1u, 0xC0000001u);
  CHECK(!fixture.timeline().wait(1u));
  CHECK(!fixture.timeline().wait(1u)); }
