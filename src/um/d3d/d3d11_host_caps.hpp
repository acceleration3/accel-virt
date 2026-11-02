#ifndef AV_UM_D3D11_HOST_CAPS_HPP
#define AV_UM_D3D11_HOST_CAPS_HPP

#include "icd/d3d/dxgi_format_table.hpp"
#include "proto/d3d/dxgi_format.hpp"
#include "proto/generated/d3d/d3d_wire_types.h"
#include "um/d3d/d3d11_ddi.hpp"

#include <stdint.h>

namespace av {
class ClientTransport;
class Kmt;
}

namespace av::d3d11 {

class D3d11HostCaps {
public:
    D3d11HostCaps();
    D3d11HostCaps(const D3d11HostCaps &) = delete;
    D3d11HostCaps &operator=(const D3d11HostCaps &) = delete;

    [[nodiscard]] bool learn(ClientTransport &transport, Kmt &kmt, uint32_t adapter);
    bool               learned() const;

    HRESULT answer(const D3D10_2DDIARG_GETCAPS &args) const;
    uint32_t feature_level() const;
    bool     present_snapshots() const;
    UINT     format_support(DXGI_FORMAT format) const;
    UINT     quality_levels(DXGI_FORMAT format, UINT samples) const;

private:
    UINT pipeline_levels() const;

    d3dproto::Caps11     caps_;
    d3d::DxgiFormatTable formats_;
    bool                 snapshots_;
    bool                 learned_;
};

}

#endif
