#ifndef AV_UM_KERNEL_DEVICE_HPP
#define AV_UM_KERNEL_DEVICE_HPP

#include "um/d3d/d3d11_ddi.hpp"

namespace av::d3d {

class KernelHeap;
class KernelSurface;

class KernelDevice {
public:
    KernelDevice();
    KernelDevice(const KernelDevice &) = delete;
    KernelDevice &operator=(const KernelDevice &) = delete;

    void    bind(const D3DDDI_DEVICECALLBACKS *callbacks, HANDLE runtime);
    bool    bound() const;
    const D3DDDI_DEVICECALLBACKS &callbacks() const;
    HANDLE                        runtime() const;

    HRESULT make_context(HANDLE &context);
    void    destroy_context(HANDLE context);
    HRESULT allocate(KernelSurface &surface, HANDLE resource, D3DKMT_HANDLE &allocation);
    HRESULT allocate(KernelHeap &heap, HANDLE resource, D3DKMT_HANDLE &allocation);
    [[nodiscard]] HRESULT deallocate(D3DKMT_HANDLE allocation, HANDLE resource);
    void   *lock_to_write(D3DKMT_HANDLE allocation);
    void   *lock_to_read(D3DKMT_HANDLE allocation);
    void    unlock(D3DKMT_HANDLE allocation);
    HRESULT acquire(HANDLE token, HANDLE context);
    HRESULT release(HANDLE token, HANDLE context);
    HRESULT present(D3DDDICB_PRESENT &present);
    HRESULT set_display_mode(D3DDDICB_SETDISPLAYMODE &mode);

private:
    [[nodiscard]] void *lock(D3DDDICB_LOCK &asked);

    const D3DDDI_DEVICECALLBACKS *callbacks_;
    HANDLE                        runtime_;
};

}

#endif
