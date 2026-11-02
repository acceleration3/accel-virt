#ifndef AV_GLICD_UNIFORM_SHAPES_HPP
#define AV_GLICD_UNIFORM_SHAPES_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
}

namespace av::glicd {

struct UniformShapesProbe;

class UniformShapes {
public:
    explicit UniformShapes(BlockSource &memory);
    UniformShapes(const UniformShapes &) = delete;
    UniformShapes &operator=(const UniformShapes &) = delete;

    [[nodiscard]] Status remember(uint32_t program, int32_t location, uint32_t type);
    void                 forget(uint32_t program);
    uint32_t             values(uint32_t program, int32_t location) const;
    uint32_t             held() const;

    bool invariants_hold() const;

private:
    uint64_t key_of(uint32_t program, int32_t location) const;

    KeyedSlots<uint64_t, uint32_t> values_;

    friend struct UniformShapesProbe;
};

}

#endif
