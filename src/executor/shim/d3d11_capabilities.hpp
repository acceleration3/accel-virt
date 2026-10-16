/* What one host GPU's Direct3D 11 device can do, asked once when it is made:
 * the answers the guest's user-mode driver reports through GetCaps, and every
 * DXGI format's support, sample counts and quality levels. Asked of the
 * device itself rather than assumed, so a guest on a host whose driver lacks
 * a feature is told it lacks it. */
#ifndef AV_D3D11_CAPABILITIES_HPP
#define AV_D3D11_CAPABILITIES_HPP

#include "executor/d3d/format_caps_rows.hpp"
#include "proto/generated/d3d/d3d_wire_types.h"

#include <stdint.h>

struct ID3D11Device5;

namespace av {

class D3d11Capabilities {
public:
    D3d11Capabilities();

    void measure(ID3D11Device5 &device, uint32_t feature_level);

    const d3dproto::Caps11 &caps() const;
    const FormatCapsRows   &formats() const;

private:
    void measure_options(ID3D11Device5 &device);
    void measure_formats(ID3D11Device5 &device);

    d3dproto::Caps11 caps_;
    FormatCapsRows   formats_;
};

}  // namespace av

#endif  // AV_D3D11_CAPABILITIES_HPP
