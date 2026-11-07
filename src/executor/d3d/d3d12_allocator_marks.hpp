#ifndef AV_D3D12_ALLOCATOR_MARKS_HPP
#define AV_D3D12_ALLOCATOR_MARKS_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class BlockSource;

class D3d12AllocatorMarks {
public:
    explicit D3d12AllocatorMarks(BlockSource &blocks);
    D3d12AllocatorMarks(const D3d12AllocatorMarks &) = delete;
    D3d12AllocatorMarks &operator=(const D3d12AllocatorMarks &) = delete;

    [[nodiscard]] Status bind(uint32_t list, uint32_t allocator);
    [[nodiscard]] Status submitted(uint32_t list, uint64_t mark);
    void                 forget_list(uint32_t list);
    void                 forget_allocator(uint32_t allocator);
    void                 clear();

    uint64_t mark_of(uint32_t allocator) const;
    uint32_t lists() const;

    bool invariants_hold() const;

private:
    KeyedSlots<uint32_t, uint32_t> allocators_;
    KeyedSlots<uint32_t, uint64_t> marks_;
};

}

#endif
