/* What a Direct3D device asks of the kernel on its own behalf, through the
 * runtime's callbacks (D3DDDI_DEVICECALLBACKS) with the runtime's device
 * handle: its virtual context, the allocations it presents from, and a CPU
 * view of one. Direct3D 9's and 11's devices each hold one. */
#ifndef AV_UM_KERNEL_DEVICE_HPP
#define AV_UM_KERNEL_DEVICE_HPP

#include "um/d3d/d3d11_ddi.hpp"

namespace av::d3d {

class KernelSurface;

class KernelDevice {
public:
    KernelDevice();
    KernelDevice(const KernelDevice &) = delete;
    KernelDevice &operator=(const KernelDevice &) = delete;

    /* The callbacks and device handle the runtime gave CreateDevice; the
     * callbacks outlive the call (the device keeps the table they are in). */
    void    bind(const D3DDDI_DEVICECALLBACKS *callbacks, HANDLE runtime);
    bool    bound() const;
    /* What a paging queue and a batch follower are made over: the same pair. */
    const D3DDDI_DEVICECALLBACKS &callbacks() const;
    HANDLE                        runtime() const;

    /* A context on node 0, the one engine the miniport runs. */
    HRESULT make_context(HANDLE &context);
    /* Nothing for a null context. */
    void    destroy_context(HANDLE context);
    /* An allocation of `surface`'s private data, made beforehand - a primary
     * scanned out at its source - belonging to the runtime's `resource`, or
     * to none for null. */
    HRESULT allocate(KernelSurface &surface, HANDLE resource, D3DKMT_HANDLE &allocation);
    void    deallocate(D3DKMT_HANDLE allocation, HANDLE resource);
    /* A CPU view of `allocation` to write, or to read; null when the kernel
     * would not lock it. Each is unlocked once read or written. */
    void   *lock_to_write(D3DKMT_HANDLE allocation);
    void   *lock_to_read(D3DKMT_HANDLE allocation);
    void    unlock(D3DKMT_HANDLE allocation);
    /* The wait a sync token stands for - DWM giving a flip-model buffer back
     * (AcquireResource) - queued on `context`. */
    HRESULT acquire(HANDLE token, HANDLE context);
    /* A present the runtime describes; a display mode a primary is set to. */
    HRESULT present(D3DDDICB_PRESENT &present);
    HRESULT set_display_mode(D3DDDICB_SETDISPLAYMODE &mode);

private:
    [[nodiscard]] void *lock(D3DDDICB_LOCK &asked);

    const D3DDDI_DEVICECALLBACKS *callbacks_;
    HANDLE                        runtime_;
};

}  // namespace av::d3d

#endif  // AV_UM_KERNEL_DEVICE_HPP
