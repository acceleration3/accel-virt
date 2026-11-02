#ifndef AV_GL_CL_TARGET_HPP
#define AV_GL_CL_TARGET_HPP

#include <CL/cl.h>

#include <stdint.h>

namespace av {

class GlClTarget {
public:
    explicit GlClTarget(uint32_t target);

    bool     known() const;
    uint32_t image_target() const;
    uint32_t bind_target() const;
    uint32_t image_type() const;
    uint32_t object_type() const;
    uint32_t dimensions() const;
    void     describe(uint32_t width, uint32_t height, uint32_t depth, cl_image_desc &desc) const;

private:
    uint32_t image_target_;
    uint32_t bind_target_;
    uint32_t image_type_;
    uint32_t object_type_;
    uint32_t dimensions_;
};

}

#endif
