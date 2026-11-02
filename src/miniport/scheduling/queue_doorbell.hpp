#ifndef AV_QUEUE_DOORBELL_HPP
#define AV_QUEUE_DOORBELL_HPP

#include "miniport/ddi/dxgkarg_doorbell_records.hpp"
#include "core/platform/platform.hpp"

#include <stdint.h>

namespace av {

class HwQueue;

class QueueDoorbell {
public:
    QueueDoorbell(Platform &platform, HwQueue &queue, uint32_t page);
    QueueDoorbell(const QueueDoorbell &) = delete;
    QueueDoorbell &operator=(const QueueDoorbell &) = delete;

    HwQueue &queue() const;
    uint32_t page() const;
    void     connect();
    void     disconnect();
    bool     connected() const;
    DoorbellStatus status() const;

private:
    Platform &platform_;
    HwQueue  *queue_;
    uint32_t  page_;
    bool      connected_;
};

}

#endif
