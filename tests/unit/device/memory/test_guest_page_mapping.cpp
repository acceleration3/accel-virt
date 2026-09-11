#include "core/support/status.hpp"
#include "device/control/control_request.hpp"
#include "device/memory/blob_window.hpp"
#include "device/memory/guest_page_mapping.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>

using av::ControlRequest;
using av::GuestPageMapping;
using av::HostViolation;
using av::MockHost;
using av::MockRequest;
using av::Status;

namespace av {

struct GuestPageMappingProbe {
    static void miscount(GuestPageMapping &mapping) { mapping.length_ += 1u; }
    static void overcount(GuestPageMapping &mapping) { mapping.count_ = mapping.capacity_ + 1u; }
    static void lose_table(GuestPageMapping &mapping) { mapping.capacity_ = 1u; }
    /* Back to an empty mapping, so destruction releases nothing it never held. */
    static void repair(GuestPageMapping &mapping)
    {
        mapping.count_ = 0u;
        mapping.capacity_ = 0u;
        mapping.length_ = 0u;
    }
};

}  // namespace av

namespace {

constexpr std::uint64_t ram = 0x100000u;
constexpr std::size_t   header = 56u;

/* A request whose memory entries start at byte 56, as RESOURCE_CREATE_BLOB's do. */
class EntryList {
public:
    explicit EntryList(std::uint32_t capacity)
        : bytes_(static_cast<std::uint8_t *>(std::calloc(header + capacity * 16u, 1u))), count_(0u) {}
    ~EntryList() { std::free(bytes_); }
    EntryList &add(std::uint64_t address, std::uint32_t length)
    {
        std::memcpy(bytes_ + header + count_ * 16u, &address, 8);
        std::memcpy(bytes_ + header + count_ * 16u + 8u, &length, 4);
        count_++;
        return *this;
    }
    std::size_t size() const { return header + count_ * 16u; }
    const std::uint8_t *bytes() const { return bytes_; }

private:
    std::uint8_t *bytes_;
    std::uint32_t count_;
};

class Fixture {
public:
    Fixture() : mapping_(host_) { host_.add_guest_ram(ram, 0x10000u); }
    MockHost         &host() { return host_; }
    GuestPageMapping &mapping() { return mapping_; }
    Status map(const EntryList &list, std::uint32_t entries)
    {
        MockRequest mock(list.bytes(), list.size());
        ControlRequest request(host_, &mock);
        return mapping_.map(request, header, entries);
    }

private:
    MockHost         host_;
    GuestPageMapping mapping_;
};

}  // namespace

TEST(guest_page_mapping_map, maps_each_entry_until_released)
{ Fixture fixture;
  EntryList list(2u);
  list.add(ram, 0x1000u).add(ram + 0x3000u, 0x2000u);
  CHECK_STATUS(fixture.map(list, 2u), Status::ok);
  CHECK(fixture.mapping().invariants_hold());
  CHECK_EQ(fixture.mapping().length(), 0x3000u);
  CHECK_EQ(fixture.mapping().extents(), 2u);
  CHECK_EQ(fixture.host().live_guest_mappings(), 2u);
  CHECK_STATUS(fixture.map(list, 2u), Status::invalid_state);
  fixture.mapping().release();
  CHECK(fixture.mapping().invariants_hold());
  CHECK_EQ(fixture.mapping().length(), 0u);
  CHECK_EQ(fixture.host().live_guest_mappings(), 0u);
  CHECK_EQ(fixture.host().live_allocations(), 0u);
  fixture.mapping().release(); }

TEST(guest_page_mapping_map, an_entry_across_region_boundaries_takes_as_many_extents_as_it_needs)
{ Fixture fixture;
  fixture.host().split_guest_memory_at(ram + 0x1000u);
  fixture.host().split_guest_memory_at(ram + 0x2000u);
  EntryList list(1u);
  list.add(ram, 0x3000u);
  CHECK_STATUS(fixture.map(list, 1u), Status::ok);
  CHECK(fixture.mapping().invariants_hold());
  CHECK_EQ(fixture.mapping().extents(), 3u);
  CHECK_EQ(fixture.mapping().length(), 0x3000u); }

TEST(guest_page_mapping_map, a_bad_list_or_memory_that_is_not_guest_ram_holds_nothing)
{ struct Case {
      std::uint64_t address;
      std::uint32_t length;
      std::uint32_t entries;
      Status        status;
  };
  const Case cases[] = {
      {ram, 0x1000u, 0u, Status::invalid_parameter},                        /* no entries */
      {ram, 0x1000u, GuestPageMapping::max_entries + 1u, Status::invalid_parameter},
      {ram, 0x1000u, 2u, Status::invalid_parameter},                        /* the list holds one */
      {ram, 0u, 1u, Status::invalid_parameter},                             /* empty entry */
      {0xFFFFFFFFFFFFF000ull, 0x2000u, 1u, Status::invalid_parameter},     /* wraps */
      {0x900000u, 0x1000u, 1u, Status::not_found},                          /* nothing there */
      {0x800000u, 0x1000u, 1u, Status::not_supported},                      /* bounce */
  };
  for (const Case &c : cases) {
      Fixture fixture;
      fixture.host().add_bounce_region(0x800000u, 0x10000u);
      EntryList single(1u);
      single.add(c.address, c.length);
      CHECK_STATUS(fixture.map(single, c.entries), c.status);
      CHECK(fixture.mapping().invariants_hold());
      CHECK_EQ(fixture.mapping().length(), 0u);
      CHECK_EQ(fixture.host().live_guest_mappings(), 0u);
      CHECK_EQ(fixture.host().live_allocations(), 0u);
  } }

TEST(guest_page_mapping_map, a_failing_entry_stops_the_list_and_releases_the_rest)
{ Fixture later;
  EntryList list(2u);
  list.add(ram, 0x1000u).add(0x900000u, 0x1000u);
  CHECK_STATUS(later.map(list, 2u), Status::not_found);
  CHECK_EQ(later.host().live_guest_mappings(), 0u);
  CHECK_EQ(later.host().live_allocations(), 0u);
  Fixture first;
  EntryList reversed(2u);
  reversed.add(0x900000u, 0x1000u).add(ram, 0x1000u);
  CHECK_STATUS(first.map(reversed, 2u), Status::not_found);
  CHECK_EQ(first.host().live_guest_mappings(), 0u);
  Fixture empty;
  EntryList one(1u);
  one.add(ram, 0x1000u);
  empty.host().empty_mapping_at(1u);
  CHECK_STATUS(empty.map(one, 1u), Status::not_found);
  CHECK_EQ(empty.host().live_allocations(), 0u); }

TEST(guest_page_mapping_map, the_host_running_out_of_memory_holds_nothing)
{ Fixture table;
  EntryList list(1u);
  list.add(ram, 0x2000u);
  table.host().fail_allocation_at(1u);
  CHECK_STATUS(table.map(list, 1u), Status::no_memory);
  Fixture growth;
  growth.host().split_guest_memory_at(ram + 0x1000u);
  growth.host().fail_allocation_at(2u);
  CHECK_STATUS(growth.map(list, 1u), Status::no_memory);
  CHECK_EQ(growth.host().live_guest_mappings(), 0u);
  CHECK_EQ(growth.host().live_allocations(), 0u); }

TEST(guest_page_mapping_window, reaches_the_guest_bytes_across_extents)
{ Fixture fixture;
  fixture.host().split_guest_memory_at(ram + 0x1000u);
  EntryList list(1u);
  list.add(ram + 0x800u, 0x1000u);
  CHECK(fixture.mapping().window().length() == 0u);
  CHECK_STATUS(fixture.map(list, 1u), Status::ok);
  const av::BlobWindow window = fixture.mapping().window();
  CHECK(window.invariants_hold());
  CHECK_EQ(window.length(), 0x1000u);
  const std::uint8_t pattern[4] = {1u, 2u, 3u, 4u};
  CHECK_STATUS(window.write(0x7FEu, pattern, sizeof pattern), Status::ok);
  CHECK_EQ(fixture.host().guest_ram(ram + 0xFFEu)[0], 1u);
  CHECK_EQ(fixture.host().guest_ram(ram + 0x1001u)[0], 4u);
  fixture.host().guest_ram(ram + 0x800u)[0] = 0x5Au;
  std::uint8_t first = 0u;
  CHECK_STATUS(window.read(0u, &first, 1u), Status::ok);
  CHECK_EQ(first, 0x5Au); }

TEST(guest_page_mapping, maps_on_the_main_loop)
{ Fixture fixture;
  void *thread = nullptr;
  CHECK_STATUS(fixture.host().start_thread(
                   [](void *context) {
                       auto *const f = static_cast<Fixture *>(context);
                       EntryList list(1u);
                       list.add(0x900000u, 0x1000u);
                       static_cast<void>(f->map(list, 0u));
                       f->mapping().release();
                   },
                   &fixture, thread),
               Status::ok);
  fixture.host().join_thread(thread);
  CHECK(fixture.host().claim(HostViolation::main_loop_required));
  CHECK(fixture.host().claim(HostViolation::main_loop_required)); }

TEST(guest_page_mapping_invariants, a_wrong_length_count_or_table_is_detected)
{ Fixture miscounted;
  av::GuestPageMappingProbe::miscount(miscounted.mapping());
  CHECK(!miscounted.mapping().invariants_hold());
  av::GuestPageMappingProbe::repair(miscounted.mapping());
  Fixture overcounted;
  av::GuestPageMappingProbe::overcount(overcounted.mapping());
  CHECK(!overcounted.mapping().invariants_hold());
  av::GuestPageMappingProbe::repair(overcounted.mapping());
  Fixture lost;
  av::GuestPageMappingProbe::lose_table(lost.mapping());
  CHECK(!lost.mapping().invariants_hold());
  av::GuestPageMappingProbe::repair(lost.mapping()); }
