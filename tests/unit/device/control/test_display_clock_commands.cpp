/* DISPLAY_CLOCK and DISPLAY_TIMING as the device answers them: a page of guest
 * RAM is taken and published in, anything else is refused with the reason
 * logged; a head's rate is taken when the head and the rate are ones the
 * protocol allows; a request too short for its fields is refused; every other
 * type is left to the rest of the device. */
#include "device/control/control_request.hpp"
#include "device/control/display_clock_commands.hpp"
#include "device/display/display_clock.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/gpu_request_builder.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"

#include <cstdint>
#include <cstring>

using av::ControlRequest;
using av::DisplayClock;
using av::DisplayClockCommands;
using av::FenceInterrupts;
using av::GpuRequestBuilder;
using av::MockHost;
using av::MockRequest;
using av::Status;

namespace {

constexpr std::uint64_t ram = 0x300000u;

void nothing(void *) {}

struct Rig {
    MockHost        host;
    FenceInterrupts interrupts;
    DisplayClock    clock;

    Rig() : host(), interrupts(host), clock(host, interrupts, &nothing, nullptr)
    {
        host.add_guest_ram(ram, 0x10000u);
        static_cast<void>(clock.initialize());
    }
    ~Rig() { clock.detach(); }

    /* The response type, or 0 when the commands left the request alone. */
    std::uint32_t send(const GpuRequestBuilder &built, std::uint32_t type)
    {
        MockRequest    mock(built.bytes(), built.size());
        ControlRequest request(host, &mock);
        return DisplayClockCommands(host, clock).answer(request, type) ? mock.response_type() : 0u;
    }
};

}  // namespace

TEST(display_clock_commands, a_page_of_guest_ram_is_taken_and_published_in)
{ Rig rig;
  GpuRequestBuilder request(0x02F4u);
  request.u64(ram + 0x2000u);
  CHECK_EQ(rig.send(request, 0x02F4u), 0x1100u);
  CHECK(rig.clock.latches(0u));
  std::uint32_t magic = 0u;
  std::memcpy(&magic, rig.host.guest_ram(ram + 0x2000u), sizeof magic);
  CHECK_EQ(magic, 0x434B5652u);
  GpuRequestBuilder none(0x02F4u);
  none.u64(0u);
  CHECK_EQ(rig.send(none, 0x02F4u), 0x1100u);   /* none stops publishing */
  CHECK(!rig.clock.latches(0u));
  CHECK_EQ(rig.host.live_threads(), 0u); }

TEST(display_clock_commands, a_page_that_cannot_be_taken_is_refused_and_said_why)
{ Rig rig;
  GpuRequestBuilder unaligned(0x02F4u);
  unaligned.u64(ram + 0x2008u);
  CHECK_EQ(rig.send(unaligned, 0x02F4u), 0x1205u);
  CHECK(std::strstr(rig.host.last_log(), "display clock's page was refused") != nullptr);
  GpuRequestBuilder missing(0x02F4u);
  missing.u64(0x80000000u);
  CHECK_EQ(rig.send(missing, 0x02F4u), 0x1205u);
  GpuRequestBuilder cut(0x02F4u);
  cut.u64(ram).truncate(31u);
  CHECK_EQ(rig.send(cut, 0x02F4u), 0x1205u);
  rig.host.fail_thread_start_at(1u);
  GpuRequestBuilder no_thread(0x02F4u);
  no_thread.u64(ram);
  CHECK_EQ(rig.send(no_thread, 0x02F4u), 0x1201u);   /* ERR_OUT_OF_MEMORY */
  CHECK(!rig.clock.latches(0u));
  CHECK_EQ(rig.host.live_guest_mappings(), 0u); }

TEST(display_clock_commands, a_head_s_rate_is_taken_when_the_head_and_rate_are_allowed)
{ Rig rig;
  GpuRequestBuilder rate(0x02F5u);
  rate.u32(3u).u32(143998u);
  CHECK_EQ(rig.send(rate, 0x02F5u), 0x1100u);
  GpuRequestBuilder past_heads(0x02F5u);
  past_heads.u32(DisplayClock::heads).u32(60000u);
  CHECK_EQ(rig.send(past_heads, 0x02F5u), 0x1205u);
  GpuRequestBuilder too_fast(0x02F5u);
  too_fast.u32(0u).u32(1000001u);
  CHECK_EQ(rig.send(too_fast, 0x02F5u), 0x1205u);
  GpuRequestBuilder cut(0x02F5u);
  cut.u32(0u).u32(60000u).truncate(31u);
  CHECK_EQ(rig.send(cut, 0x02F5u), 0x1205u); }

TEST(display_clock_commands, other_types_are_left_alone)
{ Rig rig;
  GpuRequestBuilder modes(0x02F3u);
  CHECK_EQ(rig.send(modes, 0x02F3u), 0u); }
