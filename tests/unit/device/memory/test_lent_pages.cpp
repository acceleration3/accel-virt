#include "core/support/status.hpp"
#include "device/memory/lent_pages.hpp"
#include "device/memory/guest_ram_lending.hpp"
#include "harness/context/recording_context_plane.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"

#include <cstdint>

using av::GuestRamLending;
using av::LentPages;
using av::LentPagesProbe;
using av::MockHost;
using av::RecordingContextPlane;
using av::Status;

namespace av {

struct KeyedSlotsProbe {
    template <typename Slots>
    static void miscount(Slots &slots) { slots.count_++; }
    template <typename Slots>
    static void repair(Slots &slots) { slots.count_--; }
};

struct LentPagesProbe {
    static void miscount(LentPages &pages) { KeyedSlotsProbe::miscount(pages.pages_); }
    static void repair(LentPages &pages) { KeyedSlotsProbe::repair(pages.pages_); }
};

}

namespace {

constexpr std::uint64_t page = 4096u;

class Owner {
public:
    Owner(MockHost &host, LentPages &lent, std::uint64_t key = 1u) : lending_(host, lent, 0u, -1, plane_, key) {}
    GuestRamLending &lending() { return lending_; }

private:
    RecordingContextPlane plane_;
    GuestRamLending       lending_;
};

}

TEST(lent_pages, a_claimed_range_is_lent_page_by_page_and_released_whole)
{ MockHost  host;
  LentPages lent(host);
  Owner     owner(host, lent);
  CHECK(!lent.lent(0x10000u));
  CHECK_EQ(lent.count(), 0u);
  CHECK_STATUS(lent.claim(0x10000u, 3u * page, owner.lending()), Status::ok);
  CHECK(lent.lent(0x10000u));
  CHECK(lent.lent(0x10000u + 2u * page + 17u));
  CHECK(!lent.lent(0x10000u + 3u * page));
  CHECK(!lent.lent(0x10000u - page));
  CHECK_EQ(lent.count(), 3u);
  CHECK(lent.invariants_hold());
  lent.release(0x10000u, 3u * page);
  CHECK(!lent.lent(0x10000u + page));
  CHECK_EQ(lent.count(), 0u);
  CHECK(lent.invariants_hold()); }

TEST(lent_pages, a_claim_over_a_lent_page_is_refused_and_claims_nothing)
{ MockHost  host;
  LentPages lent(host);
  Owner     owner(host, lent);
  CHECK_STATUS(lent.claim(0x20000u, 2u * page, owner.lending()), Status::ok);
  CHECK_STATUS(lent.claim(0x20000u - page, 2u * page, owner.lending()), Status::conflict);
  CHECK(!lent.lent(0x20000u - page));
  CHECK_STATUS(lent.claim(0x20000u + page, 2u * page, owner.lending()), Status::conflict);
  CHECK(!lent.lent(0x20000u + 2u * page));
  CHECK_EQ(lent.count(), 2u);
  CHECK_STATUS(lent.claim(0x20000u + 2u * page, page, owner.lending()), Status::ok);
  lent.release(0x20000u, 2u * page);
  CHECK_STATUS(lent.claim(0x20000u - page, 2u * page, owner.lending()), Status::ok);
  CHECK(lent.invariants_hold()); }

TEST(lent_pages, the_set_grows_and_reuses_released_slots_without_losing_a_page)
{ MockHost  host;
  LentPages lent(host);
  Owner     owner(host, lent);
  for (std::uint64_t run = 0u; run < 64u; run++)
      CHECK_STATUS(lent.claim(run * 1024u * page, 100u * page, owner.lending()), Status::ok);
  CHECK_EQ(lent.count(), 6400u);
  CHECK(lent.invariants_hold());
  for (std::uint64_t run = 0u; run < 64u; run += 2u) lent.release(run * 1024u * page, 100u * page);
  CHECK_EQ(lent.count(), 3200u);
  for (std::uint64_t run = 0u; run < 64u; run++)
      CHECK_EQ(lent.lent(run * 1024u * page + 99u * page), run % 2u == 1u);
  for (std::uint64_t run = 0u; run < 64u; run += 2u)
      CHECK_STATUS(lent.claim(run * 1024u * page, 100u * page, owner.lending()), Status::ok);
  CHECK_EQ(lent.count(), 6400u);
  CHECK(lent.invariants_hold());
  lent.release(0xF0000000u, 4u * page);
  CHECK_EQ(lent.count(), 6400u); }

TEST(lent_pages, a_set_that_cannot_grow_refuses_and_keeps_what_it_had)
{ MockHost  host;
  LentPages lent(host);
  Owner     owner(host, lent);
  host.fail_allocation_at(1u);
  CHECK_STATUS(lent.claim(0x1000u, page, owner.lending()), Status::no_memory);
  CHECK_EQ(lent.count(), 0u);
  CHECK(!lent.lent(0x1000u));
  CHECK_STATUS(lent.claim(0x1000u, page, owner.lending()), Status::ok);
  host.fail_allocation_at(1u);
  CHECK_STATUS(lent.claim(0x100000u, 1024u * page, owner.lending()), Status::no_memory);
  CHECK(lent.lent(0x1000u));
  CHECK(!lent.lent(0x100000u));
  CHECK_EQ(lent.count(), 1u);
  CHECK(lent.invariants_hold()); }

TEST(lent_pages, every_page_names_the_lending_that_claimed_it)
{ MockHost  host;
  LentPages lent(host);
  Owner     first(host, lent, 1u);
  Owner     second(host, lent, 2u);
  CHECK(lent.owner_of(0x8000u) == nullptr);
  CHECK_STATUS(lent.claim(0x8000u, 2u * page, first.lending()), Status::ok);
  CHECK_STATUS(lent.claim(0x8000u + 2u * page, page, second.lending()), Status::ok);
  CHECK(lent.owner_of(0x8000u) == &first.lending());
  CHECK(lent.owner_of(0x8000u + page + 5u) == &first.lending());
  CHECK(lent.owner_of(0x8000u + 2u * page) == &second.lending());
  CHECK(lent.owner_of(0x8000u + 3u * page) == nullptr);
  lent.release(0x8000u, 2u * page);
  CHECK(lent.owner_of(0x8000u) == nullptr);
  CHECK(lent.owner_of(0x8000u + 2u * page) == &second.lending());
  CHECK(lent.invariants_hold());
  lent.release(0x8000u + 2u * page, page); }

TEST(lent_pages_invariants, a_count_that_is_not_the_tables_breaks_them)
{ MockHost  host;
  LentPages lent(host);
  Owner     owner(host, lent);
  CHECK_STATUS(lent.claim(0x4000u, 2u * page, owner.lending()), Status::ok);
  CHECK(lent.invariants_hold());
  LentPagesProbe::miscount(lent);
  CHECK(!lent.invariants_hold());
  LentPagesProbe::repair(lent);
  CHECK(lent.invariants_hold());
  lent.release(0x4000u, 2u * page); }
