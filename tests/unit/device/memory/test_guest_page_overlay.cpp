#include "core/support/status.hpp"
#include "device/control/control_request.hpp"
#include "device/memory/guest_page_overlay.hpp"
#include "device/memory/lent_pages.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

using av::ControlRequest;
using av::GuestPageOverlay;
using av::LentPages;
using av::MockHost;
using av::MockRequest;
using av::Status;

namespace av {

struct GuestPageOverlayProbe {
    static void miscount(GuestPageOverlay &overlay) { overlay.record_->covered_ += 1u; }
    static void overcount(GuestPageOverlay &overlay) { overlay.record_->count_ = overlay.record_->capacity_ + 1u; }
    static void repair(GuestPageOverlay &overlay, std::uint32_t count, std::uint64_t covered)
    {
        overlay.record_->count_ = count;
        overlay.record_->covered_ = covered;
    }
};

}

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

class BlobMemory {
public:
    explicit BlobMemory(std::uint64_t size) : fd_(memfd_create("blob", MFD_CLOEXEC)), size_(size), bytes_(nullptr)
    {
        if (fd_ < 0 || ftruncate(fd_, static_cast<off_t>(size)) != 0) std::abort();
        bytes_ = static_cast<std::uint8_t *>(mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0));
    }
    ~BlobMemory()
    {
        munmap(bytes_, size_);
        close(fd_);
    }
    BlobMemory(const BlobMemory &) = delete;
    BlobMemory &operator=(const BlobMemory &) = delete;
    int            fd() const { return fd_; }
    std::uint64_t  size() const { return size_; }
    std::uint8_t  *bytes() const { return bytes_; }

private:
    int           fd_;
    std::uint64_t size_;
    std::uint8_t *bytes_;
};

class Fixture {
public:
    explicit Fixture(std::uint64_t blob_bytes = 4u * page)
        : lent_(host_), memory_(blob_bytes), overlay_()
    { host_.add_guest_ram(ram, 0x10000u); }
    MockHost         &host() { return host_; }
    LentPages        &lent() { return lent_; }
    BlobMemory       &memory() { return memory_; }
    GuestPageOverlay &overlay() { return overlay_; }
    Status add(const EntryList &list, std::uint32_t entries)
    {
        MockRequest    mock(list.bytes(), list.size());
        ControlRequest request(host_, &mock);
        return overlay_.add(host_, lent_, request, header, entries, memory_.fd(), memory_.size());
    }

private:
    MockHost         host_;
    LentPages        lent_;
    BlobMemory       memory_;
    GuestPageOverlay overlay_;
};

}

TEST(guest_page_overlay_add, the_guests_pages_and_the_blobs_memory_are_one_until_restored)
{ Fixture fixture;
  std::uint8_t *const second = fixture.host().guest_ram(ram + 5u * page);
  std::uint8_t *const first = fixture.host().guest_ram(ram + 2u * page);
  first[10] = 0x11u;
  second[page + 20u] = 0x22u;
  EntryList one(1u);
  one.add(ram + 2u * page, static_cast<std::uint32_t>(page));
  CHECK_STATUS(fixture.add(one, 1u), Status::ok);
  CHECK_EQ(fixture.overlay().covered(), page);
  EntryList two(1u);
  two.add(ram + 5u * page, static_cast<std::uint32_t>(2u * page));
  CHECK_STATUS(fixture.add(two, 1u), Status::ok);
  CHECK(fixture.overlay().invariants_hold());
  CHECK_EQ(fixture.overlay().covered(), 3u * page);
  CHECK_EQ(fixture.overlay().runs(), 2u);
  CHECK_EQ(fixture.host().overlaid_bytes(), 3u * page);
  CHECK_EQ(fixture.lent().count(), 3u);
  CHECK_EQ(fixture.memory().bytes()[10], 0x11u);
  CHECK_EQ(fixture.memory().bytes()[2u * page + 20u], 0x22u);
  fixture.memory().bytes()[page + 7u] = 0x33u;
  CHECK_EQ(second[7], 0x33u);
  first[40] = 0x44u;
  CHECK_EQ(fixture.memory().bytes()[40], 0x44u);
  fixture.overlay().restore();
  CHECK_EQ(fixture.host().overlaid_bytes(), 0u);
  CHECK_EQ(fixture.lent().count(), 0u);
  CHECK_EQ(fixture.overlay().covered(), 0u);
  CHECK_EQ(fixture.host().live_guest_mappings(), 0u);
  CHECK_EQ(second[7], 0x33u);
  CHECK_EQ(first[40], 0x44u);
  fixture.memory().bytes()[page + 8u] = 0x55u;
  CHECK(second[8] != 0x55u);
  CHECK(fixture.overlay().invariants_hold()); }

TEST(guest_page_overlay_add, entries_across_boundaries_and_past_the_first_record_are_all_laid)
{ Fixture fixture(80u * page);
  fixture.host().split_guest_memory_at(ram + page);
  EntryList across(1u);
  across.add(ram, static_cast<std::uint32_t>(2u * page));
  CHECK_STATUS(fixture.add(across, 1u), Status::ok);
  CHECK_EQ(fixture.overlay().runs(), 2u);
  EntryList many(14u);
  for (std::uint32_t entry = 0u; entry < 14u; entry++) many.add(ram + (2u + entry) * page, static_cast<std::uint32_t>(page));
  CHECK_STATUS(fixture.add(many, 14u), Status::ok);
  CHECK_EQ(fixture.overlay().covered(), 16u * page);
  CHECK(fixture.overlay().invariants_hold()); }

TEST(guest_page_overlay_add, a_bad_piece_lays_nothing_of_itself)
{ struct Case {
      const char   *what;
      std::uint64_t address;
      std::uint32_t length;
      std::uint32_t entries;
      Status        status;
  };
  const Case cases[] = {
      {"no entries", ram, 0x1000u, 0u, Status::invalid_parameter},
      {"a short list", ram, 0x1000u, 2u, Status::invalid_parameter},
      {"an empty entry", ram, 0u, 1u, Status::invalid_parameter},
      {"an entry off a page", ram + 8u, 0x1000u, 1u, Status::invalid_parameter},
      {"a length not whole pages", ram, 0x1800u, 1u, Status::invalid_parameter},
      {"a wrapping entry", 0xFFFFFFFFFFFFF000ull, 0x2000u, 1u, Status::invalid_parameter},
      {"past the blob", ram, 0x5000u, 1u, Status::invalid_parameter},
      {"nothing there", 0x900000u, 0x1000u, 1u, Status::not_found},
      {"not guest RAM", 0x800000u, 0x1000u, 1u, Status::not_supported},
  };
  for (const Case &c : cases) {
      Fixture fixture;
      fixture.host().add_bounce_region(0x800000u, 0x10000u);
      EntryList single(1u);
      single.add(c.address, c.length);
      CHECK_STATUS(fixture.add(single, c.entries), c.status);
      CHECK_EQ(fixture.overlay().covered(), 0u);
      CHECK_EQ(fixture.lent().count(), 0u);
      CHECK_EQ(fixture.host().overlaid_bytes(), 0u);
      CHECK_EQ(fixture.host().live_guest_mappings(), 0u);
  } }

TEST(guest_page_overlay_add, a_page_already_lent_is_refused_and_earlier_runs_stay_laid)
{ Fixture fixture;
  EntryList first(1u);
  first.add(ram, static_cast<std::uint32_t>(page));
  CHECK_STATUS(fixture.add(first, 1u), Status::ok);
  EntryList again(2u);
  again.add(ram + page, static_cast<std::uint32_t>(page)).add(ram, static_cast<std::uint32_t>(page));
  CHECK_STATUS(fixture.add(again, 2u), Status::conflict);
  CHECK_EQ(fixture.overlay().covered(), 2u * page);
  CHECK_EQ(fixture.lent().count(), 2u);
  GuestPageOverlay other;
  MockRequest      mock(first.bytes(), first.size());
  ControlRequest   request(fixture.host(), &mock);
  CHECK_STATUS(other.add(fixture.host(), fixture.lent(), request, header, 1u, fixture.memory().fd(), fixture.memory().size()), Status::conflict);
  CHECK_EQ(other.covered(), 0u);
  fixture.overlay().restore();
  CHECK_EQ(fixture.host().overlaid_bytes(), 0u);
  CHECK_STATUS(other.add(fixture.host(), fixture.lent(), request, header, 1u, fixture.memory().fd(), fixture.memory().size()), Status::ok); }

TEST(guest_page_overlay_add, a_run_the_host_refuses_or_cannot_record_is_left_as_it_was)
{ { Fixture fixture;
    fixture.host().fail_overlay_at(1u);
    EntryList single(1u);
    single.add(ram, static_cast<std::uint32_t>(page));
    CHECK_STATUS(fixture.add(single, 1u), Status::not_supported);
    CHECK_EQ(fixture.lent().count(), 0u);
    CHECK_EQ(fixture.host().live_guest_mappings(), 0u); }
  { Fixture fixture;
    fixture.host().fail_allocation_at(2u);
    EntryList single(1u);
    single.add(ram, static_cast<std::uint32_t>(page));
    CHECK_STATUS(fixture.add(single, 1u), Status::no_memory);
    CHECK_EQ(fixture.host().overlaid_bytes(), 0u);
    CHECK_EQ(fixture.lent().count(), 0u); }
  { Fixture fixture;
    fixture.host().fail_allocation_at(2u);
    fixture.host().fail_restore_at(1u);
    EntryList single(1u);
    single.add(ram, static_cast<std::uint32_t>(page));
    const std::uint32_t logged = fixture.host().logged();
    CHECK_STATUS(fixture.add(single, 1u), Status::no_memory);
    CHECK_EQ(fixture.host().logged(), logged + 1u);
    CHECK(std::strstr(fixture.host().last_log(), "could not be given its RAM back") != nullptr);
    CHECK_EQ(fixture.host().overlaid_bytes(), page);
    CHECK_STATUS(fixture.host().restore_guest_run(fixture.host().guest_ram(ram), page), Status::ok); }
  Fixture fixture;
  EntryList single(1u);
  single.add(ram, static_cast<std::uint32_t>(page));
  CHECK_STATUS(fixture.add(single, 1u), Status::ok);
  fixture.host().fail_restore_at(1u);
  const std::uint32_t logged = fixture.host().logged();
  fixture.overlay().restore();
  CHECK_EQ(fixture.host().logged(), logged + 1u);
  CHECK_EQ(fixture.lent().count(), 0u);
  CHECK_STATUS(fixture.host().restore_guest_run(fixture.host().guest_ram(ram), page), Status::ok); }

TEST(guest_page_overlay_invariants, a_record_that_is_not_its_runs_breaks_them)
{ Fixture fixture;
  EntryList single(1u);
  single.add(ram, static_cast<std::uint32_t>(page));
  CHECK_STATUS(fixture.add(single, 1u), Status::ok);
  CHECK(fixture.overlay().invariants_hold());
  av::GuestPageOverlayProbe::miscount(fixture.overlay());
  CHECK(!fixture.overlay().invariants_hold());
  av::GuestPageOverlayProbe::repair(fixture.overlay(), 1u, page);
  av::GuestPageOverlayProbe::overcount(fixture.overlay());
  CHECK(!fixture.overlay().invariants_hold());
  av::GuestPageOverlayProbe::repair(fixture.overlay(), 1u, page);
  fixture.lent().release(ram, page);
  CHECK(!fixture.overlay().invariants_hold());
  CHECK_STATUS(fixture.lent().claim(ram, page), Status::ok);
  CHECK(fixture.overlay().invariants_hold()); }
