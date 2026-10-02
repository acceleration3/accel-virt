/* Which host buffer a guest GPU virtual address falls in.
 *
 * The guest's driver hands the Direct3D 12 runtime GPU addresses of its own
 * choosing - it has to answer GetGPUVirtualAddress before the host could say
 * anything - and applications do arithmetic on them: a constant buffer is a
 * buffer's address plus an offset, a vertex buffer view another. So every
 * buffer is registered with the guest range it was given and the host
 * address vkd3d-proton gave it, and a guest address translates to the host
 * buffer's address plus the same offset. An address in no registered range,
 * or a range running past its buffer's end, translates to nothing: a command
 * naming one is refused rather than handed to the GPU.
 *
 * Sorted by guest address in a fixed table, so a translation is a binary
 * search and nothing is allocated. */
#ifndef AV_D3D12_ADDRESS_MAP_HPP
#define AV_D3D12_ADDRESS_MAP_HPP

#include <stdint.h>

namespace av {

class D3d12AddressMap {
public:
    static constexpr uint32_t capacity = 16384u;

    D3d12AddressMap();

    /* `size` bytes from guest address `guest` are host buffer `id`'s, from
     * host address `host`. False for an empty range, one overlapping another,
     * one that wraps, or a full table. */
    [[nodiscard]] bool insert(uint64_t guest, uint64_t size, uint64_t host, uint32_t id);
    /* The range buffer `id` was registered with, gone; nothing for an id
     * nothing was registered under. */
    void               remove(uint32_t id);
    /* The host address `bytes` bytes from `guest` are at; 0 when they are not
     * all inside one registered range. `bytes` 0 asks about the one byte. */
    uint64_t           translate(uint64_t guest, uint64_t bytes) const;
    /* The buffer `guest` falls in, and the offset into it; 0 for none. */
    uint32_t           owner(uint64_t guest, uint64_t &offset) const;
    void               clear();
    uint32_t           count() const;

    bool invariants_hold() const;

private:
    /* The index of the last range starting at or below `guest`; count_ for none. */
    uint32_t below(uint64_t guest) const;

    uint64_t guest_[capacity];
    uint64_t size_[capacity];
    uint64_t host_[capacity];
    uint32_t id_[capacity];
    uint32_t count_;
};

}  // namespace av

#endif  // AV_D3D12_ADDRESS_MAP_HPP
