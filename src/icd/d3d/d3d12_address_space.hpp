#ifndef AV_D3D_D3D12_ADDRESS_SPACE_HPP
#define AV_D3D_D3D12_ADDRESS_SPACE_HPP

#include <stdint.h>

namespace av::d3d {

class D3d12AddressSpace {
public:
    static constexpr uint64_t alignment = uint64_t{64} * 1024u;
    static constexpr uint32_t address_bits = 40u;
    static constexpr uint64_t first = uint64_t{1} << 32u;

    D3d12AddressSpace();
    D3d12AddressSpace(const D3d12AddressSpace &) = delete;
    D3d12AddressSpace &operator=(const D3d12AddressSpace &) = delete;

    uint64_t reserve(uint64_t bytes);
    uint64_t reserved() const;

    bool invariants_hold() const;

private:
    uint64_t next_;
};

}

#endif
