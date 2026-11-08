#ifndef AV_GATED_QUEUE_HPP
#define AV_GATED_QUEUE_HPP

#include "executor/shim/borrowed_queue.hpp"

namespace av {

class ContextGate;

class GatedQueue final : public BorrowedQueue {
public:
    GatedQueue(BorrowedQueue &inner, ContextGate &gate);

    VkQueue  queue() const override;
    uint32_t family() const override;
    void     flush() override;
    void     lock() override;
    void     unlock() override;

private:
    BorrowedQueue &inner_;
    ContextGate   &gate_;
};

}

#endif
