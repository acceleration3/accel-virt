#ifndef AV_UM_D3D12_HOST_CAPS_HPP
#define AV_UM_D3D12_HOST_CAPS_HPP

#include "icd/d3d/dxgi_format_table.hpp"
#include "proto/d3d/dxgi_format.hpp"
#include "proto/generated/d3d12/d3d12_wire_types.h"
#include "um/d3d/d3d12_ddi.hpp"

#include <stdint.h>

namespace av {
class ClientTransport;
class Kmt;
}

namespace av::d3d12 {

class D3d12HostCaps {
public:
    D3d12HostCaps();
    D3d12HostCaps(const D3d12HostCaps &) = delete;
    D3d12HostCaps &operator=(const D3d12HostCaps &) = delete;

    [[nodiscard]] bool learn(ClientTransport &transport, Kmt &kmt, uint32_t adapter);
    bool               learned() const;

    HRESULT answer(const D3D12DDIARG_GETCAPS &args) const;
    UINT    format_support(DXGI_FORMAT format) const;
    UINT    quality_levels(DXGI_FORMAT format, UINT samples) const;

private:
    uint32_t pipeline_level() const;
    uint32_t tiled_tier() const;
    HRESULT  options(void *data, UINT bytes) const;
    HRESULT  shader(void *data, UINT bytes) const;
    HRESULT  shader_models(void *data, UINT bytes) const;
    HRESULT  fill(void *data, UINT bytes, const void *answer, UINT answer_bytes) const;

    d3d12proto::Caps12 caps_;
    d3d::DxgiFormatTable formats_;
    bool               learned_;
};

}

#endif
