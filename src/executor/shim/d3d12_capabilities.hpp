#ifndef AV_D3D12_CAPABILITIES_HPP
#define AV_D3D12_CAPABILITIES_HPP

#include "executor/d3d/format_caps_rows.hpp"
#include "proto/generated/d3d12/d3d12_wire_types.h"

#include <stdint.h>

struct ID3D12Device;

namespace av {

struct D3d12HostDeviceProbe;

class D3d12Capabilities {
public:
    D3d12Capabilities();

    void measure(ID3D12Device &device);
    void withhold_raytracing();

    const d3d12proto::Caps12 &caps() const;
    const FormatCapsRows     &formats() const;

private:
    void measure_options(ID3D12Device &device);
    void measure_formats(ID3D12Device &device);

    d3d12proto::Caps12 caps_;
    FormatCapsRows     formats_;

    friend struct D3d12HostDeviceProbe;
};

}

#endif
