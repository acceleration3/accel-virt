#ifndef AV_GL_MEMORY_OBJECTS_HPP
#define AV_GL_MEMORY_OBJECTS_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class BlockSource;
class GlHostImport;
class SliceProgress;

class GlMemoryObjects {
public:
    static constexpr uint32_t most_placements = uint32_t{1} << 16u;

    GlMemoryObjects(BlockSource &blocks, const SliceProgress &progress);
    GlMemoryObjects(const GlMemoryObjects &) = delete;
    GlMemoryObjects &operator=(const GlMemoryObjects &) = delete;

    [[nodiscard]] Status add(uint32_t name, uint64_t size);
    [[nodiscard]] Status admit(uint32_t buffer, uint32_t name, uint64_t offset, uint64_t bytes);
    [[nodiscard]] Status place(uint32_t buffer, uint32_t name, uint64_t offset, uint64_t bytes);
    void                 leave(uint32_t buffer);
    void                 retire(uint32_t name);
    void                 reclaim(GlHostImport &import);
    void                 destroy_all(GlHostImport &import);

    bool     holds(uint32_t name) const;
    uint32_t count() const;
    uint32_t placed() const;

    bool invariants_hold() const;

private:
    struct Memory {
        uint64_t size_;
        uint64_t mark_;
        uint32_t placed_;
        bool     retired_;
    };

    const SliceProgress           &progress_;
    KeyedSlots<uint32_t, Memory>   memories_;
    KeyedSlots<uint32_t, uint32_t> placements_;
};

}

#endif
