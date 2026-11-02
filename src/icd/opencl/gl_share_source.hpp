#ifndef AV_CLICD_GL_SHARE_SOURCE_HPP
#define AV_CLICD_GL_SHARE_SOURCE_HPP

#include <stdint.h>

namespace av::clicd {

class GlShareSource {
public:
    GlShareSource(const GlShareSource &) = delete;
    GlShareSource &operator=(const GlShareSource &) = delete;

    virtual uint32_t share_name(void *gl_context) = 0;
    virtual void     run_held_calls() = 0;

protected:
    GlShareSource() = default;
    ~GlShareSource() = default;
};

}

#endif
