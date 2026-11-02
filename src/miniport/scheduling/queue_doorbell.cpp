#include "miniport/scheduling/queue_doorbell.hpp"

#include "miniport/scheduling/hw_queue.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

QueueDoorbell::QueueDoorbell(Platform &platform, HwQueue &queue, uint32_t page)
    : platform_(platform), queue_(&queue), page_(page), connected_(false)
{
    AV_SCOPE("QueueDoorbell::QueueDoorbell");
}

HwQueue &QueueDoorbell::queue() const
{
    AV_SCOPE("QueueDoorbell::queue");
    return *queue_;
}

uint32_t QueueDoorbell::page() const
{
    AV_SCOPE("QueueDoorbell::page");
    return page_;
}

void QueueDoorbell::connect()
{
    AV_SCOPE("QueueDoorbell::connect");
    platform_.require_irql_at_most(Irql::apc, "QueueDoorbell");
    connected_ = true;
}

void QueueDoorbell::disconnect()
{
    AV_SCOPE("QueueDoorbell::disconnect");
    platform_.require_irql_at_most(Irql::apc, "QueueDoorbell");
    connected_ = false;
}

bool QueueDoorbell::connected() const
{
    AV_SCOPE("QueueDoorbell::connected");
    return connected_;
}

DoorbellStatus QueueDoorbell::status() const
{
    AV_SCOPE("QueueDoorbell::status");
    return connected_ ? DoorbellStatus::connected : DoorbellStatus::disconnected_retry;
}

}
