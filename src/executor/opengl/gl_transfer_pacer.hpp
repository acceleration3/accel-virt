#ifndef AV_GL_TRANSFER_PACER_HPP
#define AV_GL_TRANSFER_PACER_HPP

#include "executor/gpu_work_window.hpp"
#include "proto/generated/opengl/gl_real_dispatch.h"
#include "proto/wire/wire_reader.hpp"

#include <stdint.h>

namespace av {

class GlTransferPacer {
public:
    GlTransferPacer();
    GlTransferPacer(const GlTransferPacer &) = delete;
    GlTransferPacer &operator=(const GlTransferPacer &) = delete;

    void     ran(const glproto::RealGl &gl, uint32_t command_id, WireReader payload);
    void     stop(const glproto::RealGl &gl);
    uint64_t cost(const glproto::RealGl &gl, uint32_t command_id, WireReader payload) const;

    bool invariants_hold() const;

private:
    void settle(const glproto::RealGl &gl);

    GpuWorkWindow window_;
    void         *previous_;
};

}

#endif
