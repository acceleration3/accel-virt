#ifndef AV_UM_D3D12_RECORDER_HPP
#define AV_UM_D3D12_RECORDER_HPP

#include "proto/generated/d3d12/d3d12_wire_types.h"
#include "um/d3d/d3d12_ddi.hpp"

#include <stdint.h>

namespace av::d3d12 {

class D3d12Device;

class D3d12Recorder {
public:
    explicit D3d12Recorder(D3d12Device &device);
    D3d12Recorder(const D3d12Recorder &) = delete;
    D3d12Recorder &operator=(const D3d12Recorder &) = delete;

#include "um/d3d/generated/d3d12_recorder_ddi.inc"

private:
    uint32_t list(D3D12DDI_HCOMMANDLIST handle) const;
    void     sent(bool sent_ok, const char *what);
    void     set_root_view(D3D12DDI_HCOMMANDLIST handle, uint32_t compute, UINT parameter, uint32_t kind,
                           D3D12DDI_GPU_VIRTUAL_ADDRESS address);
    d3d12proto::CopyLocation location(const D3D12DDIARG_BUFFER_PLACEMENT &placement,
                                      const D3D12DDIARG_PLACED_RESOURCE &layout) const;

    D3d12Device &device_;
};

}

#endif
