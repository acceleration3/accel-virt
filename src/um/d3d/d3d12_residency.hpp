#ifndef AV_UM_D3D12_RESIDENCY_HPP
#define AV_UM_D3D12_RESIDENCY_HPP

#include "um/d3d/d3d12_ddi.hpp"

#include <stdint.h>

namespace av::d3d12 {

class D3d12Residency {
public:
    D3d12Residency(const D3D12DDI_CORELAYER_DEVICECALLBACKS_0050 &core, D3D12DDI_HRTDEVICE runtime);
    D3d12Residency(const D3d12Residency &) = delete;
    D3d12Residency &operator=(const D3d12Residency &) = delete;

    HRESULT make_resident(D3D12DDIARG_MAKERESIDENT_0001 &args) const;
    HRESULT evict(const D3D12DDIARG_EVICT &args) const;

private:
    static constexpr uint32_t batch = 64u;

    D3DKMT_HANDLE allocation_of(const D3D12DDI_HANDLE_AND_TYPE &object) const;
    uint32_t      gather(const D3D12DDI_HANDLE_AND_TYPE *objects, uint32_t count, uint32_t first,
                         D3DKMT_HANDLE *allocations, uint32_t &consumed) const;
    HRESULT       page_in(D3D12DDIARG_MAKERESIDENT_0001 &args, const D3DKMT_HANDLE *allocations,
                          uint32_t count) const;

    const D3D12DDI_CORELAYER_DEVICECALLBACKS_0050 &core_;
    D3D12DDI_HRTDEVICE                             runtime_;
};

}

#endif
