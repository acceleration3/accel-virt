#include "harness/device/fake_display_clock.hpp"

#include "harness/device/fake_virtqueue_device.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/display/clock_page_header.hpp"
#include "proto/display/vblank_record.hpp"

#include <cstring>

namespace av {

namespace {

constexpr std::uint32_t ok_nodata = 0x1100u;
constexpr std::uint32_t err_invalid_parameter = 0x1205u;
constexpr std::uint32_t head_record = 32u;   /* head 0, after the header */

}  // namespace

FakeDisplayClock::FakeDisplayClock(MockPlatform &platform)
    : platform_(platform), page_(0u), sequence_(0u), refusing_(false) {}

void FakeDisplayClock::answer(FakeVirtqueueDevice &control)
{
    const std::uint32_t before = control.requests();
    static_cast<void>(control.respond_all(refusing_ ? err_invalid_parameter : ok_nodata));
    if (refusing_ || control.requests() == before) return;
    std::memcpy(&page_, control.request(before) + 24u, sizeof page_);
    std::memset(bytes(), 0, ClockPageHeader::page_size);
    static_cast<void>(ClockPageHeader(1u).encode(bytes(), ClockPageHeader::encoded_size));
}

void FakeDisplayClock::refuse() { refusing_ = true; }

std::uint64_t FakeDisplayClock::page() const { return page_; }

std::uint8_t *FakeDisplayClock::bytes() const
{
    return static_cast<std::uint8_t *>(platform_.translate_physical(page_, ClockPageHeader::page_size));
}

void FakeDisplayClock::blank(std::uint32_t latched) { blank_at(sequence_ + 1u, latched); }

/* Odd while written and even once whole, as the device's clock writes it. */
void FakeDisplayClock::blank_at(std::uint64_t sequence, std::uint32_t latched)
{
    if (page_ == 0u) return;
    sequence_ = sequence;
    std::uint8_t *const record = bytes() + head_record;
    std::uint32_t       generation = 0u;
    std::memcpy(&generation, record, sizeof generation);
    generation++;
    std::memcpy(record, &generation, sizeof generation);
    static_cast<void>(VblankRecord(latched, sequence, sequence * 16666666u, 60000u)
                          .encode(record + VblankRecord::fields_at, VblankRecord::encoded_size));
    generation++;
    std::memcpy(record, &generation, sizeof generation);
}

std::uint32_t FakeDisplayClock::interrupt_every() const
{
    if (page_ == 0u) return 0u;
    std::uint32_t every = 0u;
    std::memcpy(&every, bytes() + head_record + VblankRecord::interrupt_every_at, sizeof every);
    return every;
}

}  // namespace av
