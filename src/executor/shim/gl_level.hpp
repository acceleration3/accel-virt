#ifndef AV_GL_LEVEL_HPP
#define AV_GL_LEVEL_HPP

#include <stdint.h>

namespace av {

class GlLevel {
public:
    GlLevel();
    GlLevel(uint32_t texture, uint32_t bind_target, uint32_t image_target, uint32_t dimensions, int32_t level);

    void moves_as(uint32_t format, uint32_t type, uint32_t width, uint32_t height, uint32_t depth);

    uint32_t texture() const;
    uint32_t bind_target() const;
    uint32_t image_target() const;
    uint32_t dimensions() const;
    int32_t  level() const;
    uint32_t format() const;
    uint32_t type() const;
    uint32_t width() const;
    uint32_t height() const;
    uint32_t depth() const;

private:
    uint32_t texture_;
    uint32_t bind_target_;
    uint32_t image_target_;
    uint32_t dimensions_;
    int32_t  level_;
    uint32_t format_;
    uint32_t type_;
    uint32_t width_;
    uint32_t height_;
    uint32_t depth_;
};

}

#endif
