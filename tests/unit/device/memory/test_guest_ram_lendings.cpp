#include "core/support/status.hpp"
#include "device/control/control_request.hpp"
#include "device/memory/guest_ram_lendings.hpp"
#include "harness/context/recording_context_plane.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace av {

struct GrowableArrayProbe {
    template <typename T>
    static void corrupt_capacity(GrowableArray<T> &array, std::uint32_t capacity) { array.capacity_ = capacity; }
};

struct KeyedSlotsProbe {
    template <typename Key, typename Entry>
    static void corrupt_count(KeyedSlots<Key, Entry> &table, std::uint32_t count) { table.count_ = count; }
};

struct ObjectPoolProbe {
    template <typename T>
    static void corrupt_live_count(ObjectPool<T> &pool) { pool.live_ += 1u; }
};

struct LentPagesProbe {
    static void miscount(LentPages &pages) { KeyedSlotsProbe::corrupt_count(pages.pages_, pages.pages_.count() + 3u); }
};

struct GuestPageOverlayProbe {
    static void miscount(GuestPageOverlay &overlay, std::uint64_t by) { overlay.covered_ += by; }
};

struct GuestRamLendingProbe {
    static void miscount_overlay(GuestRamLending &lending, std::uint64_t by)
    {
        GuestPageOverlayProbe::miscount(lending.overlay_, by);
    }
    static void break_holders(GuestRamLending &lending) { GrowableArrayProbe::corrupt_capacity(lending.holders_, 12u); }
    static void mend_holders(GuestRamLending &lending) { GrowableArrayProbe::corrupt_capacity(lending.holders_, 8u); }
    static void set_size(GuestRamLending &lending, std::uint64_t size) { lending.size_ = size; }
    static void set_fd(GuestRamLending &lending, int fd) { lending.fd_ = fd; }
    static int fd(const GuestRamLending &lending) { return lending.fd_; }
};

struct GuestRamLendingsProbe {
    static void miscount_pages(GuestRamLendings &lendings) { LentPagesProbe::miscount(lendings.pages_); }
    static void miscount_lendings(GuestRamLendings &lendings) { ObjectPoolProbe::corrupt_live_count(lendings.lendings_); }
};

}

using av::ControlRequest;
using av::GuestRamLendingProbe;
using av::GuestRamLendingsProbe;
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

}

TEST(guest_ram_lendings_join, the_same_pages_lent_through_another_plane_are_one_lending)
{ Fixture fixture;
  GuestRamLending *const lender = fixture.lend_two_pages();
  CHECK_EQ(fixture.lendings().pages(), 2u);
  GuestRamLending *const joined = fixture.lendings().join(ram + 2u * page, 2u * page, fixture.second(), 9u);
  CHECK(joined == lender);
  CHECK_EQ(lender->holders(), 2u);
  CHECK_EQ(fixture.lendings().live(), 1u);
  fixture.lendings().leave(fixture.host(), *lender, fixture.first(), 7u);
  CHECK_EQ(fixture.lendings().live(), 1u);
  CHECK_EQ(fixture.lendings().pages(), 2u);
  CHECK_EQ(fixture.host().overlaid_bytes(), 2u * page);
  av::ContextPlane *holder = nullptr;
  std::uint64_t     key = 0u;
  CHECK(lender->holder_at(0u, holder, key));
  CHECK(holder == &fixture.second() && key == 9u);
  fixture.lendings().leave(fixture.host(), *lender, fixture.second(), 9u);
  CHECK_EQ(fixture.lendings().live(), 0u);
  CHECK_EQ(fixture.lendings().pages(), 0u);
  CHECK_EQ(fixture.host().overlaid_bytes(), 0u);
  CHECK(fixture.lendings().invariants_hold()); }

TEST(guest_ram_lendings_join, anything_but_the_very_pages_from_another_plane_is_not_joined)
{ Fixture fixture;
  GuestRamLending *const lender = fixture.lend_two_pages();
  CHECK(fixture.lendings().join(ram + 3u * page, 2u * page, fixture.second(), 9u) == nullptr);
  CHECK(fixture.lendings().join(ram + 2u * page, 3u * page, fixture.second(), 9u) == nullptr);
  CHECK(fixture.lendings().join(ram + 2u * page, 2u * page, fixture.first(), 8u) == nullptr);
  CHECK(fixture.lendings().join(ram + 9u * page, 2u * page, fixture.second(), 9u) == nullptr);
  CHECK_EQ(lender->holders(), 1u);
  CHECK(!lender->held_beyond(fixture.first()));
  CHECK(lender->held_beyond(fixture.second()));
  fixture.lendings().leave(fixture.host(), *lender, fixture.first(), 7u);
  CHECK_EQ(fixture.lendings().live(), 0u); }

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
  CHECK_STATUS(lender->verify(same_request, header, 1u, next), Status::invalid_parameter);
  EntryList other(1u);
  other.add(ram + 2u * page, static_cast<std::uint32_t>(page));
  MockRequest    other_mock(other.bytes(), other.size());
  ControlRequest other_request(fixture.host(), &other_mock);
  next = 0u;
  CHECK_STATUS(lender->verify(other_request, header, 1u, next), Status::invalid_parameter);
  CHECK_EQ(next, 0u);
  fixture.lendings().leave(fixture.host(), *lender, fixture.first(), 7u); }

TEST(guest_ram_lendings_lend, a_pool_that_cannot_make_the_lending_closes_the_memory_it_was_given)
{ Fixture fixture;
  const int fd = make_memory(page);
  fixture.host().fail_allocation_at(1u);
  CHECK(fixture.lendings().lend(fixture.host(), page, fd, fixture.first(), 7u) == nullptr);
  CHECK_EQ(fcntl(fd, F_GETFD), -1);
  CHECK_EQ(fixture.lendings().live(), 0u);
  CHECK(fixture.lendings().invariants_hold()); }

TEST(guest_ram_lendings_join, a_holder_list_that_cannot_grow_leaves_the_lending_as_it_was)
{ Fixture fixture;
  GuestRamLending *const lender = fixture.lend_two_pages();
  for (std::uint64_t key = 10u; key < 17u; key++)
      CHECK(fixture.lendings().join(ram + 2u * page, 2u * page, fixture.second(), key) == lender);
  CHECK_EQ(lender->holders(), 8u);
  fixture.host().fail_allocation_at(1u);
  CHECK(fixture.lendings().join(ram + 2u * page, 2u * page, fixture.second(), 99u) == nullptr);
  CHECK_EQ(lender->holders(), 8u);
  CHECK(lender->invariants_hold());
  for (std::uint64_t key = 10u; key < 17u; key++) fixture.lendings().leave(fixture.host(), *lender, fixture.second(), key);
  fixture.lendings().leave(fixture.host(), *lender, fixture.first(), 7u);
  CHECK_EQ(fixture.lendings().live(), 0u); }

TEST(guest_ram_lendings_invariants, a_broken_page_set_pool_or_lending_is_detected_whichever_comes_first)
{ Fixture pages;
  GuestRamLending *const lender = pages.lend_two_pages();
  CHECK(pages.lendings().invariants_hold());
  GuestRamLendingsProbe::miscount_pages(pages.lendings());
  CHECK(!pages.lendings().invariants_hold());
  Fixture pool;
  GuestRamLending *const pooled = pool.lend_two_pages();
  CHECK(pool.lendings().invariants_hold());
  GuestRamLendingsProbe::miscount_lendings(pool.lendings());
  CHECK(!pool.lendings().invariants_hold());
  Fixture some;
  GuestRamLending *const first = some.lend_two_pages();
  GuestRamLending *const second = some.lendings().lend(some.host(), page, make_memory(page), some.second(), 5u);
  CHECK(second != nullptr);
  GuestRamLendingProbe::break_holders(*first);
  CHECK(!some.lendings().invariants_hold());
  GuestRamLendingProbe::mend_holders(*first);
  GuestRamLendingProbe::break_holders(*second);
  CHECK(!some.lendings().invariants_hold());
  GuestRamLendingProbe::mend_holders(*second);
  CHECK(some.lendings().invariants_hold());
  some.lendings().leave(some.host(), *second, some.second(), 5u);
  some.lendings().leave(some.host(), *first, some.first(), 7u);
  pool.lendings().leave(pool.host(), *pooled, pool.first(), 7u);
  pages.lendings().leave(pages.host(), *lender, pages.first(), 7u); }

TEST(guest_ram_lending_invariants, holders_pages_size_and_the_memory_must_agree)
{ Fixture fixture;
  GuestRamLending *const lending = fixture.lend_two_pages();
  CHECK(lending->invariants_hold());
  GuestRamLendingProbe::break_holders(*lending);
  CHECK(!lending->invariants_hold());
  GuestRamLendingProbe::mend_holders(*lending);
  GuestRamLendingProbe::miscount_overlay(*lending, 1u);
  CHECK(!lending->invariants_hold());
  GuestRamLendingProbe::miscount_overlay(*lending, ~std::uint64_t{0});
  GuestRamLendingProbe::set_size(*lending, page);
  CHECK(!lending->invariants_hold());
  GuestRamLendingProbe::set_size(*lending, 2u * page);
  const int fd = GuestRamLendingProbe::fd(*lending);
  GuestRamLendingProbe::set_fd(*lending, -1);
  CHECK(!lending->invariants_hold());
  GuestRamLendingProbe::set_fd(*lending, fd);
  CHECK(lending->invariants_hold());
  CHECK_EQ(lending->leave(fixture.first(), 7u), 0u);
  CHECK(lending->invariants_hold());
  GuestRamLendingProbe::set_fd(*lending, -1);
  CHECK(lending->invariants_hold());
  GuestRamLendingProbe::set_fd(*lending, fd);
  fixture.lendings().leave(fixture.host(), *lending, fixture.first(), 7u); }

TEST(guest_ram_lending, a_lending_with_nothing_laid_starts_nowhere_and_lets_go_of_no_holder_it_never_had)
{ Fixture fixture;
  GuestRamLending *const lending = fixture.lendings().lend(fixture.host(), page, make_memory(page), fixture.first(), 7u);
  CHECK_EQ(lending->start(), 0u);
  CHECK(!lending->complete());
  CHECK_EQ(lending->leave(fixture.first(), 8u), 1u);
  CHECK_EQ(lending->leave(fixture.second(), 7u), 1u);
  av::ContextPlane *holder = nullptr;
  std::uint64_t     key = 0u;
  CHECK(!lending->holder_at(1u, holder, key));
  CHECK(lending->holder_at(0u, holder, key));
  CHECK_EQ(key, 7u);
  fixture.lendings().leave(fixture.host(), *lending, fixture.first(), 7u); }

TEST(guest_ram_lending, memory_that_was_never_given_is_not_closed_when_the_lending_ends)
{ Fixture fixture;
  GuestRamLending *const lending = fixture.lendings().lend(fixture.host(), page, -1, fixture.first(), 7u);
  CHECK(lending != nullptr);
  fixture.lendings().leave(fixture.host(), *lending, fixture.first(), 7u);
  CHECK_EQ(fixture.lendings().live(), 0u); }

TEST(guest_ram_lending_verify, entries_that_are_none_unreadable_or_another_address_are_refused)
{ Fixture fixture;
  GuestRamLending *const lender = fixture.lend_two_pages();
  EntryList same(1u);
  same.add(ram + 2u * page, static_cast<std::uint32_t>(2u * page));
  MockRequest    mock(same.bytes(), same.size());
  ControlRequest request(fixture.host(), &mock);
  std::uint32_t  next = 0u;
  CHECK_STATUS(lender->verify(request, header, 0u, next), Status::invalid_parameter);
  CHECK_STATUS(lender->verify(request, header, 1u + 1u, next), Status::invalid_parameter);
  EntryList moved(1u);
  moved.add(ram + 3u * page, static_cast<std::uint32_t>(2u * page));
  MockRequest    moved_mock(moved.bytes(), moved.size());
  ControlRequest moved_request(fixture.host(), &moved_mock);
  CHECK_STATUS(lender->verify(moved_request, header, 1u, next), Status::invalid_parameter);
  CHECK_EQ(next, 0u);
  fixture.lendings().leave(fixture.host(), *lender, fixture.first(), 7u); }
