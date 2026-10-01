/* The kernel allocations a Direct3D 12 device's pageable objects own, made
 * resident and evicted on the paging queue D3D12Core names in each call.
 *
 * D3D12Core keeps residency itself: it makes a committed resource's heap
 * resident when it makes it, and an application's MakeResident and Evict are
 * the same calls. Nearly every heap here is the host's or the session arena's
 * and owns no kernel allocation, so nothing is paged for it. A shown surface
 * owns one (D3d12Device::show), and a blt-model present names it in the DMA
 * buffer dxgkrnl builds: dxgkrnl refuses that present, before the miniport
 * ever sees it, for an allocation nobody asked to be resident - which is how
 * Direct3D 9 on 12's windowed presents were lost. */
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

    /* E_PENDING with the paging fence each adapter's queue reaches once the
     * allocations are in, S_OK when none was needed or the kernel already had
     * them; the kernel's refusal otherwise. */
    HRESULT make_resident(D3D12DDIARG_MAKERESIDENT_0001 &args) const;
    HRESULT evict(const D3D12DDIARG_EVICT &args) const;

private:
    /* One call's worth: more objects than this are paged in turns. */
    static constexpr uint32_t batch = 64u;

    /* The kernel allocation `object` owns, 0 for none. */
    D3DKMT_HANDLE allocation_of(const D3D12DDI_HANDLE_AND_TYPE &object) const;
    /* Objects from `first` on, up to `batch` of those that own an allocation,
     * into `allocations`; how many objects were looked at in `consumed`. */
    uint32_t      gather(const D3D12DDI_HANDLE_AND_TYPE *objects, uint32_t count, uint32_t first,
                         D3DKMT_HANDLE *allocations, uint32_t &consumed) const;
    HRESULT       page_in(D3D12DDIARG_MAKERESIDENT_0001 &args, const D3DKMT_HANDLE *allocations,
                          uint32_t count) const;

    const D3D12DDI_CORELAYER_DEVICECALLBACKS_0050 &core_;
    D3D12DDI_HRTDEVICE                             runtime_;
};

}  // namespace av::d3d12

#endif  // AV_UM_D3D12_RESIDENCY_HPP
