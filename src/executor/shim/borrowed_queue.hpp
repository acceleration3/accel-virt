/* A queue an API's runtime owns and lends through its interop, which the
 * executor submits its own work to between the runtime's submissions: DXVK's
 * Direct3D 11 (DxvkD3d11Queue), a DXVK Direct3D 9 device (DxvkD3d9Queue), and
 * vkd3d-proton's Direct3D 12. Each runtime records on a thread of its own and
 * submits from another; flush() gets what it recorded onto the queue, and
 * lock() holds its submissions off while the executor's go in (QueueWork). */
#ifndef AV_BORROWED_QUEUE_HPP
#define AV_BORROWED_QUEUE_HPP

#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace av {

class BorrowedQueue {
public:
    BorrowedQueue(const BorrowedQueue &) = delete;
    BorrowedQueue &operator=(const BorrowedQueue &) = delete;

    virtual VkQueue  queue() const = 0;
    virtual uint32_t family() const = 0;
    /* Everything the runtime has recorded reaches the queue before anything
     * submitted after this returns. */
    virtual void     flush() = 0;
    virtual void     lock() = 0;
    virtual void     unlock() = 0;

protected:
    BorrowedQueue() = default;
    /* Not virtual: nothing is destroyed through this pointer (CLAUDE.md,
     * Adaptations). */
    ~BorrowedQueue() = default;
};

}  // namespace av

#endif  // AV_BORROWED_QUEUE_HPP
