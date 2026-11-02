/* Guest pages lent through two adapters' devices of one process: the second
 * device finds the very pages the first lent and holds the same lending
 * instead of laying memory of its own, the pages go back to the guest only
 * once neither holds them, and a lending is joined only by the very pages. */
#include "core/support/status.hpp"
#include "device/control/control_request.hpp"
#include "device/memory/guest_ram_lendings.hpp"
#include "device/memory/lending_life.hpp"
#include "device/memory/lent_run.hpp"
#include "harness/context/recording_context_plane.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

using av::ControlRequest;
using av::GuestRamLending;
using av::GuestRamLendings;
using av::MockHost;
using av::MockRequest;
using av::RecordingContextPlane;
using av::Status;

namespace {

constexpr std::uint64_t ram = 0x100000u;
constexpr std::uint64_t page = 4096u;
constexpr std::size_t   header = 32u;

/* An ATTACH_BACKING's entries, from byte 32. */
class EntryList {
public:
    explicit EntryList(std::uint32_t capacity)
        : bytes_(static_cast<std::uint8_t *>(std::calloc(header + capacity * 16u, 1u))), count_(0u) {}
    ~EntryList() { std::free(bytes_); }
    EntryList(const EntryList &) = delete;
    EntryList &operator=(const EntryList &) = delete;
    EntryList &add(std::uint64_t address, std::uint32_t length)
    {
        std::memcpy(bytes_ + header + count_ * 16u, &address, 8);
        std::memcpy(bytes_ + header + count_ * 16u + 8u, &length, 4);
        count_++;
        return *this;
    }
    std::size_t         size() const { return header + count_ * 16u; }
    const std::uint8_t *bytes() const { return bytes_; }

private:
    std::uint8_t *bytes_;
    std::uint32_t count_;
};

/* The memory a lender's blob was made of: a file, as the executor's dma-buf. */
int make_memory(std::uint64_t size)
{
    const int fd = memfd_create("lent", MFD_CLOEXEC);
    if (fd < 0 || ftruncate(fd, static_cast<off_t>(size)) != 0) std::abort();
    return fd;
}

class Fixture {
public:
    Fixture() : lendings_(host_) { host_.add_guest_ram(ram, 0x10000u); }
    MockHost              &host() { return host_; }
    GuestRamLendings      &lendings() { return lendings_; }
    RecordingContextPlane &first() { return first_; }
    RecordingContextPlane &second() { return second_; }
    /* The lender's pieces: the pages from ram + 2 pages, laid whole. */
    GuestRamLending *lend_two_pages()
    {
        GuestRamLending *const lending = lendings_.lend(host_, 2u * page, make_memory(2u * page), first_, 7u);
        EntryList list(1u);
        list.add(ram + 2u * page, static_cast<std::uint32_t>(2u * page));
        MockRequest    mock(list.bytes(), list.size());
        ControlRequest request(host_, &mock);
        CHECK(lending != nullptr);
        CHECK_STATUS(lending->lay(host_, request, header, 1u), Status::ok);
        return lending;
    }

private:
    MockHost              host_;
    GuestRamLendings      lendings_;
    RecordingContextPlane first_;
    RecordingContextPlane second_;
};

}  // namespace

/* The other adapter's device lends the same pages: one lending, two holders,
 * and the pages stay under it however the two leave. */
TEST(guest_ram_lendings_join, the_same_pages_lent_through_another_plane_are_one_lending)
{ Fixture fixture;
  GuestRamLending *const lender = fixture.lend_two_pages();
  CHECK_EQ(fixture.lendings().pages(), 2u);
  GuestRamLending *const joined = fixture.lendings().join(ram + 2u * page, 2u * page, fixture.second(), 9u);
  CHECK(joined == lender);
  CHECK_EQ(lender->holders(), 2u);
  CHECK_EQ(fixture.lendings().live(), 1u);
  /* The lender goes first: nothing is given back while the joiner holds. */
  fixture.lendings().leave(fixture.host(), *lender, fixture.first(), 7u);
  CHECK_EQ(fixture.lendings().live(), 1u);
  CHECK_EQ(fixture.lendings().pages(), 2u);
  CHECK_EQ(fixture.host().overlaid_bytes(), 2u * page);
  /* A third plane still finds it, through the joiner's memory. */
  av::ContextPlane *holder = nullptr;
  std::uint64_t     key = 0u;
  CHECK(lender->holder_at(0u, holder, key));
  CHECK(holder == &fixture.second() && key == 9u);
  fixture.lendings().leave(fixture.host(), *lender, fixture.second(), 9u);
  CHECK_EQ(fixture.lendings().live(), 0u);
  CHECK_EQ(fixture.lendings().pages(), 0u);
  CHECK_EQ(fixture.host().overlaid_bytes(), 0u);
  CHECK(fixture.lendings().invariants_hold()); }

/* Joined only by the very pages: another address, another size, pages not
 * laid whole yet, and the same plane's second lending are all new memory (or
 * the refusal they always were). */
TEST(guest_ram_lendings_join, anything_but_the_very_pages_from_another_plane_is_not_joined)
{ Fixture fixture;
  GuestRamLending *const lender = fixture.lend_two_pages();
  CHECK(fixture.lendings().join(ram + 3u * page, 2u * page, fixture.second(), 9u) == nullptr);
  CHECK(fixture.lendings().join(ram + 2u * page, 3u * page, fixture.second(), 9u) == nullptr);
  CHECK(fixture.lendings().join(ram + 2u * page, 2u * page, fixture.first(), 8u) == nullptr);
  CHECK(fixture.lendings().join(ram + 9u * page, 2u * page, fixture.second(), 9u) == nullptr);
  CHECK_EQ(lender->holders(), 1u);
  /* Held by no plane but the asking one is not held beyond it. */
  CHECK(!lender->held_beyond(fixture.first()));
  CHECK(lender->held_beyond(fixture.second()));
  fixture.lendings().leave(fixture.host(), *lender, fixture.first(), 7u);
  CHECK_EQ(fixture.lendings().live(), 0u); }

/* A lending whose pieces are still coming is not joined. */
TEST(guest_ram_lendings_join, a_lending_not_yet_whole_is_not_joined)
{ Fixture fixture;
  GuestRamLending *const lender = fixture.lendings().lend(fixture.host(), 2u * page, make_memory(2u * page),
                                                          fixture.first(), 7u);
  EntryList list(1u);
  list.add(ram + 2u * page, static_cast<std::uint32_t>(page));
  MockRequest    mock(list.bytes(), list.size());
  ControlRequest request(fixture.host(), &mock);
  CHECK_STATUS(lender->lay(fixture.host(), request, header, 1u), Status::ok);
  CHECK(!lender->complete());
  CHECK(fixture.lendings().join(ram + 2u * page, 2u * page, fixture.second(), 9u) == nullptr);
  fixture.lendings().leave(fixture.host(), *lender, fixture.first(), 7u);
  CHECK_EQ(fixture.lendings().pages(), 0u); }

/* The joiner's pieces are the lender's entries, in order, or nothing. */
TEST(guest_ram_lending_verify, a_joiners_entries_must_be_the_ones_the_lender_laid)
{ Fixture fixture;
  GuestRamLending *const lender = fixture.lend_two_pages();
  EntryList same(1u);
  same.add(ram + 2u * page, static_cast<std::uint32_t>(2u * page));
  MockRequest    same_mock(same.bytes(), same.size());
  ControlRequest same_request(fixture.host(), &same_mock);
  std::uint32_t  next = 0u;
  CHECK_STATUS(lender->verify(same_request, header, 1u, next), Status::ok);
  CHECK_EQ(next, 1u);
  /* A second entry past what the lender laid. */
  CHECK_STATUS(lender->verify(same_request, header, 1u, next), Status::invalid_parameter);
  EntryList other(1u);
  other.add(ram + 2u * page, static_cast<std::uint32_t>(page));
  MockRequest    other_mock(other.bytes(), other.size());
  ControlRequest other_request(fixture.host(), &other_mock);
  next = 0u;
  CHECK_STATUS(lender->verify(other_request, header, 1u, next), Status::invalid_parameter);
  CHECK_EQ(next, 0u);
  fixture.lendings().leave(fixture.host(), *lender, fixture.first(), 7u); }

/* Where a present's destination lies in a lending: an entry inside the pages
 * lent, entries that follow each other as the pages do, and across two
 * entries of the lender's that are adjacent in guest memory; but not entries
 * out of order, pages of another place, or pages not laid whole. */
TEST(guest_ram_lendings_locate, finds_the_lending_and_the_offset_of_a_run_in_order)
{ Fixture fixture;
  GuestRamLending *const lender = fixture.lend_two_pages();
  const std::uint64_t first = ram + 2u * page;
  const auto locate = [&](const EntryList &list, av::LentRun &run) {
      MockRequest    mock(list.bytes(), list.size());
      ControlRequest request(fixture.host(), &mock);
      return fixture.lendings().locate(request, header, 2u, run);
  };
  const auto locate_one = [&](const EntryList &list, av::LentRun &run) {
      MockRequest    mock(list.bytes(), list.size());
      ControlRequest request(fixture.host(), &mock);
      return fixture.lendings().locate(request, header, 1u, run);
  };
  av::LentRun run;
  CHECK_STATUS(locate_one(EntryList(1u).add(first + page, static_cast<std::uint32_t>(page)), run), Status::ok);
  CHECK(run.lending() == lender);
  CHECK_EQ(run.offset(), page);
  CHECK_EQ(run.length(), page);
  CHECK_STATUS(locate(EntryList(2u).add(first, static_cast<std::uint32_t>(page)).add(first + page, static_cast<std::uint32_t>(page)), run),
               Status::ok);
  CHECK_EQ(run.offset(), 0u);
  CHECK_EQ(run.length(), 2u * page);
  /* Out of order, or leaving the pages lent. */
  CHECK_STATUS(locate(EntryList(2u).add(first + page, static_cast<std::uint32_t>(page)).add(first, static_cast<std::uint32_t>(page)), run),
               Status::not_supported);
  CHECK_STATUS(locate_one(EntryList(1u).add(first + page, static_cast<std::uint32_t>(2u * page)), run), Status::not_supported);
  CHECK_STATUS(locate_one(EntryList(1u).add(ram + 8u * page, static_cast<std::uint32_t>(page)), run), Status::not_found);
  CHECK_STATUS(locate(EntryList(1u).add(first, static_cast<std::uint32_t>(page)), run), Status::invalid_parameter);   /* a short list */
  fixture.lendings().leave(fixture.host(), *lender, fixture.first(), 7u);
  CHECK(fixture.lendings().invariants_hold()); }

/* A lend and a give-back each hold the writers still once - the engines out of
 * the pages and the guest's processors - however many runs they swap; and the
 * lending's life, which a present on another thread reads, says it is given
 * back before the RAM is. */
TEST(guest_ram_lendings_swaps, hold_the_writers_still_once_for_each_lay_and_each_give_back)
{ Fixture fixture;
  CHECK_EQ(fixture.host().guest_pauses(), 0u);
  GuestRamLending *const lender = fixture.lend_two_pages();
  CHECK_EQ(fixture.host().guest_pauses(), 1u);
  CHECK_EQ(fixture.host().guest_resumes(), 1u);
  av::LendingLife *const watched = fixture.lendings().watch(*lender);
  CHECK(!watched->given_back());
  CHECK_EQ(fixture.lendings().oldest_live(), lender->id());
  fixture.lendings().leave(fixture.host(), *lender, fixture.first(), 7u);
  CHECK_EQ(fixture.host().guest_pauses(), 2u);
  CHECK_EQ(fixture.host().guest_resumes(), 2u);
  CHECK(watched->given_back());   /* still readable: the watcher holds it */
  CHECK_EQ(fixture.lendings().oldest_live(), 0u);
  fixture.lendings().unwatch(watched);
  CHECK(fixture.lendings().invariants_hold()); }
