#include "icd/opencl/cl_event_watcher.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/common/log_sink.hpp"
#include "icd/opencl/cl_object.hpp"
#include "proto/generated/opencl/cl_dispatch_client.h"

namespace av::clicd {

ClEventWatcher::ClEventWatcher(icd::ServiceThread &thread, icd::LogSink &log)
    : thread_(thread), log_(log), registrations_(), held_(0u), delivered_(0u)
{
    AV_SCOPE("ClEventWatcher::ClEventWatcher");
}

ClEventWatcher::~ClEventWatcher()
{
    AV_SCOPE("ClEventWatcher::~ClEventWatcher");
    /* The thread holds a pointer to this loop, so it stops before the loop
     * goes. What makes its turn return is the transport it is waiting on
     * being torn down with the context. */
    thread_.stop();
}

cl_int ClEventWatcher::watch(ClObject *event, uint64_t wire, ClientTransport *wire_of_context,
                             cl_int status_wanted, ClWatchedEvent::Notify notify, void *user_data)
{
    AV_SCOPE("ClEventWatcher::watch");
    uint32_t slot = 0u;
    while (slot < max_registrations && registrations_[slot].held()) ++slot;
    if (slot == max_registrations) return CL_OUT_OF_RESOURCES;
    if (!registrations_[slot].take(event, wire, wire_of_context, status_wanted, notify, user_data)) {
        return CL_INVALID_VALUE;
    }
    ++held_;
    /* The thread runs while anything is registered, and not before: a process
     * that never asks for a callback never starts one. */
    if (!thread_.running()) {
        const Status started = thread_.start(*this);
        if (started != Status::ok) {
            registrations_[slot].give_up();
            --held_;
            log_.write("rvkcl: no thread to watch events on");
            return CL_OUT_OF_RESOURCES;
        }
    }
    return CL_SUCCESS;
}

bool ClEventWatcher::completed(const ClWatchedEvent &registration, cl_int &status) const
{
    AV_SCOPE("ClEventWatcher::completed");
    cl_int      execution = CL_QUEUED;
    size_t      answered = 0u;
    const cl_int asked = av::clproto::rvk_clGetEventInfo(
        *registration.transport(), registration.wire(), CL_EVENT_COMMAND_EXECUTION_STATUS,
        sizeof execution, &execution, &answered);
    if (asked != CL_SUCCESS) {
        /* An event this can no longer ask about is one whose callback will
         * never be right to run; it is dropped rather than left for ever. */
        status = asked;
        return true;
    }
    status = execution;
    return registration.reached_by(execution);
}

bool ClEventWatcher::turn()
{
    AV_SCOPE("ClEventWatcher::turn");
    for (uint32_t slot = 0u; slot < max_registrations; ++slot) {
        if (!registrations_[slot].held()) continue;
        cl_int status = CL_QUEUED;
        if (!completed(registrations_[slot], status)) continue;
        /* Copied and given up before it runs: a callback that releases its
         * own event, or the context it belongs to, is releasing something
         * this is no longer holding. */
        const ClWatchedEvent ran = registrations_[slot];
        registrations_[slot].give_up();
        --held_;
        ++delivered_;
        /* No ICD lock is held here, and the callback may call back into the
         * ICD - which is the whole reason it is not run under one. */
        ran.run(status);
    }
    return held_ != 0u;
}

void ClEventWatcher::forget_context(const ClientTransport *wire_of_context)
{
    AV_SCOPE("ClEventWatcher::forget_context");
    for (uint32_t slot = 0u; slot < max_registrations; ++slot) {
        if (!registrations_[slot].held() || registrations_[slot].transport() != wire_of_context) {
            continue;
        }
        registrations_[slot].give_up();
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
    if (held_ > max_registrations) return false;
    uint32_t held = 0u;
    for (uint32_t slot = 0u; slot < max_registrations; ++slot) {
        if (!registrations_[slot].invariants_hold()) return false;
        if (registrations_[slot].held()) ++held;
    }
    return held == held_;
}

}  // namespace av::clicd
