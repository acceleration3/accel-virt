/* Which threads asked that the shared textures they make next be laid out as
 * rows (ResourceMisc): the code of ours that makes a texture another API reads
 * as rows - a Vulkan or OpenGL window's chain slot, an OpenCL image's twin -
 * asks through avumd's AvAskSharedRows around the CreateTexture2D it calls on
 * that thread, which the runtime turns into CreateResource on the same thread
 * before it returns. Every other shared texture is laid out in the GPU's own
 * tiling.
 *
 * Asked and withdrawn from any thread, read under the root's lock: each slot
 * is a thread id, claimed and cleared atomically. One per process, in the
 * Direct3D 11 driver. */
#ifndef AV_D3D_SHARED_ROWS_REQUEST_HPP
#define AV_D3D_SHARED_ROWS_REQUEST_HPP

#include <stdint.h>

namespace av::d3d {

class SharedRowsRequest {
public:
    /* More threads than ever make such textures at once. */
    static constexpr uint32_t most_threads = 8u;

    SharedRowsRequest();
    SharedRowsRequest(const SharedRowsRequest &) = delete;
    SharedRowsRequest &operator=(const SharedRowsRequest &) = delete;

    /* `thread` (non-zero) asks; false when every slot is taken, and its
     * textures are then tiled - the reader falls back to what it does for a
     * texture it cannot open. Asking again while asked is one ask. */
    bool ask(uint32_t thread);
    /* `thread` no longer asks; nothing for one that did not. */
    void withdraw(uint32_t thread);
    bool asked_by(uint32_t thread) const;

private:
    /* The slot holding `thread`, or most_threads. */
    uint32_t slot_of(uint32_t thread) const;

    uint32_t threads_[most_threads];
};

}  // namespace av::d3d

#endif  // AV_D3D_SHARED_ROWS_REQUEST_HPP
