#ifndef AV_GLICD_REPORTED_GL_ERRORS_HPP
#define AV_GLICD_REPORTED_GL_ERRORS_HPP

#include "proto/submission/gl_error_word.hpp"

#include <stdint.h>

namespace av::glicd {

class ReportedGlErrors {
public:
    ReportedGlErrors();

    bool     readable(uint64_t word) const;
    uint32_t take(uint64_t word);

private:
    uint8_t reported_[GlErrorWord::kinds];
};

}

#endif
