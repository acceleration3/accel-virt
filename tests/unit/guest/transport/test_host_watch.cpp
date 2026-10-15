/* Whether a waiting process keeps waiting on its host: while anything the
 * device publishes moves - the ring's head, the page's completion, progress
 * or the pulse's beat - and not once the page says faulted or everything has
 * been still for HostWatch::silence_seconds. The test plays the device by
 * writing the ring header and the page, and moves the clock itself. */
#include "core/support/status.hpp"
#include "guest/transport/host_watch.hpp"
#include "guest/transport/transport_ring.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "proto/submission/fence_page.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>

using av::FencePage;
using av::HostWatch;
using av::MockKmt;
using av::Status;
using av::TransportRing;
using av::WireWriter;

namespace av {

struct HostWatchProbe {
    static void run_backwards(HostWatch &watch) { watch.last_moved_ = watch.now_ + 1u; }
};

}  // namespace av

namespace {

constexpr std::uint8_t  order = 12u;
constexpr std::uint64_t second = MockKmt::ticks_per_second;

/* A ring and a fence page the test writes as the device does, and the clock
 * the watch reads, moved only when the test says. */
class Device {
public:
    Device() : ring_bytes_{}, page_{}, ring_(), watch_(kmt_, ring_)
    {
        CHECK_STATUS(ring_.initialize(ring_bytes_, order), Status::ok);
        CHECK_STATUS(FencePage(0u, 0u).encode(page_, sizeof page_), Status::ok);
        kmt_.reads_clock(nullptr, 0u);
    }

    HostWatch &watch() { return watch_; }
    MockKmt   &kmt() { return kmt_; }
    const std::uint8_t *page() const { return page_; }

    void take_to(std::uint64_t head) { WireWriter(ring_bytes_ + 16u, 8u).write_u64(head); }
    void write(const FencePage &page) { CHECK_STATUS(page.encode(page_, sizeof page_), Status::ok); }
    /* Time passing between two looks. Otherwise the mock's clock moves a
     * millisecond a read once it has been read idly for a while. */
    void pass(std::uint64_t seconds)
    {
        std::uint64_t frequency = 0u;
        kmt_.reads_clock(nullptr, seconds * second);
        static_cast<void>(kmt_.performance_counter(frequency));
        kmt_.reads_clock(nullptr, 0u);
    }

private:
    MockKmt       kmt_;
    std::uint8_t  ring_bytes_[32u + (1u << order)];
    std::uint8_t  page_[FencePage::encoded_size];
    TransportRing ring_;
    HostWatch     watch_;
};

}  // namespace

TEST(host_watch, without_a_page_only_the_head_is_watched)
{ Device device;
  CHECK(!device.watch().reads_page());
  CHECK_EQ(device.watch().completed(), 0u);
  CHECK(!device.watch().faulted());
  device.watch().start();
  device.pass(HostWatch::silence_seconds - 1u);
  CHECK(device.watch().keeps_waiting());
  device.take_to(64u);
  CHECK(device.watch().keeps_waiting());   /* moved: the silence starts again */
  device.pass(HostWatch::silence_seconds - 1u);
  CHECK(device.watch().keeps_waiting());
  device.pass(1u);
  CHECK(!device.watch().keeps_waiting());   /* the silence since the head moved */
  CHECK(device.watch().invariants_hold()); }

/* Each word the device publishes alone keeps the wait going: the completion,
 * the progress, and the pulse's beat. */
TEST(host_watch, each_published_word_alone_is_a_host_at_work)
{ const FencePage moves[] = {FencePage(1u, 0u), FencePage(0u, 1u), FencePage(0u, 0u).beating(1u)};
  for (const FencePage &move : moves) {
      Device device;
      device.watch().read_page(device.page());
      device.watch().start();
      device.pass(HostWatch::silence_seconds - 1u);
      CHECK(device.watch().keeps_waiting());
      device.write(move);
      CHECK(device.watch().keeps_waiting());
      device.pass(HostWatch::silence_seconds - 1u);
      CHECK(device.watch().keeps_waiting());   /* 58 s since the start, 29 since the move */
      device.pass(1u);
      CHECK(!device.watch().keeps_waiting());
  } }

TEST(host_watch, reads_the_completion_and_the_fault_from_the_page)
{ Device device;
  device.watch().read_page(device.page());
  CHECK(device.watch().reads_page());
  device.write(FencePage(7u, 9u));
  CHECK_EQ(device.watch().completed(), 7u);
  CHECK(!device.watch().faulted());
  CHECK_EQ(device.watch().fault_code(), 0u);
  device.write(FencePage(7u, 9u, 6u));
  CHECK(device.watch().faulted());
  CHECK_EQ(device.watch().fault_code(), 6u);
  device.write(FencePage(7u, 9u, 7u));
  CHECK_EQ(device.watch().fault_code(), 7u);
  device.watch().read_page(nullptr);
  CHECK(!device.watch().faulted());
  CHECK_EQ(device.watch().fault_code(), 0u);
  CHECK_EQ(device.watch().completed(), 0u); }

/* A fault ends the wait at once, whatever else moved. */
TEST(host_watch, a_faulted_page_ends_the_wait_at_once)
{ Device device;
  device.watch().read_page(device.page());
  device.watch().start();
  device.write(FencePage(3u, 4u, 7u).beating(9u));
  CHECK(!device.watch().keeps_waiting()); }

/* Silence is counted from a wait's first look, not from the last wait's:
 * what moved between two waits is the new baseline, and a start reads no
 * clock - most waits end at their first look. */
TEST(host_watch, start_counts_the_silence_afresh_from_what_is_published_then)
{ Device device;
  device.watch().read_page(device.page());
  device.watch().start();
  CHECK(device.watch().keeps_waiting());
  device.pass(HostWatch::silence_seconds);
  CHECK(!device.watch().keeps_waiting());
  device.write(FencePage(0u, 0u).beating(5u));
  device.watch().start();
  device.pass(HostWatch::silence_seconds);
  CHECK(device.watch().keeps_waiting());   /* the first look; the beat before the start is no move */
  CHECK_EQ(device.watch().elapsed(), 0u);
  CHECK_EQ(device.watch().frequency(), second);
  device.pass(HostWatch::silence_seconds);
  CHECK(!device.watch().keeps_waiting());
  CHECK(device.watch().elapsed() >= HostWatch::silence_seconds * second); }

/* A clock that cannot say how long a tick is gives no silence any length:
 * the wait ends at the first look. */
TEST(host_watch, a_clock_with_no_frequency_ends_the_wait)
{ Device device;
  device.kmt().set_frequency(0u);
  device.watch().start();
  CHECK(!device.watch().keeps_waiting()); }

TEST(host_watch_invariants, a_move_after_the_last_look_is_detected)
{ Device device;
  device.watch().start();
  CHECK(device.watch().invariants_hold());
  av::HostWatchProbe::run_backwards(device.watch());
  CHECK(!device.watch().invariants_hold()); }
