/* A kernel allocation's pages lent to the host: the pages of an allocation the
 * runtime made for a cross-adapter resource or heap - an application's own
 * (Direct3D 12's existing heaps) or the driver's (a Direct3D 11 texture shared
 * between two adapters) - found through the runtime's lock callback and lent
 * to the session's device as host memory laid under them (docs/host-memory.md,
 * "Application memory"). The lend is the same whichever API asks and whichever
 * adapter's device is lent them: the device, not the guest, decides whether it
 * makes new memory or finds the very pages lent through another adapter
 * already and joins it. Direct3D 11 and Direct3D 12 devices each hold one. */
#ifndef AV_UM_ALLOCATION_LENDER_HPP
#define AV_UM_ALLOCATION_LENDER_HPP

#include "core/support/status.hpp"
#include "um/d3d/d3d11_ddi.hpp"

#include <stdint.h>

namespace av {
class HostAllocation;
}  // namespace av

namespace av::d3d {

class D3dSession;

class AllocationLender {
public:
    /* Lends through `session`. */
    explicit AllocationLender(D3dSession &session);
    AllocationLender(const AllocationLender &) = delete;
    AllocationLender &operator=(const AllocationLender &) = delete;

    /* The callbacks and device handle the runtime gave CreateDevice; the
     * callbacks outlive the call. */
    void   bind(const D3DDDI_DEVICECALLBACKS *callbacks, HANDLE runtime);
    /* The `bytes` of `allocation`'s pages (whole pages), found through a lock
     * of it that is held across the lend, lent as readback
     * memory - the pages are write-back in the process, so the memory under
     * them must be cached - and imported: `made` is the host's allocation, its
     * handle the guest's name for it in the API's commands. no_memory when
     * the kernel has no pages to lend, otherwise the session's refusal. */
    [[nodiscard]] Status lend(D3DKMT_HANDLE allocation, uint64_t bytes, HostAllocation *&made) const;
    /* Closed now, for memory no work of the host's is over; or retired, for
     * memory the host's queued work may still read and write, closed once the
     * GPU has got past it. */
    void  close(HostAllocation &made) const;
    void  retire(HostAllocation &made) const;

private:
    D3dSession                    &session_;
    const D3DDDI_DEVICECALLBACKS *callbacks_;
    HANDLE                         runtime_;
};

}  // namespace av::d3d

#endif  // AV_UM_ALLOCATION_LENDER_HPP
