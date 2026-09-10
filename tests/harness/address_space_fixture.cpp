#include "address_space_fixture.hpp"

#include "test_registry.hpp"

#include <cstdint>

namespace av {

AddressSpaceFixture::AddressSpaceFixture() : platform_(), space_(platform_)
{
    if (space_.initialize(ByteRange(base, size)) != Status::ok)
        TestRegistry::instance().record_failure(__FILE__, __LINE__,
                                                "fixture failed to initialize");
    if (!space_.invariants_hold())
        TestRegistry::instance().record_failure(__FILE__, __LINE__,
                                                "fixture is born inconsistent");
}

/* Leaks are reported by ~MockPlatform, which runs after the space has released
 * its table. */
AddressSpaceFixture::~AddressSpaceFixture() = default;

MockPlatform           &AddressSpaceFixture::platform() { return platform_; }
GpuVirtualAddressSpace &AddressSpaceFixture::space()    { return space_; }

std::uint64_t AddressSpaceFixture::reserve_page()
{
    std::uint64_t reserved = 0u;
    if (space_.reserve(page, Alignment(page), reserved) != Status::ok)
        TestRegistry::instance().record_failure(__FILE__, __LINE__,
                                                "reserve_page failed");
    return reserved;
}

GpuVirtualAddressSpaceProbe::GpuVirtualAddressSpaceProbe(
    GpuVirtualAddressSpace &target) : space(target) {}

VirtualRange &GpuVirtualAddressSpaceProbe::range_at(std::uint32_t index)
{ return space.ranges_[index]; }

std::uint32_t GpuVirtualAddressSpaceProbe::capacity() const
{ return space.capacity_; }

void GpuVirtualAddressSpaceProbe::set_count(std::uint32_t count)
{ space.count_ = count; }

void GpuVirtualAddressSpaceProbe::set_capacity(std::uint32_t capacity)
{ space.capacity_ = capacity; }

void GpuVirtualAddressSpaceProbe::drop_table() { space.ranges_ = nullptr; }

void GpuVirtualAddressSpaceProbe::set_ceiling(std::uint32_t ceiling)
{ space.ceiling_ = ceiling; }

void GpuVirtualAddressSpaceProbe::set_bounds(const ByteRange &bounds)
{ space.bounds_ = bounds; }

void GpuVirtualAddressSpaceProbe::set_span(std::uint32_t index,
                                           const ByteRange &span)
{ space.ranges_[index].span_ = span; }

void GpuVirtualAddressSpaceProbe::set_object(std::uint32_t index,
                                             std::uint32_t object)
{ space.ranges_[index].object_ = object; }

void GpuVirtualAddressSpaceProbe::set_object_offset(std::uint32_t index,
                                                    std::uint64_t offset)
{ space.ranges_[index].object_offset_ = offset; }

void GpuVirtualAddressSpaceProbe::set_mapped(std::uint32_t index, bool mapped)
{ space.ranges_[index].mapped_ = mapped; }

}  // namespace av
