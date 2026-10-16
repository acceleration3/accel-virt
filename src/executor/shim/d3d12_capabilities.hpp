/* What one host GPU's Direct3D 12 device can do, asked once when it is made:
 * the answers the guest's user-mode driver reports through GetCaps, and every
 * DXGI format's support and quality levels, as vkd3d-proton's
 * CheckFeatureSupport gives them. The guest asks for all of it in one
 * QueryCaps12, so a device open costs it one round trip, not one a format. */
#ifndef AV_D3D12_CAPABILITIES_HPP
#define AV_D3D12_CAPABILITIES_HPP

#include "executor/d3d/format_caps_rows.hpp"
#include "proto/generated/d3d12/d3d12_wire_types.h"

#include <stdint.h>

struct ID3D12Device;

namespace av {

class D3d12Capabilities {
public:
    D3d12Capabilities();

    void measure(ID3D12Device &device);

    const d3d12proto::Caps12 &caps() const;
    const FormatCapsRows     &formats() const;

private:
    void measure_options(ID3D12Device &device);
    void measure_formats(ID3D12Device &device);

    d3d12proto::Caps12 caps_;
    FormatCapsRows     formats_;
};

}  // namespace av

#endif  // AV_D3D12_CAPABILITIES_HPP
