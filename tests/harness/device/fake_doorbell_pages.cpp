#include "harness/device/fake_doorbell_pages.hpp"

namespace av {

FakeDoorbellPages::FakeDoorbellPages() : rings_{}, stray_(0u) {}

std::uint64_t FakeDoorbellPages::length() const { return pages * page_bytes; }

std::uint8_t  FakeDoorbellPages::read_u8(std::uint64_t) { return 0u; }
std::uint16_t FakeDoorbellPages::read_u16(std::uint64_t) { return 0u; }
std::uint32_t FakeDoorbellPages::read_u32(std::uint64_t) { return 0u; }

void FakeDoorbellPages::write_u8(std::uint64_t offset, std::uint8_t) { store(offset, 1u); }
void FakeDoorbellPages::write_u16(std::uint64_t offset, std::uint16_t) { store(offset, 2u); }
void FakeDoorbellPages::write_u32(std::uint64_t offset, std::uint32_t) { store(offset, 4u); }
void FakeDoorbellPages::write_u64(std::uint64_t offset, std::uint64_t) { store(offset, 8u); }

void FakeDoorbellPages::store(std::uint64_t offset, unsigned width)
{
    const bool ring = width == 4u && offset % page_bytes == 0u && offset / page_bytes < pages;
    if (ring) rings_[offset / page_bytes]++;
    else stray_++;
}

std::uint32_t FakeDoorbellPages::rings(std::uint32_t page) const { return page < pages ? rings_[page] : 0u; }
std::uint32_t FakeDoorbellPages::stray_stores() const { return stray_; }

}  // namespace av
