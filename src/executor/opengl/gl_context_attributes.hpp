/* The EGL 1.5 attribute list an OpenGL context is made with, from the options
 * a guest asked for (feature 050): its version, profile and the debug, forward
 * compatible and robustness flags, each only when asked. Options no OpenGL
 * has - 4.7, a profile or flag not defined, a reserved field set - make a list
 * that is not valid() and holds nothing, so EGL is never asked for them. */
#ifndef AV_GL_CONTEXT_ATTRIBUTES_HPP
#define AV_GL_CONTEXT_ATTRIBUTES_HPP

#include "device/abi/av_executor.h"

#include <stdint.h>

namespace av {

class GlContextAttributes {
public:
    /* Null is the defaults. */
    explicit GlContextAttributes(const AvContextOptions *options);

    bool           valid() const;
    /* Name, value pairs ending in EGL_NONE. */
    const int32_t *list() const;

private:
    /* Seven pairs and the terminator. */
    static constexpr uint32_t capacity = 15u;

    bool acceptable() const;
    void add(int32_t name, int32_t value);

    AvContextOptions options_;
    int32_t  list_[capacity];
    uint32_t used_;
    bool     valid_;
};

}  // namespace av

#endif  // AV_GL_CONTEXT_ATTRIBUTES_HPP
