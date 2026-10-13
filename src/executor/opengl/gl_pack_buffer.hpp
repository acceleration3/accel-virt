/* A context's guest region as a GL buffer a pixel transfer packs into
 * (RealGl::av_pack_blob, docs/compositor.md): a read-back of a frame into the
 * guest's memory is written there by the GPU, rather than read back by the
 * driver and copied into a pointer by the CPU. Made over the dma-buf the
 * region was imported as, the first time a transfer asks; a region with none,
 * or a driver that imports no memory object, packs nothing and the pointer is
 * used as before. */
#ifndef AV_GL_PACK_BUFFER_HPP
#define AV_GL_PACK_BUFFER_HPP

#include <stdint.h>

namespace av::glproto {
struct RealGl;
}  // namespace av::glproto

namespace av {

class GlHostImport;

class GlPackBuffer {
public:
    GlPackBuffer();
    ~GlPackBuffer();
    GlPackBuffer(const GlPackBuffer &) = delete;
    GlPackBuffer &operator=(const GlPackBuffer &) = delete;

    /* The region, `size` bytes, as the dma-buf `fd` - copied, the caller's
     * kept; -1 for a region that is no dma-buf. */
    void adopt(int fd, uint64_t size);
    /* The region gone: the buffer is let go the next time the context is
     * current here. */
    void abandon();
    /* On the context's thread: the buffer bound as the pack buffer and
     * `offset` what a transfer of `bytes` at `at` names instead of a pointer;
     * false where there is no buffer, or the range is not the region's. */
    [[nodiscard]] bool bind(const glproto::RealGl &gl, GlHostImport &import, uint64_t at, uint64_t bytes, void *&offset);
    /* After the transfer: unbound, and its write finished. */
    void               unbind(const glproto::RealGl &gl);
    /* Whether a buffer is made over the region. */
    bool               packing() const;
    /* On the context's thread, as it closes: the GL objects deleted. */
    void               release(const glproto::RealGl &gl, GlHostImport &import);

private:
    [[nodiscard]] bool make(const glproto::RealGl &gl, GlHostImport &import);

    int      fd_;
    uint64_t size_;
    uint32_t memory_;
    uint32_t buffer_;
    bool     refused_;
    bool     stale_;
};

}  // namespace av

#endif  // AV_GL_PACK_BUFFER_HPP
