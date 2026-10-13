/* A Direct3D 12 queue's Vulkan queue, as vkd3d-proton lends it through its
 * interop (ID3D12DXVKInteropDevice): its handle and family, and the lock vkd3d
 * takes to submit to it. vkd3d submits what a list recorded at
 * ExecuteCommandLists, so there is nothing to flush. */
#ifndef AV_VKD3D_QUEUE_HPP
#define AV_VKD3D_QUEUE_HPP

#include "executor/shim/borrowed_queue.hpp"

struct ID3D12CommandQueue;
struct ID3D12DXVKInteropDevice;

namespace av {

class Vkd3dQueue final : public BorrowedQueue {
public:
    Vkd3dQueue();
    /* `queue`'s Vulkan queue through `interop`; neither is held. Null for
     * none. */
    void     borrow(ID3D12DXVKInteropDevice *interop, ID3D12CommandQueue *queue);
    VkQueue  queue() const override;
    uint32_t family() const override;
    void     flush() override;
    void     lock() override;
    void     unlock() override;

private:
    ID3D12DXVKInteropDevice *interop_;
    ID3D12CommandQueue      *d3d12_;
    VkQueue                  queue_;
    uint32_t                 family_;
};

}  // namespace av

#endif  // AV_VKD3D_QUEUE_HPP
