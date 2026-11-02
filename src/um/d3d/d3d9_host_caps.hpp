#ifndef AV_UM_D3D9_HOST_CAPS_HPP
#define AV_UM_D3D9_HOST_CAPS_HPP

#include "icd/d3d/d3d9_format_ops.hpp"
#include "proto/generated/d3d9/d3d9_wire_types.h"
#include "um/d3d/d3d9_ddi.hpp"

#include <stdint.h>

namespace av {
class ClientTransport;
class Kmt;
}

namespace av::d3d9 {

class D3d9HostCaps {
public:
    D3d9HostCaps();
    D3d9HostCaps(const D3d9HostCaps &) = delete;
    D3d9HostCaps &operator=(const D3d9HostCaps &) = delete;

    [[nodiscard]] bool learn(ClientTransport &transport, Kmt &kmt, uint32_t adapter);
    bool               learned() const;

    HRESULT answer(const D3DDDIARG_GETCAPS &args) const;
    bool    present_snapshots() const;
    const D3DCAPS9 &caps() const;

private:
    void    fix_up();
    HRESULT copy_out(const D3DDDIARG_GETCAPS &args, const void *data, size_t bytes) const;
    HRESULT answer_formats(const D3DDDIARG_GETCAPS &args) const;
    HRESULT answer_quality(const D3DDDIARG_GETCAPS &args) const;

    D3DCAPS9            caps_;
    d3d::D3d9FormatOps  formats_;
    d3d9proto::FormatCaps9 rows_[d3d::D3d9FormatOps::max_formats];
    bool                snapshots_;
    bool                learned_;
};

}

#endif
