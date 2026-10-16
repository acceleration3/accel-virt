/* The uniform shapes one context learned, for the glGetUniform queries whose
 * reply the generated client sizes inside the call, where it cannot ask the
 * host: the host answers exactly the size asked, so a size too large would
 * write past the application's buffer. A location is remembered when
 * glGetUniformLocation answers it, and a program's are forgotten when it is
 * linked again or deleted; a location never remembered answers 0, and nothing
 * is written. As many are kept as an application's programs have locations. */
#ifndef AV_GLICD_UNIFORM_SHAPES_HPP
#define AV_GLICD_UNIFORM_SHAPES_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
}  // namespace av

namespace av::glicd {

struct UniformShapesProbe;

class UniformShapes {
public:
    explicit UniformShapes(BlockSource &memory);
    UniformShapes(const UniformShapes &) = delete;
    UniformShapes &operator=(const UniformShapes &) = delete;

    /* Replaces what the location answered before; nothing for a negative
     * location or a type OpenGL does not have. no_memory when it cannot be
     * kept, and then the location answers 0 as one never remembered. */
    [[nodiscard]] Status remember(uint32_t program, int32_t location, uint32_t type);
    void                 forget(uint32_t program);
    uint32_t             values(uint32_t program, int32_t location) const;
    uint32_t             held() const;

    bool invariants_hold() const;

private:
    uint64_t key_of(uint32_t program, int32_t location) const;

    /* Each location's value count, by program and location. */
    KeyedSlots<uint64_t, uint32_t> values_;

    friend struct UniformShapesProbe;
};

}  // namespace av::glicd

#endif  // AV_GLICD_UNIFORM_SHAPES_HPP
