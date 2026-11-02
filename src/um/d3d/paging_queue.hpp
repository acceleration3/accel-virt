/* A device's paging queue: what the kernel pages the device's allocations in
 * through, and the fence it signals once they are.
 *
 * Under WDDM 2's residency model a DMA buffer may only name an allocation its
 * device asked the kernel to keep resident. Host resources need no guest
 * residency, so nothing else here asks - but a blt-model present names the
 * presented surface in the allocation list of the blt dxgkrnl builds, and
 * dxgkrnl puts the device in error for one never asked for ("Failed to
 * reference DMA buffer: Allocation is not requested to be resident"), which
 * the application sees as DXGI_ERROR_DEVICE_HUNG on its first Present.
 *
 * The queue itself is made the first time an allocation is asked for. */
#ifndef AV_UM_PAGING_QUEUE_HPP
#define AV_UM_PAGING_QUEUE_HPP

#include "um/d3d/d3d11_ddi.hpp"

#include <stdint.h>

namespace av {
class PollClock;
}  // namespace av

namespace av::d3d11 {

class PagingQueue {
public:
    PagingQueue(const D3DDDI_DEVICECALLBACKS &kernel, HANDLE device, PollClock &clock);
    ~PagingQueue();
    PagingQueue(const PagingQueue &) = delete;
    PagingQueue &operator=(const PagingQueue &) = delete;

    /* S_OK once `allocation` is resident, waiting for the paging the kernel
     * queued for it; the kernel's refusal otherwise. */
    [[nodiscard]] HRESULT make_resident(D3DKMT_HANDLE allocation);
    /* The first `pages` 4 KiB pages of `allocation` mapped read/write at an
     * address of this process's GPU virtual space the kernel picks, `address`.
     * The paging the kernel queued for it is not waited for: nothing on a GPU
     * reads through the address - dxgkrnl only checks one is assigned. E_NOTIMPL
     * when the runtime gave the device no callback to map with, the kernel's
     * refusal otherwise. */
    [[nodiscard]] HRESULT map_address(D3DKMT_HANDLE allocation, uint64_t pages, uint64_t &address);
    /* The `pages` pages mapped at `address` given back; S_OK for nothing, 0. */
    [[nodiscard]] HRESULT unmap_address(uint64_t address, uint64_t pages);

private:
    [[nodiscard]] HRESULT open();
    [[nodiscard]] HRESULT wait_for(uint64_t fence) const;

    const D3DDDI_DEVICECALLBACKS &kernel_;
    HANDLE                        device_;
    PollClock                    &clock_;
    D3DKMT_HANDLE                 queue_;
    const volatile uint64_t      *fence_;
};

}  // namespace av::d3d11

#endif  // AV_UM_PAGING_QUEUE_HPP
