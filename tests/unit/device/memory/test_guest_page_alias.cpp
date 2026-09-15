/* One host range over a guest blob's scattered pages (feature 045, T011): the
 * host GPU imports a pointer, and a pointer names contiguous memory, so the
 * blob's extents are mapped again side by side. Every test runs over MockHost's
 * file-backed guest RAM, so the alias is a real second mapping and a byte is
 * judged through both addresses. */
#include "core/support/status.hpp"
#include "device/memory/blob_window.hpp"
#include "device/memory/guest_page_alias.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>

using av::BlobWindow;
using av::GuestPageAlias;
using av::HostViolation;
using av::MockHost;
using av::Status;

namespace av {

struct GuestPageAliasProbe {
    static void lose_base(GuestPageAlias &alias) { alias.base_ = nullptr; }
    static void misalign_length(GuestPageAlias &alias) { alias.length_ += 1u; }
    static void overcount_runs(GuestPageAlias &alias) { alias.runs_ = static_cast<std::uint32_t>(alias.length_ / GuestPageAlias::page_size) + 1u; }
    static void count_runs_unmapped(GuestPageAlias &alias) { alias.runs_ = 1u; }
    static void repair(GuestPageAlias &alias, void *base, std::uint64_t length, std::uint32_t runs)
    {
        alias.base_ = base;
        alias.length_ = length;
        alias.runs_ = runs;
    }
};

}  // namespace av

namespace {

constexpr std::uint64_t page = GuestPageAlias::page_size;
constexpr std::uint64_t ram = 0x40000000u;
constexpr std::uint32_t ram_pages = 64u;

/* A blob's extents as GuestPageMapping holds them: up to `capacity` of them,
 * each a pointer into the mock's guest RAM and a length. */
class Extents {
public:
    explicit Extents(std::uint32_t capacity)
        : pointers_(static_cast<void **>(std::calloc(capacity, sizeof(void *)))),
          lengths_(static_cast<std::uint64_t *>(std::calloc(capacity, sizeof(std::uint64_t)))), count_(0u),
          length_(0u) {}
    ~Extents()
    {
        std::free(pointers_);
        std::free(lengths_);
    }
    Extents(const Extents &) = delete;
    Extents &operator=(const Extents &) = delete;

    Extents &add(void *pointer, std::uint64_t bytes)
    {
        pointers_[count_] = pointer;
        lengths_[count_] = bytes;
        count_++;
        length_ += bytes;
        return *this;
    }
    BlobWindow window() const { return BlobWindow(pointers_, lengths_, count_, length_); }

private:
    void         **pointers_;
    std::uint64_t *lengths_;
    std::uint32_t  count_;
    std::uint64_t  length_;
};

class Fixture {
public:
    Fixture()
    {
        host_.add_guest_ram(ram, ram_pages * page);
        for (std::uint32_t index = 0u; index < ram_pages; index++)
            std::memset(guest_page(index), static_cast<int>(0x40u + index), page);
    }

    MockHost &host() { return host_; }
    void     *guest_page(std::uint32_t index) { return host_.guest_ram(ram + index * page); }

private:
    MockHost host_;
};

/* Pages 7, 2-3, 12, 13 (adjacent to 12), 40-42, 5 and 9: seven extents that
 * are six runs, ten pages. */
Extents &scattered(Fixture &fixture, Extents &extents)
{
    return extents.add(fixture.guest_page(7u), page)
        .add(fixture.guest_page(2u), 2u * page)
        .add(fixture.guest_page(12u), page)
        .add(fixture.guest_page(13u), page)
        .add(fixture.guest_page(40u), 3u * page)
        .add(fixture.guest_page(5u), page)
        .add(fixture.guest_page(9u), page);
}

}  // namespace

TEST(guest_page_alias_map, every_offset_of_the_range_is_the_guest_byte_of_its_extent_both_ways)
{ Fixture fixture;
  Extents extents(8u);
  GuestPageAlias alias(fixture.host());
  CHECK(!alias.mapped());
  CHECK(alias.base() == nullptr);
  CHECK(alias.invariants_hold());
  CHECK_STATUS(alias.map(scattered(fixture, extents).window()), Status::ok);
  CHECK(alias.invariants_hold());
  CHECK(alias.mapped());
  CHECK_EQ(alias.length(), 10u * page);
  const std::uint32_t order[] = {7u, 2u, 3u, 12u, 13u, 40u, 41u, 42u, 5u, 9u};
  auto *const         view = static_cast<std::uint8_t *>(alias.base());
  for (std::uint32_t index = 0u; index < 10u; index++)
      CHECK(std::memcmp(view + index * page, fixture.guest_page(order[index]), page) == 0);
  view[5u * page + 0x123u] = 0xA1u;
  CHECK_EQ(static_cast<std::uint8_t *>(fixture.guest_page(40u))[0x123u], 0xA1u);
  static_cast<std::uint8_t *>(fixture.guest_page(9u))[page - 1u] = 0x1Au;
  CHECK_EQ(view[10u * page - 1u], 0x1Au); }

TEST(guest_page_alias_map, extents_adjacent_in_guest_memory_are_one_mapping)
{ Fixture fixture;
  Extents extents(8u);
  GuestPageAlias alias(fixture.host());
  CHECK_STATUS(alias.map(scattered(fixture, extents).window()), Status::ok);
  CHECK_EQ(alias.runs(), 6u);                          /* 12 and 13 merged; the rest stand alone */
  CHECK_EQ(fixture.host().live_aliases(), 6u);
  Extents contiguous(4u);
  contiguous.add(fixture.guest_page(20u), page).add(fixture.guest_page(21u), 2u * page).add(fixture.guest_page(23u), page);
  GuestPageAlias whole(fixture.host());
  CHECK_STATUS(whole.map(contiguous.window()), Status::ok);
  CHECK_EQ(whole.runs(), 1u);
  CHECK_EQ(fixture.host().live_aliases(), 7u);
  CHECK(std::memcmp(static_cast<std::uint8_t *>(whole.base()) + 3u * page, fixture.guest_page(23u), page) == 0);
  /* The page before an extent is not adjacent to it: order matters, not membership. */
  Extents backwards(2u);
  backwards.add(fixture.guest_page(31u), page).add(fixture.guest_page(30u), page);
  GuestPageAlias reversed(fixture.host());
  CHECK_STATUS(reversed.map(backwards.window()), Status::ok);
  CHECK_EQ(reversed.runs(), 2u);
  CHECK(reversed.invariants_hold()); }

TEST(guest_page_alias_map, a_blob_of_exactly_the_run_limit_maps_and_one_more_run_is_refused_unreserved)
{ Fixture fixture;
  Extents limit(GuestPageAlias::max_runs + 1u);
  for (std::uint32_t index = 0u; index < GuestPageAlias::max_runs; index++) limit.add(fixture.guest_page(index % 2u * 3u), page);
  GuestPageAlias alias(fixture.host());
  CHECK_STATUS(alias.map(limit.window()), Status::ok);
  CHECK_EQ(alias.runs(), GuestPageAlias::max_runs);
  CHECK(alias.invariants_hold());
  alias.release();
  CHECK(alias.invariants_hold());
  CHECK_EQ(fixture.host().live_ranges(), 0u);
  limit.add(fixture.guest_page(6u), page);
  fixture.host().fail_reserve_at(1u);
  CHECK_STATUS(alias.map(limit.window()), Status::no_memory);
  CHECK(alias.invariants_hold());
  CHECK(!alias.mapped());
  /* Refused before the host was asked: the reservation it was told to fail is still to come. */
  void *base = nullptr;
  CHECK_STATUS(fixture.host().reserve_range(page, base), Status::no_memory); }

TEST(guest_page_alias_map, extents_that_are_not_whole_pages_or_are_empty_are_malformed_and_hold_nothing)
{ Fixture fixture;
  const auto *const second = static_cast<std::uint8_t *>(fixture.guest_page(1u));
  struct Case { void *pointer; std::uint64_t bytes; };
  const Case cases[] = {
      {static_cast<std::uint8_t *>(fixture.guest_page(1u)) + 1, page},   /* misaligned base */
      {fixture.guest_page(1u), page - 1u},                              /* short of a page */
      {fixture.guest_page(1u), page + 1u},                              /* a byte over */
      {fixture.guest_page(1u), 0u},                                     /* empty extent */
      {nullptr, page},                                                  /* no memory behind it */
  };
  for (const Case &c : cases) {
      Extents extents(2u);
      extents.add(fixture.guest_page(0u), page).add(c.pointer, c.bytes);
      GuestPageAlias alias(fixture.host());
      CHECK_STATUS(alias.map(extents.window()), Status::malformed);
      CHECK(!alias.mapped());
      CHECK(alias.invariants_hold());
      CHECK_EQ(fixture.host().live_ranges(), 0u);
  }
  GuestPageAlias empty(fixture.host());
  CHECK_STATUS(empty.map(BlobWindow()), Status::malformed);
  CHECK_EQ(second[0], 0x41u);
  CHECK_EQ(fixture.host().live_ranges(), 0u); }

TEST(guest_page_alias_map, a_host_that_cannot_reserve_or_alias_refuses_and_releases_what_it_made)
{ Fixture fixture;
  Extents extents(8u);
  scattered(fixture, extents);
  GuestPageAlias alias(fixture.host());
  fixture.host().fail_reserve_at(1u);
  CHECK_STATUS(alias.map(extents.window()), Status::no_memory);
  CHECK(alias.invariants_hold());
  CHECK(alias.base() == nullptr);
  for (std::uint32_t failing = 1u; failing <= 6u; failing++) {
      fixture.host().fail_alias_at(failing);
      CHECK_STATUS(alias.map(extents.window()), Status::not_supported);
      CHECK(alias.invariants_hold());
      CHECK(!alias.mapped());
      CHECK_EQ(alias.runs(), 0u);
      CHECK_EQ(fixture.host().live_ranges(), 0u);
  }
  std::uint8_t *const unfiled = static_cast<std::uint8_t *>(std::aligned_alloc(page, page));
  Extents foreign(2u);
  foreign.add(fixture.guest_page(0u), page).add(unfiled, page);
  CHECK_STATUS(alias.map(foreign.window()), Status::not_supported);   /* RAM with no file behind it */
  CHECK_EQ(fixture.host().live_ranges(), 0u);
  std::free(unfiled);
  CHECK_STATUS(alias.map(extents.window()), Status::ok);
  CHECK(alias.invariants_hold()); }

TEST(guest_page_alias_map, a_mapped_alias_refuses_a_second_map_and_keeps_its_first)
{ Fixture fixture;
  Extents first(1u);
  first.add(fixture.guest_page(8u), page);
  Extents second(1u);
  second.add(fixture.guest_page(9u), page);
  GuestPageAlias alias(fixture.host());
  CHECK_STATUS(alias.map(first.window()), Status::ok);
  const void *const base = alias.base();
  CHECK_STATUS(alias.map(second.window()), Status::invalid_state);
  CHECK(alias.invariants_hold());
  CHECK(alias.base() == base);
  CHECK_EQ(static_cast<const std::uint8_t *>(alias.base())[0], 0x48u);
  CHECK_EQ(fixture.host().live_ranges(), 1u); }

TEST(guest_page_alias_release, lets_go_of_every_mapping_and_can_map_again_and_destruction_releases)
{ Fixture fixture;
  Extents extents(8u);
  scattered(fixture, extents);
  {
      GuestPageAlias alias(fixture.host());
      CHECK_STATUS(alias.map(extents.window()), Status::ok);
      alias.release();
      CHECK(alias.invariants_hold());
      CHECK(!alias.mapped());
      CHECK_EQ(alias.length(), 0u);
      CHECK_EQ(alias.runs(), 0u);
      CHECK_EQ(fixture.host().live_ranges(), 0u);
      alias.release();
      CHECK_EQ(fixture.host().unclaimed_violations(), 0u);
      CHECK_STATUS(alias.map(extents.window()), Status::ok);
      CHECK_EQ(fixture.host().live_ranges(), 1u);
  }
  CHECK_EQ(fixture.host().live_ranges(), 0u);
  CHECK_EQ(fixture.host().live_aliases(), 0u);
  CHECK_EQ(static_cast<std::uint8_t *>(fixture.guest_page(40u))[0], 0x68u); }

TEST(guest_page_alias_map, may_run_on_a_worker)
{ Fixture fixture;
  Extents extents(8u);
  scattered(fixture, extents);
  struct Call { GuestPageAlias *alias; const Extents *extents; Status status; };
  GuestPageAlias alias(fixture.host());
  Call call{&alias, &extents, Status::invalid_state};
  void *thread = nullptr;
  CHECK_STATUS(fixture.host().start_thread([](void *context) {
                   auto *const c = static_cast<Call *>(context);
                   c->status = c->alias->map(c->extents->window());
               }, &call, thread), Status::ok);
  fixture.host().join_thread(thread);
  CHECK_STATUS(call.status, Status::ok);
  CHECK_EQ(fixture.host().unclaimed_violations(), 0u); }

TEST(guest_page_alias_invariants, each_rule_is_detected)
{ Fixture fixture;
  Extents extents(8u);
  scattered(fixture, extents);
  GuestPageAlias alias(fixture.host());
  CHECK_STATUS(alias.map(extents.window()), Status::ok);
  const void *const base = alias.base();
  auto *const mutable_base = static_cast<void *>(static_cast<std::uint8_t *>(alias.base()));
  const std::uint64_t length = alias.length();
  const std::uint32_t runs = alias.runs();
  av::GuestPageAliasProbe::lose_base(alias);
  CHECK(!alias.invariants_hold());
  av::GuestPageAliasProbe::repair(alias, mutable_base, length, runs);
  av::GuestPageAliasProbe::misalign_length(alias);
  CHECK(!alias.invariants_hold());
  av::GuestPageAliasProbe::repair(alias, mutable_base, length, runs);
  av::GuestPageAliasProbe::overcount_runs(alias);
  CHECK(!alias.invariants_hold());
  av::GuestPageAliasProbe::repair(alias, mutable_base, length, runs);
  /* Every run a page long, and one run more than an alias may spend. */
  av::GuestPageAliasProbe::repair(alias, mutable_base, (GuestPageAlias::max_runs + 1u) * page, GuestPageAlias::max_runs + 1u);
  CHECK(!alias.invariants_hold());
  av::GuestPageAliasProbe::repair(alias, mutable_base, GuestPageAlias::max_runs * page, GuestPageAlias::max_runs);
  CHECK(alias.invariants_hold());
  av::GuestPageAliasProbe::repair(alias, mutable_base, length, runs);
  CHECK(alias.invariants_hold());
  CHECK(alias.base() == base);
  alias.release();
  av::GuestPageAliasProbe::count_runs_unmapped(alias);
  CHECK(!alias.invariants_hold());
  av::GuestPageAliasProbe::repair(alias, nullptr, 0u, 0u);
  CHECK(alias.invariants_hold()); }
