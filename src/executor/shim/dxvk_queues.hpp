#ifndef AV_DXVK_QUEUES_HPP
#define AV_DXVK_QUEUES_HPP

#include "executor/shim/borrowed_queue.hpp"

struct IDXGIVkInteropDevice1;
struct ID3D9VkInteropDevice;

namespace av {

class DxvkD3d11Queue final : public BorrowedQueue {
public:
    DxvkD3d11Queue();
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

}

#endif
