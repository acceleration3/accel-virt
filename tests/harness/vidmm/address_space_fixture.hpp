#ifndef AV_ADDRESS_SPACE_FIXTURE_HPP
#define AV_ADDRESS_SPACE_FIXTURE_HPP

#include "core/support/growable_array.hpp"
#include "miniport/memory/gpu_virtual_address_space.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

namespace av {

class AddressSpaceFixture {
public:
    static constexpr std::uint64_t base = 0x1000000000ull;
    static constexpr std::uint64_t size = 0x100000ull;
    static constexpr std::uint64_t page = GpuVirtualAddressSpace::page_size;

    AddressSpaceFixture();
    ~AddressSpaceFixture();
    AddressSpaceFixture(const AddressSpaceFixture &) = delete;
    AddressSpaceFixture &operator=(const AddressSpaceFixture &) = delete;

    MockPlatform           &platform();
    GpuVirtualAddressSpace &space();

    std::uint64_t reserve_page();

private:
    MockPlatform           platform_;
    GpuVirtualAddressSpace space_;
};

struct GpuVirtualAddressSpaceProbe {
    explicit GpuVirtualAddressSpaceProbe(GpuVirtualAddressSpace &space);

    VirtualRange &range_at(std::uint32_t index);
    GrowableArray<VirtualRange> &ranges();
    std::uint32_t capacity() const;
    std::uint32_t packed() const;
    void          set_packed(std::uint32_t packed);
    void          set_bounds(const ByteRange &bounds);
    void          set_span(std::uint32_t index, const ByteRange &span);
    void          set_object(std::uint32_t index, std::uint32_t object);
    void          set_object_offset(std::uint32_t index, std::uint64_t offset);
    void          set_mapped(std::uint32_t index, bool mapped);

    GpuVirtualAddressSpace &space;
};

}

#endif
