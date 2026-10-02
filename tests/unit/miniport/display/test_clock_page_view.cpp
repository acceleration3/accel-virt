/* The display clock's page as the guest reads it: a head's blank only when
 * it read the record whole - the same even generation before and after - and
 * only from a page the device headed; everything else on the page is the
 * device's word and refused when it is wrong. The one field the driver
 * writes, interrupt_every, lands in the head's record and nowhere else. */
#include "miniport/display/clock_page_view.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/display/clock_page_header.hpp"
#include "proto/display/vblank_record.hpp"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <thread>

using av::ClockPageHeader;
using av::ClockPageView;
using av::Irql;
using av::MockPlatform;
using av::Status;
using av::VblankRecord;

namespace {

std::uint8_t *page_of(MockPlatform &platform, const ClockPageView &view)
{
    return static_cast<std::uint8_t *>(platform.translate_physical(view.address(), ClockPageHeader::page_size));
}

void head(std::uint8_t *page, std::uint32_t heads)
{
    static_cast<void>(ClockPageHeader(heads).encode(page, ClockPageHeader::encoded_size));
}

void write_blank(std::uint8_t *page, std::uint32_t at_head, const VblankRecord &blank, std::uint32_t generation)
{
    std::uint8_t *const record = page + 32u + 64u * at_head;
    static_cast<void>(blank.encode(record + VblankRecord::fields_at, VblankRecord::encoded_size));
    std::memcpy(record, &generation, sizeof generation);
}

}  // namespace

TEST(clock_page_view, a_page_is_a_zeroed_block_the_device_can_address_and_is_freed)
{ MockPlatform platform;
  {
      ClockPageView view(platform);
      CHECK_EQ(view.address(), 0u);
      CHECK_STATUS(view.initialize(), Status::ok);
      CHECK(view.address() != 0u);
      CHECK_EQ(page_of(platform, view)[ClockPageHeader::page_size - 1u], 0u);
      CHECK_STATUS(view.initialize(), Status::invalid_state);
      CHECK_EQ(platform.live_allocations(), 1u);
      view.release();
      CHECK_EQ(view.address(), 0u);
      view.release();
      CHECK_STATUS(view.initialize(), Status::ok);
  }
  CHECK_EQ(platform.live_allocations(), 0u);   /* freed with the view */
  ClockPageView refused(platform);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(refused.initialize(), Status::no_memory);
  CHECK_EQ(refused.address(), 0u); }

TEST(clock_page_view, a_head_s_blank_is_read_from_a_page_the_device_headed)
{ MockPlatform platform;
  ClockPageView view(platform);
  VblankRecord  blank(7u, 8u, 9u, 60000u);
  CHECK_STATUS(view.read(0u, blank), Status::invalid_state);   /* no page */
  CHECK_STATUS(view.initialize(), Status::ok);
  CHECK_STATUS(view.read(0u, blank), Status::malformed);       /* nothing headed it */
  std::uint8_t *const page = page_of(platform, view);
  head(page, 2u);
  write_blank(page, 1u, VblankRecord(3u, 400u, 5000u, 143998u), 6u);
  CHECK_STATUS(view.read(2u, blank), Status::not_found);
  platform.set_irql(Irql::dispatch);
  CHECK_STATUS(view.read(1u, blank), Status::ok);
  platform.set_irql(Irql::passive);
  CHECK_EQ(blank.latched(), 3u);
  CHECK_EQ(blank.sequence(), 400u);
  CHECK_EQ(blank.vblank_ns(), 5000u);
  CHECK_EQ(blank.refresh_millihz(), 143998u);
  write_blank(page, 1u, VblankRecord(3u, 401u, 5000u, 999u), 8u);   /* a rate no device runs at */
  CHECK_STATUS(view.read(1u, blank), Status::malformed);
  CHECK_EQ(blank.sequence(), 400u);
  CHECK(view.invariants_hold()); }

TEST(clock_page_view, a_record_never_whole_is_a_conflict)
{ MockPlatform platform;
  ClockPageView view(platform);
  CHECK_STATUS(view.initialize(), Status::ok);
  std::uint8_t *const page = page_of(platform, view);
  head(page, 1u);
  write_blank(page, 0u, VblankRecord(1u, 1u, 1u, 60000u), 5u);   /* odd: mid-write, and it stays so */
  VblankRecord blank;
  CHECK_STATUS(view.read(0u, blank), Status::conflict);
  CHECK_EQ(blank.sequence(), 0u); }

/* A device writing the record while the guest reads it: every read either
 * finds a blank the device wrote whole or says it could not. */
TEST(clock_page_view, a_record_written_during_the_read_is_never_taken_torn)
{ MockPlatform platform;
  ClockPageView view(platform);
  CHECK_STATUS(view.initialize(), Status::ok);
  std::uint8_t *const page = page_of(platform, view);
  head(page, 1u);
  write_blank(page, 0u, VblankRecord(1u, 1u, 1u, 60000u), 2u);
  std::atomic<bool> stop{false};
  std::thread writer([&] {
      auto *const generation = reinterpret_cast<std::uint32_t *>(page + 32u);
      std::uint8_t *const fields = page + 32u + VblankRecord::fields_at;
      for (std::uint64_t sequence = 2u; !stop.load(); sequence++) {
          __atomic_fetch_add(generation, 1u, __ATOMIC_RELEASE);
          std::uint8_t bytes[VblankRecord::encoded_size] = {};
          static_cast<void>(VblankRecord(static_cast<std::uint32_t>(sequence), sequence, sequence, 60000u)
                                .encode(bytes, sizeof bytes));
          for (std::size_t at = 0u; at < sizeof bytes; at += 4u) {
              std::uint32_t word = 0u;
              std::memcpy(&word, bytes + at, sizeof word);
              __atomic_store_n(reinterpret_cast<std::uint32_t *>(fields + at), word, __ATOMIC_RELAXED);
          }
          __atomic_fetch_add(generation, 1u, __ATOMIC_RELEASE);
      }
  });
  std::uint32_t whole = 0u, refused = 0u;
  for (std::uint32_t read = 0u; read < 200000u; read++) {
      VblankRecord blank;
      const Status status = view.read(0u, blank);
      if (status == Status::ok) {
          whole++;
          CHECK_EQ(blank.latched(), static_cast<std::uint32_t>(blank.sequence()));
          CHECK_EQ(blank.vblank_ns(), blank.sequence());
      } else {
          CHECK_STATUS(status, Status::conflict);
          refused++;
      }
  }
  stop.store(true);
  writer.join();
  CHECK(whole > 0u);
  CHECK_EQ(whole + refused, 200000u); }

TEST(clock_page_view, the_driver_asks_in_its_head_s_record_and_nowhere_else)
{ MockPlatform platform;
  ClockPageView view(platform);
  view.ask(0u, 1u);   /* no page: nothing */
  CHECK_STATUS(view.initialize(), Status::ok);
  std::uint8_t *const page = page_of(platform, view);
  view.ask(0u, 1u);   /* a page no device headed: nowhere to ask */
  std::uint32_t every = 0u;
  std::memcpy(&every, page + 32u + VblankRecord::interrupt_every_at, sizeof every);
  CHECK_EQ(every, 0u);
  head(page, 2u);
  view.ask(1u, 24u);
  view.ask(2u, 7u);   /* past the page's heads */
  std::memcpy(&every, page + 96u + VblankRecord::interrupt_every_at, sizeof every);
  CHECK_EQ(every, 24u);
  std::memcpy(&every, page + 32u + VblankRecord::interrupt_every_at, sizeof every);
  CHECK_EQ(every, 0u);
  std::memcpy(&every, page + 160u + VblankRecord::interrupt_every_at, sizeof every);
  CHECK_EQ(every, 0u); }
