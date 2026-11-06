#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
#include "device/memory/window_stats_window.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/pages/window_stats.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <pthread.h>

namespace av {

struct WindowStatsWindowProbe {
    static void shift(WindowStatsWindow &window, std::ptrdiff_t by) { window.page_ += by; }
};

}

using av::MockHost;
using av::StateWriter;
using av::Status;
using av::WindowStats;
using av::WindowStatsWindow;
using av::WindowStatsWindowProbe;

namespace {

constexpr std::uint64_t ram = 0x100000u;
constexpr std::uint64_t page = ram + 0x2000u;

void publish(MockHost &host, const WindowStats &stats, std::uint32_t sequence)
{
    std::uint8_t *const bytes = host.guest_ram(page);
    std::uint8_t        encoded[WindowStats::encoded_size] = {};
    CHECK_STATUS(stats.encode(encoded, sizeof encoded), Status::ok);
    std::memcpy(bytes, encoded, WindowStats::sequence_at);
    std::memcpy(bytes + WindowStats::fields_at, encoded + WindowStats::fields_at,
                WindowStats::encoded_size - WindowStats::fields_at);
    std::memcpy(bytes + WindowStats::sequence_at, &sequence, sizeof sequence);
}

struct Writer {
    std::uint8_t     *bytes;
    volatile bool     stop;
    std::uint32_t     published;
};

void *run_writer(void *context)
{
    Writer *const writer = static_cast<Writer *>(context);
    auto *const   sequence = reinterpret_cast<std::uint32_t *>(writer->bytes + WindowStats::sequence_at);
    auto *const   fields = reinterpret_cast<std::uint64_t *>(writer->bytes + WindowStats::fields_at);
    std::uint32_t before = 0u;
    while (!writer->stop) {
        __atomic_store_n(sequence, before + 1u, __ATOMIC_RELAXED);
        __atomic_thread_fence(__ATOMIC_RELEASE);
        for (std::uint32_t index = 0u; index < WindowStats::field_count; index++)
            __atomic_store_n(&fields[index], std::uint64_t{before} + 1u, __ATOMIC_RELAXED);
        __atomic_store_n(sequence, before + 2u, __ATOMIC_RELEASE);
        before += 2u;
        writer->published++;
    }
    return nullptr;
}

}

TEST(window_stats_window, an_unmapped_window_refuses_a_read)
{ MockHost host;
  WindowStatsWindow window(host);
  WindowStats       stats;
  CHECK(!window.mapped());
  CHECK_STATUS(window.read(stats), Status::invalid_state);
  CHECK(window.invariants_hold()); }

TEST(window_stats_window, an_address_outside_guest_ram_is_not_found)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  WindowStatsWindow window(host);
  CHECK_STATUS(window.map(0x900000u), Status::not_found);
  CHECK(!window.mapped());
  CHECK_EQ(host.live_guest_mappings(), 0u); }

TEST(window_stats_window, a_page_with_the_right_header_maps_and_reads)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  publish(host, WindowStats(512u, 100u, 300u, 2u, 0x4000u, 5u), 6u);
  WindowStatsWindow window(host);
  CHECK_STATUS(window.map(page), Status::ok);
  CHECK(window.mapped());
  CHECK(window.invariants_hold());
  CHECK_EQ(host.live_guest_mappings(), 1u);
  WindowStats stats;
  CHECK_STATUS(window.read(stats), Status::ok);
  CHECK_EQ(stats.total_pages(), 512u);
  CHECK_EQ(stats.pages_in_use(), 100u);
  CHECK_EQ(stats.peak_pages_in_use(), 300u);
  CHECK_EQ(stats.refusals(), 2u);
  CHECK_EQ(stats.refused_bytes(), 0x4000u);
  CHECK_EQ(stats.largest_free_run_at_refusal(), 5u);
  window.release();
  CHECK(!window.mapped());
  CHECK_EQ(host.live_guest_mappings(), 0u); }

TEST(window_stats_window, a_page_with_a_foreign_header_is_unmapped_and_malformed)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  std::memset(host.guest_ram(page), 0x5A, WindowStats::page_size);
  WindowStatsWindow window(host);
  CHECK_STATUS(window.map(page), Status::malformed);
  CHECK(!window.mapped());
  CHECK_EQ(host.live_guest_mappings(), 0u); }

TEST(window_stats_window, a_page_a_writer_holds_mid_update_cannot_be_mapped)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  publish(host, WindowStats(1u, 1u, 1u, 1u, 1u, 1u), 3u);
  WindowStatsWindow window(host);
  CHECK_STATUS(window.map(page), Status::malformed);
  CHECK(!window.mapped());
  CHECK_EQ(host.live_guest_mappings(), 0u); }

TEST(window_stats_window, a_read_under_an_odd_sequence_gives_up_after_its_attempts)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  publish(host, WindowStats(9u, 8u, 7u, 6u, 5u, 4u), 4u);
  WindowStatsWindow window(host);
  CHECK_STATUS(window.map(page), Status::ok);
  const std::uint32_t odd = 5u;
  std::memcpy(host.guest_ram(page) + WindowStats::sequence_at, &odd, sizeof odd);
  WindowStats stats(1u, 1u, 1u, 1u, 1u, 1u);
  CHECK_STATUS(window.read(stats), Status::conflict);
  CHECK_EQ(stats.total_pages(), 1u);
  const std::uint32_t even = 6u;
  std::memcpy(host.guest_ram(page) + WindowStats::sequence_at, &even, sizeof even);
  CHECK_STATUS(window.read(stats), Status::ok);
  CHECK_EQ(stats.total_pages(), 9u);
  CHECK(window.invariants_hold()); }

TEST(window_stats_window, mapping_again_releases_the_page_it_held)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  publish(host, WindowStats(2u, 2u, 2u, 2u, 2u, 2u), 2u);
  WindowStatsWindow window(host);
  CHECK_STATUS(window.map(page), Status::ok);
  CHECK_STATUS(window.map(page), Status::ok);
  CHECK_EQ(host.live_guest_mappings(), 1u);
  CHECK_STATUS(window.map(0x900000u), Status::not_found);
  CHECK(!window.mapped());
  CHECK_EQ(host.live_guest_mappings(), 0u); }

TEST(window_stats_window, a_read_never_returns_a_torn_page_while_a_writer_runs)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  publish(host, WindowStats(), 0u);
  WindowStatsWindow window(host);
  CHECK_STATUS(window.map(page), Status::ok);
  Writer    writer = {host.guest_ram(page), false, 0u};
  pthread_t thread;
  CHECK_EQ(pthread_create(&thread, nullptr, run_writer, &writer), 0);
  std::uint32_t stable = 0u;
  for (std::uint32_t attempt = 0u; attempt < 20000u; attempt++) {
      WindowStats stats;
      if (window.read(stats) != Status::ok) continue;
      stable++;
      for (std::uint32_t index = 1u; index < WindowStats::field_count; index++)
          if (stats.field(index) != stats.field(0u))
              av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "torn read: field %u", index);
  }
  writer.stop = true;
  pthread_join(thread, nullptr);
  CHECK(stable > 0u);
  CHECK(window.invariants_hold()); }

TEST(window_stats_window, the_report_says_attached_and_stable_with_the_numbers)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  publish(host, WindowStats(512u, 100u, 300u, 2u, 0x4000u, 5u), 2u);
  WindowStatsWindow window(host);
  CHECK_STATUS(window.map(page), Status::ok);
  char            text[512] = {};
  StateWriter     writer(text, sizeof text);
  window.report(writer);
  CHECK(std::strstr(writer.dump(), "window_stats/0") != nullptr);
  CHECK(std::strstr(writer.dump(), "attached=true") != nullptr);
  CHECK(std::strstr(writer.dump(), "stable=true") != nullptr);
  CHECK(std::strstr(writer.dump(), "total_pages=512") != nullptr);
  CHECK(std::strstr(writer.dump(), "largest_free_run_at_refusal=5") != nullptr);
  CHECK(writer.invariants_hold()); }

TEST(window_stats_window, the_report_of_an_unmapped_window_has_no_numbers)
{ MockHost host;
  WindowStatsWindow window(host);
  char            text[512] = {};
  StateWriter     writer(text, sizeof text);
  window.report(writer);
  CHECK(std::strstr(writer.dump(), "attached=false") != nullptr);
  CHECK(std::strstr(writer.dump(), "stable=false") != nullptr);
  CHECK(std::strstr(writer.dump(), "total_pages") == nullptr); }

TEST(window_stats_window, a_mapping_that_covers_nothing_is_not_found)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  host.empty_mapping_at(1u);
  WindowStatsWindow window(host);
  CHECK_STATUS(window.map(page), Status::not_found);
  CHECK(!window.mapped());
  CHECK_EQ(host.live_guest_mappings(), 0u); }

TEST(window_stats_window, a_page_the_host_cannot_map_whole_and_directly_is_not_supported)
{ MockHost bounced;
  bounced.add_bounce_region(ram, 0x10000u);
  WindowStatsWindow window(bounced);
  CHECK_STATUS(window.map(page), Status::not_supported);
  CHECK(!window.mapped());
  CHECK_EQ(bounced.live_guest_mappings(), 0u);
  MockHost split;
  split.add_guest_ram(ram, 0x10000u);
  split.split_guest_memory_at(page + 0x800u);
  WindowStatsWindow cut(split);
  CHECK_STATUS(cut.map(page), Status::not_supported);
  CHECK(!cut.mapped());
  CHECK_EQ(split.live_guest_mappings(), 0u); }

TEST(window_stats_window, a_page_held_off_an_eight_byte_boundary_breaks_the_invariant)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  publish(host, WindowStats(512u, 100u, 300u, 2u, 0x4000u, 5u), 6u);
  WindowStatsWindow window(host);
  CHECK_STATUS(window.map(page), Status::ok);
  CHECK(window.invariants_hold());
  WindowStatsWindowProbe::shift(window, 1);
  CHECK(!window.invariants_hold());
  WindowStatsWindowProbe::shift(window, -1);
  CHECK(window.invariants_hold()); }
