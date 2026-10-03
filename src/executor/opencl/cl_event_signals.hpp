/* The host OpenCL events one session's guest asked to be told about.
 *
 * OpenCL says an event completed by calling a function the caller registered,
 * on whatever thread the host driver please - never the context's worker. So
 * this is the one place in the executor library that another thread reaches,
 * and it is built for exactly that: the foreign thread does two things, checks
 * a number and calls the device's signal, and everything else happens on the
 * worker afterwards. Nothing about *which* event completed crosses; the guest
 * is woken and asks (clGetEventInfo), which keeps this side stateless enough
 * to be safe from a thread it does not own.
 *
 * **A callback can arrive after the session is gone.** OpenCL has no way to
 * unregister one, so a registration outlives `shut_down`, the executor's
 * destroy, and the context itself. That is what the generation is for: every
 * registration carries the value the session had when it was made, and a
 * callback whose generation is stale does nothing at all. The storage it
 * writes to is a member of the backend, which lives in ExecutorRoot's array
 * for as long as the library is loaded, so the pointer the host holds stays
 * valid even when what it named is finished with. */
#ifndef AV_CL_EVENT_SIGNALS_HPP
#define AV_CL_EVENT_SIGNALS_HPP

#include "executor/opencl/cl_event_registration.hpp"
#include "executor/wake_signal.hpp"

#include <stdint.h>

namespace av {

struct ClEventSignalsProbe;

class ClEventSignals {
public:
    /* Events one context may be waiting on at once. A guest that wants more
     * is told so at the registration, which is a call it can see fail. */
    static constexpr uint32_t max_registrations = 128u;

    ClEventSignals();
    ClEventSignals(const ClEventSignals &) = delete;
    ClEventSignals &operator=(const ClEventSignals &) = delete;

    /* What the device gave the library to call (executor ABI version 5), or
     * nothing: a version 4 device signals nothing and the guest waits for its
     * events at a synchronising call instead. */
    void bind(void (*signal)(void *context), void *context);
    bool bound() const;

    /* Takes a registration, or null when full. It is what the host callback
     * is given, and it carries the generation this session is at. Worker
     * only. */
    ClEventRegistration *take();
    /* Gives one back: the registration could not be made after all. Worker
     * only, and only for what `take` just answered. */
    void                 give_back(ClEventRegistration *registration);

    /* A registration's host callback came, so its slot is free again. Any
     * thread: it is called from the host driver's. */
    void answered();

    /* Every registration made so far goes stale, and the device's signal is
     * forgotten. Worker only. A callback still to come finds its generation
     * changed and does nothing. */
    void shut_down();

    /* How many registrations are outstanding, and how many callbacks have
     * been answered - both for the tests and for a state dump. */
    uint32_t outstanding() const;
    uint64_t signalled() const;
    /* Callbacks that arrived after their registration went stale. */
    uint64_t stale() const;

    bool invariants_hold() const;

    /* Reached from the host driver's thread, through a registration. Public
     * because that is where it is called from, and because nothing else may:
     * it does the two things a foreign thread is allowed to do here, compare
     * a number and call the device. */
    void signal_from_host(uint64_t generation);

private:
    friend struct ClEventSignalsProbe;

    ClEventRegistration slots_[max_registrations];
    uint64_t generation_;
    uint32_t taken_;
    /* Written by the worker, read by both; the counters are for reporting and
     * are allowed to be a little behind. */
    uint64_t signalled_;
    uint64_t   stale_;
    WakeSignal wake_;
};

}  // namespace av

#endif  // AV_CL_EVENT_SIGNALS_HPP
