/* One host OpenCL event a guest asked to be told about.
 *
 * The host's OpenCL is handed a pointer to this and calls it back on a thread
 * of its own, so it is an object rather than a field: what the callback needs
 * is here, and nothing else is reachable from it.
 *
 * It outlives what made it. OpenCL cannot be told to stop calling a callback,
 * so one can arrive after the session shut down, after the executor was
 * destroyed, and after the context is gone - which is what the generation is
 * for. A registration carries the value its owner was at when it was made, and
 * one whose generation no longer matches does nothing at all. */
#ifndef AV_CL_EVENT_REGISTRATION_HPP
#define AV_CL_EVENT_REGISTRATION_HPP

#include <stdint.h>

namespace av {

class ClEventSignals;

class ClEventRegistration {
public:
    ClEventRegistration();

    /* Which collection this belongs to, once and for its life. */
    void bind(ClEventSignals &owner);
    /* Taken for a registration at `generation`, or given back. */
    void arm(uint64_t generation);
    void disarm();
    bool armed() const;
    uint64_t generation() const;

    /* Called from the host driver's thread when the event completes: signals
     * the owner when this registration is still the one that was made. */
    void completed();

    bool invariants_hold() const;

private:
    ClEventSignals *owner_;
    /* Read by the host's thread, written by the worker. */
    uint64_t        generation_;
    bool            armed_;
};

}  // namespace av

#endif  // AV_CL_EVENT_REGISTRATION_HPP
