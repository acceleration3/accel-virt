#include "core/scheduling/doorbell_table.hpp"

#include "core/pci/mapped_register_window.hpp"
#include "core/scheduling/hw_queue.hpp"
#include "core/platform/mutex_guard.hpp"
#include "core/platform/mutex_try_guard.hpp"
#include "core/rvk/ring_submitter.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av {

DoorbellTable::DoorbellTable(Platform &platform, MappedRegisterWindow &doorbells)
    : platform_(platform), doorbells_(doorbells), lock_(platform, LockRank::doorbell_table), held_(platform)
{
    AV_SCOPE("DoorbellTable::DoorbellTable");
}

Status DoorbellTable::create(HwQueue &queue, uint32_t page, void *&handle)
{
    AV_SCOPE("DoorbellTable::create");
    platform_.require_irql_at_most(Irql::passive, "DoorbellTable");
    handle = nullptr;
    /* A page past the mapping is a page no store could reach. */
    if ((uint64_t{page} + 1u) * RingSubmitter::doorbell_page_bytes > doorbells_.length())
        return Status::invalid_parameter;
    MutexGuard lock(lock_);
    if (held_.live() >= max_doorbells) return Status::no_space;
    QueueDoorbell *made = nullptr;
    const Status created = held_.create(made, platform_, queue, page);
    if (created != Status::ok) return created;
    handle = made;
    return Status::ok;
}

Status DoorbellTable::connect(void *handle, void *&address, DoorbellStatus &status)
{
    AV_SCOPE("DoorbellTable::connect");
    platform_.require_irql_at_most(Irql::passive, "DoorbellTable");
    address = nullptr;
    MutexGuard lock(lock_);
    QueueDoorbell *const doorbell = find(handle);
    if (doorbell == nullptr) return Status::invalid_parameter;
    doorbell->connect();
    address = doorbells_.address_at(uint64_t{doorbell->page()} * RingSubmitter::doorbell_page_bytes);
    status = doorbell->status();
    return Status::ok;
}

Status DoorbellTable::disconnect(void *handle)
{
    AV_SCOPE("DoorbellTable::disconnect");
    platform_.require_irql_at_most(Irql::passive, "DoorbellTable");
    MutexGuard lock(lock_);
    QueueDoorbell *const doorbell = find(handle);
    if (doorbell == nullptr) return Status::invalid_parameter;
    /* Nothing is remapped: a store to this device's doorbell wakes a worker
     * that reads a ring tail, and a tail that did not move consumes nothing
     * (research.md §10). */
    doorbell->disconnect();
    return Status::ok;
}

Status DoorbellTable::destroy(void *handle)
{
    AV_SCOPE("DoorbellTable::destroy");
    platform_.require_irql_at_most(Irql::passive, "DoorbellTable");
    MutexGuard lock(lock_);
    QueueDoorbell *const doorbell = find(handle);
    if (doorbell == nullptr) return Status::invalid_parameter;
    return held_.destroy(doorbell);
}

void DoorbellTable::forget_queue(const HwQueue *queue)
{
    AV_SCOPE("DoorbellTable::forget_queue");
    platform_.require_irql_at_most(Irql::passive, "DoorbellTable");
    if (queue == nullptr) return;
    MutexGuard lock(lock_);
    held_.destroy_if([queue](const QueueDoorbell &doorbell) { return &doorbell.queue() == queue; });
}

DoorbellStatus DoorbellTable::status_of(void *handle)
{
    AV_SCOPE("DoorbellTable::status_of");
    platform_.require_irql_at_most(Irql::dispatch, "DoorbellTable");
    MutexGuard      lock(lock_);
    QueueDoorbell *const doorbell = find(handle);
    /* A handle this table does not know is not disconnected for a moment: it
     * is gone, and a process holding it must make another. */
    return doorbell != nullptr ? doorbell->status() : DoorbellStatus::disconnected_abort;
}

bool DoorbellTable::serves(const HwQueue *queue)
{
    AV_SCOPE("DoorbellTable::serves");
    platform_.require_irql_at_most(Irql::dispatch, "DoorbellTable");
    MutexGuard lock(lock_);
    bool       found = false;
    held_.for_each([queue, &found](const QueueDoorbell &doorbell) { found = found || &doorbell.queue() == queue; });
    return found;
}

uint32_t DoorbellTable::live() const
{
    AV_SCOPE("DoorbellTable::live");
    return held_.live();
}

void DoorbellTable::report(StateWriter &writer)
{
    AV_SCOPE("DoorbellTable::report");
    platform_.require_irql_at_most(Irql::apc, "DoorbellTable::report");
    const MutexTryGuard guard(lock_);
    writer.open("doorbell_table", 0u);
    writer.flag("busy", !guard.held());
    if (guard.held()) {
        writer.number("live", held_.live());
        uint64_t index = 0u;
        held_.for_each([&](const QueueDoorbell &doorbell) {
            writer.open("doorbell", index++);
            writer.address("handle", &doorbell);
            writer.address("queue", &doorbell.queue());
            writer.number("page", doorbell.page());
            writer.flag("connected", doorbell.connected());
            writer.close();
        });
    }
    writer.close();
}

QueueDoorbell *DoorbellTable::find(void *handle)
{
    AV_SCOPE("DoorbellTable::find");
    return handle != nullptr ? held_.find(handle) : nullptr;
}

bool DoorbellTable::invariants_hold() const
{
    AV_SCOPE("DoorbellTable::invariants_hold");
    const uint64_t length = doorbells_.length();
    return held_.all_of([length](const QueueDoorbell &doorbell) {
        return (uint64_t{doorbell.page()} + 1u) * RingSubmitter::doorbell_page_bytes <= length;
    });
}

}  // namespace av
