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
