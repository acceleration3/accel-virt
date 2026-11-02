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
    virtual void     flush() = 0;
    virtual void     lock() = 0;
    virtual void     unlock() = 0;

protected:
    BorrowedQueue() = default;
    ~BorrowedQueue() = default;
};

}

#endif
