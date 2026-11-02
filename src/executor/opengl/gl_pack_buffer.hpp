#ifndef AV_GL_PACK_BUFFER_HPP
#define AV_GL_PACK_BUFFER_HPP

#include "executor/owned_fd.hpp"

#include <stdint.h>

namespace av::glproto {
struct RealGl;
}

namespace av {

class GlSharedBuffer;

class GlPackBuffer {
public:
    GlPackBuffer(const glproto::RealGl &gl, GlSharedBuffer &buffers);
    ~GlPackBuffer();
    GlPackBuffer(const GlPackBuffer &) = delete;
    GlPackBuffer &operator=(const GlPackBuffer &) = delete;

    void adopt(OwnedFd &descriptor, uint64_t size);
    void abandon();
    [[nodiscard]] bool bind(uint64_t at, uint64_t bytes, void *&offset);
    void               unbind();
    bool               packing() const;
    void               release();

private:
    [[nodiscard]] bool make();

    const glproto::RealGl &gl_;
    GlSharedBuffer        &buffers_;
    OwnedFd   fd_;
    uint64_t size_;
    uint32_t memory_;
    uint32_t buffer_;
    bool     refused_;
    bool     stale_;
};

}

#endif
