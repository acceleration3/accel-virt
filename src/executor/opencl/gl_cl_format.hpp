#ifndef AV_GL_CL_FORMAT_HPP
#define AV_GL_CL_FORMAT_HPP

#include <stdint.h>

namespace av {

class GlClFormat {
public:
    explicit GlClFormat(uint32_t internal_format);

    bool     known() const;
    uint32_t order() const;
    uint32_t type() const;
    uint32_t transfer_format() const;
    uint32_t transfer_type() const;
    uint32_t texel_bytes() const;

private:
    uint32_t order_;
    uint32_t type_;
    uint32_t transfer_format_;
    uint32_t transfer_type_;
    uint32_t texel_bytes_;
};

}

#endif
