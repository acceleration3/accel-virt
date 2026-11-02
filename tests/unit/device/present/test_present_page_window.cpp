#include "device/present/present_page_window.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/pages/present_progress.hpp"

#include <cstdint>
#include <cstring>

using av::MockHost;
using av::PresentPageWindow;
using av::PresentProgress;
using av::Status;

namespace {

constexpr std::uint64_t ram = 0x100000u;
constexpr std::uint64_t page = ram + 0x3000u;

std::uint64_t u64_at(MockHost &host, std::uint64_t address)
{
    std::uint64_t value = 0u;
    std::memcpy(&value, host.guest_ram(address), sizeof value);
    return value;
}

PresentProgress progress_of(MockHost &host)
{
    PresentProgress progress(99u, 99u);
    CHECK_STATUS(progress.decode(host.guest_ram(page), PresentProgress::encoded_size), Status::ok);
    return progress;
}

}

TEST(present_page_window, a_page_of_guest_ram_is_headed_and_counts_nothing)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  std::memset(host.guest_ram(page), 0xAB, PresentProgress::page_size);
  PresentPageWindow window(host);
  CHECK(!window.mapped());
  CHECK_STATUS(window.map(page), Status::ok);
  CHECK(window.mapped());
  const PresentProgress progress = progress_of(host);
  CHECK_EQ(progress.completed(), 0u);
  CHECK_EQ(progress.refused(), 0u);
  CHECK_EQ(u64_at(host, page + PresentProgress::page_size - 8u), 0u);
  CHECK_EQ(host.live_guest_mappings(), 1u);
  window.release();
  CHECK(!window.mapped());
  CHECK_EQ(host.live_guest_mappings(), 0u); }

TEST(present_page_window, each_progress_is_written_at_its_offsets_and_nothing_else_moves)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  PresentPageWindow window(host);
  CHECK_STATUS(window.map(page), Status::ok);
  window.publish(PresentProgress(0x100000007u, 2u));
  CHECK_EQ(u64_at(host, page + 8u), 0x100000007u);
  CHECK_EQ(u64_at(host, page + 16u), 2u);
  CHECK_EQ(u64_at(host, page + 24u), 0u);
  const PresentProgress progress = progress_of(host);
  CHECK_EQ(progress.completed(), 0x100000007u);
  CHECK_EQ(progress.refused(), 2u);
  window.release();
  window.publish(PresentProgress(9u, 0u));
  CHECK_EQ(u64_at(host, page + 8u), 0x100000007u); }

TEST(present_page_window, memory_that_is_not_a_page_of_guest_ram_is_refused_and_nothing_held)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  host.add_bounce_region(0x40000000u, 0x10000u);
  host.split_guest_memory_at(ram + 0x5800u);
  PresentPageWindow window(host);
  CHECK_STATUS(window.map(0x80000000u), Status::not_found);
  CHECK_STATUS(window.map(0x40000000u), Status::not_supported);
  CHECK_STATUS(window.map(ram + 0x5000u), Status::not_supported);
  host.empty_mapping_at(1u);
  CHECK_STATUS(window.map(page), Status::not_found);
  CHECK(!window.mapped());
  CHECK_EQ(host.live_guest_mappings(), 0u); }

TEST(present_page_window, a_page_mapped_again_lets_the_first_go)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  PresentPageWindow window(host);
  CHECK_STATUS(window.map(page), Status::ok);
  CHECK_STATUS(window.map(page + 0x1000u), Status::ok);
  CHECK_EQ(host.live_guest_mappings(), 1u);
  window.publish(PresentProgress(3u, 0u));
  CHECK_EQ(u64_at(host, page + 0x1000u + 8u), 3u);
  CHECK_STATUS(window.map(0x80000000u), Status::not_found);
  CHECK_EQ(host.live_guest_mappings(), 0u);
  CHECK(!window.mapped()); }
