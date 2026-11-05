#ifndef AV_GL_PACK_BUFFER_HPP
#define AV_GL_PACK_BUFFER_HPP

#include "executor/owned_fd.hpp"
#include "proto/wire/guest_bytes.hpp"

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

    void adopt(OwnedFd &descriptor, uint64_t base, uint64_t size);
    void abandon(uint64_t base);
    void abandon_all();
    [[nodiscard]] bool bind(uint64_t at, uint64_t bytes, void *&offset);
    void               unbind();
    bool               packing() const;
    void               sweep();
    void               release();

private:
    struct Segment {
        OwnedFd  fd_;
        uint64_t base_;
        uint64_t size_;
        uint32_t memory_;
        uint32_t buffer_;
        bool     refused_;
        bool     stale_;
    };

    Segment           *covering(uint64_t at);
    [[nodiscard]] bool make(Segment &segment);
    void               destroy(Segment &segment);

    const glproto::RealGl &gl_;
    GlSharedBuffer        &buffers_;
    Segment                segments_[GuestBytes::slot_count];
};

}

#endif
