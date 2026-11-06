#include "harness/vidmm/address_space_fixture.hpp"

#include "harness/framework/test_registry.hpp"

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
{ return space.ranges_.at(index); }

GrowableArray<VirtualRange> &GpuVirtualAddressSpaceProbe::ranges()
{ return space.ranges_; }

std::uint32_t GpuVirtualAddressSpaceProbe::capacity() const
{ return space.ranges_.capacity(); }

std::uint32_t GpuVirtualAddressSpaceProbe::packed() const
{ return space.packed_; }

void GpuVirtualAddressSpaceProbe::set_packed(std::uint32_t packed)
{ space.packed_ = packed; }

void GpuVirtualAddressSpaceProbe::set_bounds(const ByteRange &bounds)
{ space.bounds_ = bounds; }

void GpuVirtualAddressSpaceProbe::set_span(std::uint32_t index,
                                           const ByteRange &span)
{ space.ranges_.at(index).span_ = span; }

void GpuVirtualAddressSpaceProbe::set_object(std::uint32_t index,
                                             std::uint32_t object)
{ space.ranges_.at(index).object_ = object; }

void GpuVirtualAddressSpaceProbe::set_object_offset(std::uint32_t index,
                                                    std::uint64_t offset)
{ space.ranges_.at(index).object_offset_ = offset; }

void GpuVirtualAddressSpaceProbe::set_mapped(std::uint32_t index, bool mapped)
{ space.ranges_.at(index).mapped_ = mapped; }

}
