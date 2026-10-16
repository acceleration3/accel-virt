/* A context's guest region as a GL buffer a pixel transfer packs into
 * (RealGl::av_pack_blob, docs/compositor.md): a read-back of a frame into the
 * guest's memory is written there by the GPU, rather than read back by the
 * driver and copied into a pointer by the CPU. Made over the dma-buf the
 * region was imported as (GlSharedBuffer), the first time a transfer asks; a
 * region with none, or a driver that imports no memory object, packs nothing
 * and the pointer is used as before. */
#ifndef AV_GL_PACK_BUFFER_HPP
#define AV_GL_PACK_BUFFER_HPP

#include <stdint.h>

namespace av::glproto {
struct RealGl;
}  // namespace av::glproto

namespace av {

class GlSharedBuffer;

class GlPackBuffer {
public:
    /* `gl` is the context's table and `buffers` makes buffers in that
     * context; both must outlive this. */
    GlPackBuffer(const glproto::RealGl &gl, GlSharedBuffer &buffers);
    ~GlPackBuffer();
    GlPackBuffer(const GlPackBuffer &) = delete;
    GlPackBuffer &operator=(const GlPackBuffer &) = delete;

    /* The region, `size` bytes, as the dma-buf `fd`, which is this buffer's
     * from now on; -1 for a region that is no dma-buf. */
    void adopt(int fd, uint64_t size);
    /* The region gone: the buffer is let go the next time the context is
     * current here. */
    void abandon();
    /* On the context's thread: the buffer bound as the pack buffer and
     * `offset` what a transfer of `bytes` at `at` names instead of a pointer;
     * false where there is no buffer, or the range is not the region's. */
    [[nodiscard]] bool bind(uint64_t at, uint64_t bytes, void *&offset);
    /* After the transfer: unbound, and its write finished. */
    void               unbind();
    /* Whether a buffer is made over the region. */
    bool               packing() const;
    /* On the context's thread, as it closes: the GL objects deleted. */
    void               release();

private:
    [[nodiscard]] bool make();

    const glproto::RealGl &gl_;
    GlSharedBuffer        &buffers_;
    int      fd_;
    uint64_t size_;
    uint32_t memory_;
    uint32_t buffer_;
    bool     refused_;
    bool     stale_;
};

}  // namespace av

#endif  // AV_GL_PACK_BUFFER_HPP
