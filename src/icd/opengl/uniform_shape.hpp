/* How many values one uniform location of one program answers a glGetUniform
 * with, from the uniform's type (OpenGL 4.6 table 7.3). */
#ifndef AV_GLICD_UNIFORM_SHAPE_HPP
#define AV_GLICD_UNIFORM_SHAPE_HPP

#include <stdint.h>

namespace av::glicd {

class UniformShape {
public:
    UniformShape();
    UniformShape(uint32_t program, int32_t location, uint32_t type);

    uint32_t program() const;
    int32_t  location() const;
    /* 0 for a type OpenGL does not have. */
    uint32_t values() const;

private:
    uint32_t values_of(uint32_t type) const;

    uint32_t program_;
    int32_t  location_;
    uint32_t values_;
};

}  // namespace av::glicd

#endif  // AV_GLICD_UNIFORM_SHAPE_HPP
