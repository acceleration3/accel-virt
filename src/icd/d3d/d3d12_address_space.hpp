/* The GPU virtual addresses a Direct3D 12 device hands out.
 *
 * The addresses are the guest's own: the runtime asks the driver where a
 * buffer lives (pfnCheckResourceVirtualAddress) and applications do arithmetic
 * on the answer, but nothing in the guest dereferences one - the host finds
 * the buffer an address falls in and adds the offset to the host buffer's own
 * (d3d12.toml). So an address only has to be unique and aligned: each heap and
 * committed resource takes the next range, from any thread, and a range is
 * never handed out twice. The space is what GPUVA_CAPS reports, 40 bits, which
 * a process would take years of allocation to exhaust. */
#ifndef AV_D3D_D3D12_ADDRESS_SPACE_HPP
#define AV_D3D_D3D12_ADDRESS_SPACE_HPP

#include <stdint.h>

namespace av::d3d {

class D3d12AddressSpace {
public:
    static constexpr uint64_t alignment = uint64_t{64} * 1024u;
    static constexpr uint32_t address_bits = 40u;
    /* Low enough to keep the whole space inside address_bits, high enough that
     * an address is never mistaken for a small integer. */
    static constexpr uint64_t first = uint64_t{1} << 32u;

    D3d12AddressSpace();
    D3d12AddressSpace(const D3d12AddressSpace &) = delete;
    D3d12AddressSpace &operator=(const D3d12AddressSpace &) = delete;

    /* The base of `bytes` of addresses no other reservation has, aligned; 0
     * for no bytes or when the space is used up. */
    uint64_t reserve(uint64_t bytes);
    uint64_t reserved() const;

    bool invariants_hold() const;

private:
    uint64_t next_;
};

}  // namespace av::d3d

#endif  // AV_D3D_D3D12_ADDRESS_SPACE_HPP
