/* DXVK's queues as the executor borrows them (BorrowedQueue): Direct3D 11's,
 * through IDXGIVkInteropDevice1, and a Direct3D 9 device's, through
 * ID3D9VkInteropDevice. Both flush by draining DXVK's command-stream thread,
 * and both lock by DXVK's submission lock, which first waits for DXVK's
 * submission thread to empty (DxvkDevice::lockSubmission) - so work the
 * executor submits under it lands after everything DXVK recorded before. */
#ifndef AV_DXVK_QUEUES_HPP
#define AV_DXVK_QUEUES_HPP

#include "executor/shim/borrowed_queue.hpp"

struct IDXGIVkInteropDevice1;
struct ID3D9VkInteropDevice;

namespace av {

class DxvkD3d11Queue final : public BorrowedQueue {
public:
    DxvkD3d11Queue();
    /* `interop`'s queue; the interop must outlive this. */
    void     borrow(IDXGIVkInteropDevice1 *interop);
    VkQueue  queue() const override;
    uint32_t family() const override;
    void     flush() override;
    void     lock() override;
    void     unlock() override;

private:
    IDXGIVkInteropDevice1 *interop_;
    VkQueue                queue_;
    uint32_t               family_;
};

class DxvkD3d9Queue final : public BorrowedQueue {
public:
    DxvkD3d9Queue();
    void     borrow(ID3D9VkInteropDevice *interop);
    VkQueue  queue() const override;
    uint32_t family() const override;
    void     flush() override;
    void     lock() override;
    void     unlock() override;

private:
    ID3D9VkInteropDevice *interop_;
    VkQueue               queue_;
    uint32_t              family_;
};

}  // namespace av

#endif  // AV_DXVK_QUEUES_HPP
