/* The uniform shapes one context learned, for the glGetUniform queries whose
 * reply the generated client sizes inside the call, where it cannot ask the
 * host: the host answers exactly the size asked, so a size too large would
 * write past the application's buffer. A location is remembered when
 * glGetUniformLocation answers it, and a program's are forgotten when it is
 * linked again or deleted; a location never remembered answers 0, and nothing
 * is written. */
#ifndef AV_GLICD_UNIFORM_SHAPES_HPP
#define AV_GLICD_UNIFORM_SHAPES_HPP

#include "icd/opengl/uniform_shape.hpp"

#include <stdint.h>

namespace av::glicd {

struct UniformShapesProbe;

class UniformShapes {
public:
    static constexpr uint32_t capacity = 1024u;

    UniformShapes();
    UniformShapes(const UniformShapes &) = delete;
    UniformShapes &operator=(const UniformShapes &) = delete;

    /* Replaces what the location answered before; ignored for a negative
     * location, a type OpenGL does not have, or once `capacity` are held. */
    void     remember(uint32_t program, int32_t location, uint32_t type);
    void     forget(uint32_t program);
    uint32_t values(uint32_t program, int32_t location) const;
    uint32_t held() const;

    bool invariants_hold() const;

private:
    uint32_t row_of(uint32_t program, int32_t location) const;

    UniformShape shapes_[capacity];
    uint32_t     count_;

    friend struct UniformShapesProbe;
};

}  // namespace av::glicd

#endif  // AV_GLICD_UNIFORM_SHAPES_HPP
