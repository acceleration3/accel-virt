#include "device/control/control_request.hpp"
#include "device/control/window_stats_commands.hpp"
#include "device/memory/window_stats_window.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/gpu_request_builder.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"
#include "proto/pages/window_stats.hpp"

#include <cstdint>
#include <cstring>

using av::ControlRequest;
using av::GpuRequestBuilder;
using av::MockHost;
using av::MockRequest;
using av::WindowStatsCommands;
using av::WindowStatsWindow;

namespace {

constexpr std::uint64_t ram = 0x300000u;
constexpr std::uint32_t window_stats_type = 0x02F9u;

struct Rig {
    MockHost          host;
    WindowStatsWindow window;

    Rig() : host(), window(host) { host.add_guest_ram(ram, 0x10000u); }
    void publish(std::uint64_t at)
    {
        std::uint8_t encoded[av::WindowStats::encoded_size] = {};
        CHECK_STATUS(av::WindowStats(512u, 100u, 300u, 2u, 0x4000u, 5u).encode(encoded, sizeof encoded), av::Status::ok);
        std::memcpy(host.guest_ram(at), encoded, sizeof encoded);
    }
    ~Rig() { window.release(); }

    std::uint32_t send(const GpuRequestBuilder &built, std::uint32_t type)
    {
        MockRequest    mock(built.bytes(), built.size());
        ControlRequest request(host, &mock);
        return WindowStatsCommands(host, window).answer(request, type) ? mock.response_type() : 0u;
    }
};

}

TEST(window_stats_commands, a_page_of_guest_ram_is_mapped_for_reading)
{ Rig rig;
  rig.publish(ram + 0x2000u);
  GpuRequestBuilder request(window_stats_type);
  request.u64(ram + 0x2000u);
  CHECK_EQ(rig.send(request, window_stats_type), 0x1100u);
  CHECK(rig.window.mapped());
  CHECK(rig.window.invariants_hold());
  CHECK_EQ(rig.host.live_guest_mappings(), 1u);
  av::WindowStats stats;
  CHECK_STATUS(rig.window.read(stats), av::Status::ok);
  CHECK_EQ(stats.total_pages(), 512u); }

TEST(window_stats_commands, a_page_with_no_counters_header_is_refused)
{ Rig rig;
  GpuRequestBuilder request(window_stats_type);
  request.u64(ram + 0x4000u);
  CHECK_EQ(rig.send(request, window_stats_type), 0x1205u);
  CHECK(!rig.window.mapped());
  CHECK_EQ(rig.host.live_guest_mappings(), 0u); }

TEST(window_stats_commands, a_page_that_cannot_be_taken_is_refused_and_said_why)
{ Rig rig;
  GpuRequestBuilder unaligned(window_stats_type);
  unaligned.u64(ram + 0x2008u);
  CHECK_EQ(rig.send(unaligned, window_stats_type), 0x1205u);
  CHECK(std::strstr(rig.host.last_log(), "host window counters' page was refused") != nullptr);
  GpuRequestBuilder missing(window_stats_type);
  missing.u64(0x80000000u);
  CHECK_EQ(rig.send(missing, window_stats_type), 0x1205u);
  GpuRequestBuilder cut(window_stats_type);
  cut.u64(ram).truncate(31u);
  CHECK_EQ(rig.send(cut, window_stats_type), 0x1205u);
  CHECK(!rig.window.mapped());
  CHECK_EQ(rig.host.live_guest_mappings(), 0u); }

TEST(window_stats_commands, other_types_are_left_alone)
{ Rig rig;
  GpuRequestBuilder request(0x02F4u);
  request.u64(ram);
  CHECK_EQ(rig.send(request, 0x02F4u), 0u);
  CHECK(!rig.window.mapped()); }
