#ifndef AV_GL_MEMORY_OBJECTS_HPP
#define AV_GL_MEMORY_OBJECTS_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class BlockSource;
class GlHostImport;
class GlMemoryGroup;
class SliceProgress;

class GlMemoryObjects {
public:
    static constexpr uint32_t most_placements = uint32_t{1} << 16u;

    GlMemoryObjects(BlockSource &blocks, const SliceProgress &progress);
    ~GlMemoryObjects();
    GlMemoryObjects(const GlMemoryObjects &) = delete;
    GlMemoryObjects &operator=(const GlMemoryObjects &) = delete;

    [[nodiscard]] bool   join(GlMemoryGroup &group, GlHostImport *own = nullptr);
    void                 part();
    [[nodiscard]] Status add(uint32_t name, uint64_t size);
    [[nodiscard]] Status admit(uint32_t buffer, uint32_t name, uint64_t offset, uint64_t bytes);
    [[nodiscard]] Status place(uint32_t buffer, uint32_t name, uint64_t offset, uint64_t bytes);
    void                 leave(uint32_t buffer);
    [[nodiscard]] Status admit_texture(uint32_t texture, uint32_t name, uint64_t offset, uint64_t bytes);
    [[nodiscard]] Status place_texture(uint32_t texture, uint32_t name, uint64_t offset, uint64_t bytes);
    [[nodiscard]] Status inherit_texture(uint32_t view, uint32_t original);
    void                 leave_texture(uint32_t texture);
    void                 retire(uint32_t name);
    void                 reclaim(GlHostImport &import);
    void                 destroy_all(GlHostImport &import);
    void                 forget_all();

    bool     holds(uint32_t name);
    uint32_t count() const;
    uint32_t placed() const;
    uint32_t textured() const;

    bool invariants_hold() const;

private:
    struct Memory {
        uint64_t size_;
        uint64_t mark_;
        uint32_t placed_;
        uint32_t textured_;
        bool     retired_;
    };

    const SliceProgress           &progress_;
    KeyedSlots<uint32_t, Memory>   memories_;
    KeyedSlots<uint32_t, uint32_t> placements_;
    KeyedSlots<uint32_t, uint32_t> textures_;

    class Hold {
    public:
        explicit Hold(GlMemoryGroup *group);
        ~Hold();
        Hold(const Hold &) = delete;
        Hold &operator=(const Hold &) = delete;

    private:
        GlMemoryGroup *group_;
    };

    GlMemoryObjects *holder_of(uint32_t name);
    GlMemoryObjects *texture_holder_of(uint32_t texture);
    void             leave_everywhere(uint32_t buffer);
    void             leave_texture_everywhere(uint32_t texture);
    [[nodiscard]] Status admit_here(uint32_t buffer, uint32_t name, uint64_t offset, uint64_t bytes);
    [[nodiscard]] Status place_here(uint32_t buffer, uint32_t name, uint64_t offset, uint64_t bytes);
    void                 leave_here(uint32_t buffer);
    [[nodiscard]] Status admit_texture_here(uint32_t texture, uint32_t name, uint64_t offset, uint64_t bytes);
    [[nodiscard]] Status place_texture_here(uint32_t texture, uint32_t name, uint64_t offset, uint64_t bytes);
    [[nodiscard]] Status inherit_texture_here(uint32_t view, uint32_t original);
    void                 leave_texture_here(uint32_t texture);
    void                 retire_here(uint32_t name);
    void                 reclaim_here(GlHostImport &import, GlHostImport &owner);
    void                 destroy_here(GlHostImport &import);

    GlMemoryGroup *group_;
    GlHostImport  *own_;
    uint32_t       retiring_;

    void bump(Memory &memory);
};

}

#endif
