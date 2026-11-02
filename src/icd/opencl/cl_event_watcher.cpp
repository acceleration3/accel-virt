#include "icd/opencl/cl_event_watcher.hpp"

#include "core/platform/thread_lock.hpp"
#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "core/log/driver_log.hpp"
#include "icd/opencl/cl_object.hpp"
#include "proto/generated/opencl/cl_dispatch_client.h"

namespace av::clicd {

ClEventWatcher::ClEventWatcher(BlockSource &memory, icd::ServiceThread &thread, DriverLog &log, ThreadLock &lock,
                               ClEventOwner &owner)
    : thread_(thread), log_(log), lock_(lock), owner_(owner), rows_(memory, Pool::paged), queues_(memory, Pool::paged),
      held_(0u), delivered_(0u), next_(0u)
{
    AV_SCOPE("ClEventWatcher::ClEventWatcher");
}

ClEventWatcher::~ClEventWatcher()
{
    AV_SCOPE("ClEventWatcher::~ClEventWatcher");
    thread_.stop();
}

cl_int ClEventWatcher::watch(ClObject *event, uint64_t wire, ClientTransport *wire_of_context,
                             cl_int status_wanted, ClWatchedEvent::Notify notify, void *user_data, bool driven)
{
    AV_SCOPE("ClEventWatcher::watch");
    ClWatchedEvent registration;
    if (!registration.take(event, wire, wire_of_context, status_wanted, notify, user_data)) return CL_INVALID_VALUE;
    uint32_t slot = 0u;
    {
        const ThreadLockHold locked(lock_);
        while (slot < rows_.size() && rows_.at(slot).registration_.held()) ++slot;
        const Row row{registration, driven, false};
        if (slot < rows_.size()) rows_.at(slot) = row;
        else if (rows_.append(row) != Status::ok) return CL_OUT_OF_HOST_MEMORY;
        ++held_;
    }
    const Status started = thread_.start(*this);
    if (started == Status::invalid_state) {
        thread_.wake();
    } else if (started != Status::ok) {
        {
            const ThreadLockHold locked(lock_);
            rows_.at(slot).registration_.give_up();
            --held_;
        }
        log_.write("no thread to watch events on");
        return CL_OUT_OF_RESOURCES;
    }
    return CL_SUCCESS;
}

uint64_t ClEventWatcher::wake_handle() const
{
    AV_SCOPE("ClEventWatcher::wake_handle");
    return thread_.wake_handle();
}

bool ClEventWatcher::attend()
{
    AV_SCOPE("ClEventWatcher::attend");
    const Status started = thread_.start(*this);
    return started == Status::ok || started == Status::invalid_state;
}

uint64_t ClEventWatcher::woken() const
{
    AV_SCOPE("ClEventWatcher::woken");
    return thread_.woken();
}

void ClEventWatcher::await_woken(uint64_t seen, uint64_t timeout)
{
    AV_SCOPE("ClEventWatcher::await_woken");
    thread_.await_woken(seen, timeout);
}

bool ClEventWatcher::completed(const ClWatchedEvent &registration, cl_int &status) const
{
    AV_SCOPE("ClEventWatcher::completed");
    cl_int      execution = CL_QUEUED;
    size_t      answered = 0u;
    const cl_int asked = av::clproto::av_clGetEventInfo(
        *registration.transport(), registration.wire(), CL_EVENT_COMMAND_EXECUTION_STATUS,
        sizeof execution, &execution, &answered);
    if (asked != CL_SUCCESS) {
        status = asked;
        return true;
    }
    status = execution;
    return registration.reached_by(execution);
}

bool ClEventWatcher::turn()
{
    AV_SCOPE("ClEventWatcher::turn");
    bool     driven = false;
    uint32_t rows = 0u;
    {
        const ThreadLockHold locked(lock_);
        if (held_ != 0u) {
            driven = mark_oldest();
            rows = rows_.size();
        }
    }
    if (rows == 0u) {
        thread_.rest();
        return true;
    }
    uint32_t asks = 0u;
    bool     ran = false;
    uint32_t step = 0u;
    for (; step < rows && asks < asks_per_turn; ++step) {
        const uint32_t slot = (next_ + step) % rows;
        Row asked{};
        {
            const ThreadLockHold locked(lock_);
            asked = rows_.at(slot);
        }
        if (!asked.oldest_ || !asked.registration_.held()) continue;
        cl_int status = CL_QUEUED;
        if (!completed(asked.registration_, status)) {
            ++asks;
            continue;
        }
        if (deliver(slot, asked.registration_, status)) ran = true;
    }
    next_ = rows == 0u ? 0u : (next_ + step) % rows;
    if (!ran) thread_.pause(driven && step == rows);
    return true;
}

bool ClEventWatcher::mark_oldest()
{
    AV_SCOPE("ClEventWatcher::mark_oldest");
    queues_.truncate(0u);
    bool driven = true;
    for (uint32_t slot = 0u; slot < rows_.size(); ++slot) {
        Row &row = rows_.at(slot);
        row.oldest_ = false;
        if (!row.registration_.held()) continue;
        driven = driven && row.driven_;
        const ClObject *const event = row.registration_.event();
        const ClObject *const queue = event->parent();
        if (queue == nullptr || queue->kind() != ClObject::Kind::command_queue || event->ordinal() == 0u) {
            row.oldest_ = true;
            continue;
        }
        uint32_t known = 0u;
        while (known < queues_.size() && queues_.at(known).queue_ != queue) ++known;
        if (known == queues_.size()) {
            if (queues_.append(QueueOldest{queue, slot}) != Status::ok) row.oldest_ = true;
        } else if (event->ordinal() < rows_.at(queues_.at(known).row_).registration_.event()->ordinal()) {
            queues_.at(known).row_ = slot;
        }
    }
    for (const QueueOldest &oldest : queues_) rows_.at(oldest.row_).oldest_ = true;
    return driven;
}

bool ClEventWatcher::deliver(uint32_t slot, const ClWatchedEvent &asked, cl_int status)
{
    AV_SCOPE("ClEventWatcher::deliver");
    bool still = false;
    {
        const ThreadLockHold locked(lock_);
        ClWatchedEvent      &row = rows_.at(slot).registration_;
        still = row.held() && row.event() == asked.event() && row.wire() == asked.wire();
        if (still) {
            row.give_up();
            --held_;
            ++delivered_;
        }
    }
    if (!still) return false;
    owner_.deliver_watched(*asked.event());
    asked.run(status);
    owner_.release_watched(*asked.event());
    return true;
}

void ClEventWatcher::forget_context(const ClientTransport *wire_of_context)
{
    AV_SCOPE("ClEventWatcher::forget_context");
    for (Row &row : rows_) {
        if (!row.registration_.held() || row.registration_.transport() != wire_of_context) continue;
        row.registration_.give_up();
        --held_;
    }
}

uint32_t ClEventWatcher::registered() const
{
    AV_SCOPE("ClEventWatcher::registered");
    return held_;
}

uint64_t ClEventWatcher::delivered() const
{
    AV_SCOPE("ClEventWatcher::delivered");
    return delivered_;
}

void ClEventWatcher::report(StateWriter &writer) const
{
    AV_SCOPE("ClEventWatcher::report");
    writer.open("events", 0u);
    writer.number("registered", held_);
    writer.number("delivered", delivered_);
    writer.flag("watching", thread_.running());
    writer.close();
}

bool ClEventWatcher::invariants_hold() const
{
    AV_SCOPE("ClEventWatcher::invariants_hold");
    uint32_t held = 0u;
    for (const Row &row : rows_) {
        if (!row.registration_.invariants_hold()) return false;
        if (row.registration_.held()) ++held;
    }
    return held == held_ && rows_.invariants_hold() && queues_.invariants_hold();
}

}
