#include "core/support/status.hpp"
#include "device/memory/lent_pages.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"

#include <cstdint>

using av::LentPages;
using av::LentPagesProbe;
using av::MockHost;
using av::Status;

namespace av {

struct LentPagesProbe {
    static std::uint64_t *slots(LentPages &pages) { return pages.slots_; }
    static void           miscount(LentPages &pages) { pages.count_++; }
    static void           overuse(LentPages &pages) { pages.used_ = pages.capacity_ + 1u; }
    static void           repair(LentPages &pages, std::uint64_t count, std::uint64_t used)
    {
        pages.count_ = count;
        pages.used_ = used;
    }
};

}

namespace {

constexpr std::uint64_t page = 4096u;

}

TEST(lent_pages, a_claimed_range_is_lent_page_by_page_and_released_whole)
{ MockHost  host;
  LentPages lent(host);
  CHECK(!lent.lent(0x10000u));
  CHECK_EQ(lent.count(), 0u);
  CHECK_STATUS(lent.claim(0x10000u, 3u * page), Status::ok);
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
  CHECK_STATUS(lent.claim(0x20000u, 2u * page), Status::ok);
  CHECK_STATUS(lent.claim(0x20000u - page, 2u * page), Status::conflict);
  CHECK(!lent.lent(0x20000u - page));
  CHECK_STATUS(lent.claim(0x20000u + page, 2u * page), Status::conflict);
  CHECK(!lent.lent(0x20000u + 2u * page));
  CHECK_EQ(lent.count(), 2u);
  CHECK_STATUS(lent.claim(0x20000u + 2u * page, page), Status::ok);
  lent.release(0x20000u, 2u * page);
  CHECK_STATUS(lent.claim(0x20000u - page, 2u * page), Status::ok);
  CHECK(lent.invariants_hold()); }

TEST(lent_pages, the_set_grows_and_reuses_released_slots_without_losing_a_page)
{ MockHost  host;
  LentPages lent(host);
  for (std::uint64_t run = 0u; run < 64u; run++)
      CHECK_STATUS(lent.claim(run * 1024u * page, 100u * page), Status::ok);
  CHECK_EQ(lent.count(), 6400u);
  CHECK(lent.invariants_hold());
  for (std::uint64_t run = 0u; run < 64u; run += 2u) lent.release(run * 1024u * page, 100u * page);
  CHECK_EQ(lent.count(), 3200u);
  for (std::uint64_t run = 0u; run < 64u; run++)
      CHECK_EQ(lent.lent(run * 1024u * page + 99u * page), run % 2u == 1u);
  for (std::uint64_t run = 0u; run < 64u; run += 2u)
      CHECK_STATUS(lent.claim(run * 1024u * page, 100u * page), Status::ok);
  CHECK_EQ(lent.count(), 6400u);
  CHECK(lent.invariants_hold());
  lent.release(0xF0000000u, 4u * page);
  CHECK_EQ(lent.count(), 6400u); }

TEST(lent_pages, a_set_that_cannot_grow_refuses_and_keeps_what_it_had)
{ MockHost  host;
  LentPages lent(host);
  host.fail_allocation_at(1u);
  CHECK_STATUS(lent.claim(0x1000u, page), Status::no_memory);
  CHECK_EQ(lent.count(), 0u);
  CHECK(!lent.lent(0x1000u));
  CHECK_STATUS(lent.claim(0x1000u, page), Status::ok);
  host.fail_allocation_at(1u);
  CHECK_STATUS(lent.claim(0x100000u, 1024u * page), Status::no_memory);
  CHECK(lent.lent(0x1000u));
  CHECK(!lent.lent(0x100000u));
  CHECK_EQ(lent.count(), 1u);
  CHECK(lent.invariants_hold()); }

TEST(lent_pages_invariants, counts_that_are_not_the_tables_break_them)
{ MockHost  host;
  LentPages lent(host);
  CHECK_STATUS(lent.claim(0x4000u, 2u * page), Status::ok);
  LentPagesProbe::miscount(lent);
  CHECK(!lent.invariants_hold());
  LentPagesProbe::repair(lent, 2u, 2u);
  CHECK(lent.invariants_hold());
  LentPagesProbe::overuse(lent);
  CHECK(!lent.invariants_hold());
  LentPagesProbe::repair(lent, 2u, 2u);
  std::uint64_t *const slots = LentPagesProbe::slots(lent);
  std::uint64_t        moved = 0u;
  for (std::uint64_t slot = 0u; slot < 256u; slot++) {
      if (slots[slot] != 0x4000u / page) continue;
      slots[(slot + 128u) % 256u] = slots[slot];
      slots[slot] = ~std::uint64_t{0};
      moved++;
      break;
  }
  CHECK_EQ(moved, 1u);
  CHECK(!lent.invariants_hold()); }
