#include "device/display/clock_page_window.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/pages/clock_page_header.hpp"
#include "proto/pages/vblank_record.hpp"

#include <cstdint>
#include <cstring>

namespace av {

struct ClockPageWindowProbe {
    static void set_heads(ClockPageWindow &window, std::uint32_t heads) { window.heads_ = heads; }
};

}

using av::ClockPageHeader;
using av::ClockPageWindowProbe;
using av::ClockPageWindow;
using av::MockHost;
using av::Status;
using av::VblankRecord;

namespace {

constexpr std::uint64_t ram = 0x100000u;
constexpr std::uint64_t page = ram + 0x3000u;

std::uint32_t word_at(MockHost &host, std::uint64_t address)
{
    std::uint32_t value = 0u;
    std::memcpy(&value, host.guest_ram(address), sizeof value);
    return value;
}

std::uint64_t record_of(std::uint32_t head) { return page + 32u + 64u * head; }

}

TEST(clock_page_window, a_page_of_guest_ram_is_headed_and_its_records_zeroed)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  std::memset(host.guest_ram(page), 0xAB, ClockPageHeader::page_size);
  ClockPageWindow window(host);
  CHECK(!window.mapped());
  CHECK_STATUS(window.map(page, 16u), Status::ok);
  CHECK(window.mapped());
  CHECK(window.invariants_hold());
  ClockPageHeader header;
  CHECK_STATUS(header.decode(host.guest_ram(page), ClockPageHeader::encoded_size), Status::ok);
  CHECK_EQ(header.heads(), 16u);
  CHECK_EQ(word_at(host, record_of(15u) + 60u), 0u);
  CHECK_EQ(word_at(host, page + ClockPageHeader::page_size - 4u), 0u);
  CHECK_EQ(host.live_guest_mappings(), 1u);
  window.release();
  CHECK(!window.mapped());
  CHECK_EQ(host.live_guest_mappings(), 0u);
  CHECK(window.invariants_hold()); }

TEST(clock_page_window, a_blank_is_written_whole_under_an_even_generation)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  ClockPageWindow window(host);
  CHECK_STATUS(window.map(page, 4u), Status::ok);
  window.publish(3u, VblankRecord(9u, 77u, 123456789u, 60000u));
  CHECK_EQ(word_at(host, record_of(3u)), 2u);
  VblankRecord read;
  CHECK_STATUS(read.decode(host.guest_ram(record_of(3u) + VblankRecord::fields_at), VblankRecord::encoded_size),
               Status::ok);
  CHECK_EQ(read.sequence(), 77u);
  CHECK_EQ(read.latched(), 9u);
  CHECK_EQ(read.vblank_ns(), 123456789u);
  CHECK_EQ(read.refresh_millihz(), 60000u);
  window.publish(3u, VblankRecord(10u, 78u, 140123456u, 60000u));
  CHECK_EQ(word_at(host, record_of(3u)), 4u);
  CHECK_EQ(word_at(host, record_of(2u)), 0u);
  CHECK_EQ(word_at(host, record_of(3u) + 64u), 0u);
  window.publish(4u, VblankRecord(1u, 1u, 1u, 60000u));
  CHECK_EQ(word_at(host, record_of(4u)), 0u);
  CHECK(window.invariants_hold()); }

TEST(clock_page_window, the_guest_s_interrupt_every_is_read_as_it_wrote_it)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  ClockPageWindow window(host);
  CHECK_EQ(window.interrupt_every(0u), 0u);
  CHECK_STATUS(window.map(page, 2u), Status::ok);
  CHECK_EQ(window.interrupt_every(1u), 0u);
  const std::uint32_t every = 0xFFFFFFFFu;
  std::memcpy(host.guest_ram(record_of(1u) + VblankRecord::interrupt_every_at), &every, sizeof every);
  CHECK_EQ(window.interrupt_every(1u), 0xFFFFFFFFu);
  CHECK_EQ(window.interrupt_every(0u), 0u);
  std::memcpy(host.guest_ram(record_of(2u) + VblankRecord::interrupt_every_at), &every, sizeof every);
  CHECK_EQ(window.interrupt_every(2u), 0u);
  window.publish(1u, VblankRecord(1u, 2u, 3u, 60000u));
  CHECK_EQ(window.interrupt_every(1u), 0xFFFFFFFFu);
  window.release();
  CHECK_EQ(window.interrupt_every(1u), 0u); }

TEST(clock_page_window, memory_that_is_not_a_page_of_guest_ram_is_refused_and_nothing_held)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  host.add_bounce_region(0x40000000u, 0x10000u);
  host.split_guest_memory_at(ram + 0x5800u);
  ClockPageWindow window(host);
  CHECK_STATUS(window.map(0x80000000u, 1u), Status::not_found);
  CHECK_STATUS(window.map(0x40000000u, 1u), Status::not_supported);
  CHECK_STATUS(window.map(ram + 0x5000u, 1u), Status::not_supported);
  CHECK_STATUS(window.map(page, 0u), Status::invalid_parameter);
  CHECK_STATUS(window.map(page, 17u), Status::invalid_parameter);
  host.empty_mapping_at(1u);
  CHECK_STATUS(window.map(page, 1u), Status::not_found);
  CHECK(!window.mapped());
  CHECK_EQ(host.live_guest_mappings(), 0u);
  window.publish(0u, VblankRecord(1u, 1u, 1u, 60000u));
  CHECK(window.invariants_hold()); }

TEST(clock_page_window, a_page_mapped_again_lets_the_first_go)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  ClockPageWindow window(host);
  CHECK_STATUS(window.map(page, 1u), Status::ok);
  CHECK_STATUS(window.map(page + 0x1000u, 2u), Status::ok);
  CHECK_EQ(host.live_guest_mappings(), 1u);
  window.publish(1u, VblankRecord(3u, 3u, 3u, 60000u));
  CHECK_EQ(word_at(host, page + 0x1000u + 96u), 2u);
  CHECK_STATUS(window.map(0x80000000u, 1u), Status::not_found);
  CHECK_EQ(host.live_guest_mappings(), 0u);
  CHECK(!window.mapped()); }

TEST(clock_page_window, the_invariant_ties_the_head_count_to_whether_a_page_is_mapped)
{ MockHost host;
  host.add_guest_ram(ram, 0x10000u);
  ClockPageWindow window(host);
  CHECK(window.invariants_hold());
  ClockPageWindowProbe::set_heads(window, 1u);
  CHECK(!window.invariants_hold());
  ClockPageWindowProbe::set_heads(window, 0u);
  CHECK_STATUS(window.map(page, 4u), Status::ok);
  CHECK(window.invariants_hold());
  ClockPageWindowProbe::set_heads(window, 0u);
  CHECK(!window.invariants_hold());
  ClockPageWindowProbe::set_heads(window, ClockPageHeader::max_heads + 1u);
  CHECK(!window.invariants_hold());
  ClockPageWindowProbe::set_heads(window, 4u);
  CHECK(window.invariants_hold()); }
