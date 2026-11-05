#ifndef AV_GL_ERROR_QUEUE_HPP
#define AV_GL_ERROR_QUEUE_HPP

#include "proto/generated/opengl/gl_real_dispatch.h"

#include <stdint.h>

namespace av {

class GlErrorWord;

class GlErrorQueue {
public:
    static constexpr uint32_t most_collected = 8u;

    GlErrorQueue(const glproto::RealGl &gl, GlErrorWord &word);
    GlErrorQueue(const GlErrorQueue &) = delete;
    GlErrorQueue &operator=(const GlErrorQueue &) = delete;

    void     collect();
    uint32_t take();
    void     record(uint32_t error);
    void     recover();

    bool lost() const;

    bool invariants_hold() const;

private:
    const glproto::RealGl &gl_;
    GlErrorWord           &word_;
    bool                   lost_;
};

}

#endif
