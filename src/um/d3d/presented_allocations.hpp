/* The kernel allocations a Direct3D 9 or 11 device presents from, and the
 * context it presents on: each allocation made over a KernelSurface's private
 * data and asked to be resident (PagingQueue), since a blt-model present names
 * it in a DMA buffer dxgkrnl refuses otherwise. What the kernel refuses is
 * said once per answer, at the point of decision, with the answer. */
#ifndef AV_UM_PRESENTED_ALLOCATIONS_HPP
#define AV_UM_PRESENTED_ALLOCATIONS_HPP

#include "core/support/object_storage.hpp"
#include "um/d3d/d3d11_ddi.hpp"
#include "um/d3d/paging_queue.hpp"

namespace av {
class KeyedLog;
class PollClock;
}  // namespace av

namespace av::d3d {

class KernelDevice;
class KernelSurface;

class PresentedAllocations {
public:
    PresentedAllocations(KernelDevice &kernel, KeyedLog &lines, PollClock &clock);
    ~PresentedAllocations();
    PresentedAllocations(const PresentedAllocations &) = delete;
    PresentedAllocations &operator=(const PresentedAllocations &) = delete;
    PresentedAllocations(PresentedAllocations &&) = delete;
    PresentedAllocations &operator=(PresentedAllocations &&) = delete;

    /* The allocation of `surface`, made (KernelSurface::make) beforehand,
     * belonging to the runtime's `resource` or, made long after it, to none.
     * A refusal to keep it resident is said and the allocation still made: a
     * flip never names it. */
    [[nodiscard]] HRESULT make(KernelSurface &surface, HANDLE resource, D3DKMT_HANDLE &allocation);
    /* What `make` made, given back; the kernel's refusal is said once. */
    void give_back(D3DKMT_HANDLE allocation, HANDLE resource);
    /* An allocation another device made and this one opened - a window's
     * redirection surface, which a present names as its destination - asked
     * to be resident for this device; the kernel's refusal is said once. */
    [[nodiscard]] HRESULT keep_resident(D3DKMT_HANDLE allocation);
    /* The device's own context, on the one engine the miniport runs. */
    [[nodiscard]] HRESULT make_context(HANDLE &context);

private:
    KernelDevice                      &kernel_;
    KeyedLog                          &lines_;
    PollClock                         &clock_;
    /* Opened at the first allocation, once the kernel device is bound. */
    ObjectStorage<d3d11::PagingQueue> paging_;
};

}  // namespace av::d3d

#endif  // AV_UM_PRESENTED_ALLOCATIONS_HPP
