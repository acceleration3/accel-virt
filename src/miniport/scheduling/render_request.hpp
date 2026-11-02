#ifndef AV_RENDER_REQUEST_HPP
#define AV_RENDER_REQUEST_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

struct DxgkargRenderRecord;

class RenderRequest {
public:
    RenderRequest(Platform &platform, DxgkargRenderRecord &request);
    RenderRequest(const RenderRequest &) = delete;
    RenderRequest &operator=(const RenderRequest &) = delete;

    [[nodiscard]] Status render();

private:
    [[nodiscard]] Status validate() const;

    Platform            &platform_;
    DxgkargRenderRecord &request_;
};

}

#endif
