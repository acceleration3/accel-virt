/* One DxgkDdiRender call: the command buffer a user-mode driver recorded moves
 * into its DMA buffer, and its patch locations into the list Patch will read.
 * Every size, index and offset comes from user mode and is checked before a
 * byte moves. */
#ifndef AV_RENDER_REQUEST_HPP
#define AV_RENDER_REQUEST_HPP

#include "platform.hpp"
#include "status.hpp"

#include <stdint.h>

namespace av {

struct DxgkargRenderRecord;

class RenderRequest {
public:
    RenderRequest(Platform &platform, DxgkargRenderRecord &request);
    RenderRequest(const RenderRequest &) = delete;
    RenderRequest &operator=(const RenderRequest &) = delete;

    /* Copies, then advances the DMA buffer and output patch list past what it wrote. */
    [[nodiscard]] Status render();

private:
    [[nodiscard]] Status validate() const;

    Platform            &platform_;
    DxgkargRenderRecord &request_;
};

}  // namespace av

#endif  // AV_RENDER_REQUEST_HPP
