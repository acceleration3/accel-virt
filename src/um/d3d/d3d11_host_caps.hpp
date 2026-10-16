/* What the host GPU's Direct3D 11 device can do, asked once per adapter.
 *
 * The runtime asks an adapter for its capabilities (GetCaps) before it makes
 * any device, so this opens a session of its own on the adapter, asks the
 * host (QueryCaps11) and closes it again, keeping the answer: the feature
 * level, the options the DDI reports, and every format's support. A device's
 * CheckFormatSupport and CheckMultisampleQualityLevels are answered from the
 * same table, so the runtime is told one story. */
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
}  // namespace av

namespace av::d3d11 {

class D3d11HostCaps {
public:
    D3d11HostCaps();
    D3d11HostCaps(const D3d11HostCaps &) = delete;
    D3d11HostCaps &operator=(const D3d11HostCaps &) = delete;

    /* Asks the host through `adapter` (a D3DKMT adapter handle). False when
     * the adapter runs no Direct3D, which the runtime is told as a failed
     * GetCaps. */
    [[nodiscard]] bool learn(ClientTransport &transport, Kmt &kmt, uint32_t adapter);
    bool               learned() const;

    /* One GetCaps question, answered into the caller's buffer. */
    HRESULT answer(const D3D10_2DDIARG_GETCAPS &args) const;
    /* D3D_FEATURE_LEVEL the host device runs at. */
    uint32_t feature_level() const;
    /* Whether a blt-model present may name a present snapshot rather than its
     * buffer read back: what the miniport's UMDRIVERPRIVATE said. */
    bool     present_snapshots() const;
    UINT     format_support(DXGI_FORMAT format) const;
    UINT     quality_levels(DXGI_FORMAT format, UINT samples) const;

private:
    /* The DDI's pipeline levels from the host's feature level, each a bit. */
    UINT pipeline_levels() const;

    d3dproto::Caps11     caps_;
    d3d::DxgiFormatTable formats_;
    bool                 snapshots_;
    bool                 learned_;
};

}  // namespace av::d3d11

#endif  // AV_UM_D3D11_HOST_CAPS_HPP
