#ifndef AV_GLICD_BINDLESS_HANDLES_HPP
#define AV_GLICD_BINDLESS_HANDLES_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
}

namespace av::glicd {

class BindlessHandles {
public:
    enum class Kind : uint8_t { texture, image };

    explicit BindlessHandles(BlockSource &memory);
    BindlessHandles(const BindlessHandles &) = delete;
    BindlessHandles &operator=(const BindlessHandles &) = delete;

    [[nodiscard]] Status add(uint32_t group, uint64_t handle, Kind kind, uint32_t texture);
    bool                 valid(uint32_t group, uint64_t handle, Kind kind) const;
    void                 forget_textures(uint32_t group, int32_t count, const uint32_t *names);
    uint32_t             count() const;

    bool invariants_hold() const;

private:
    struct Entry {
        uint64_t handle_;
        uint32_t group_;
        uint32_t texture_;
        Kind     kind_;
    };

    static uint64_t key_of(uint32_t group, uint64_t handle);

    KeyedSlots<uint64_t, Entry> entries_;
};

}

#endif
