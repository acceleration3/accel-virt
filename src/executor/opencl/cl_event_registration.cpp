#include "executor/opencl/cl_event_registration.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/opencl/cl_event_signals.hpp"

namespace av {

ClEventRegistration::ClEventRegistration() : owner_(nullptr), generation_(0u), armed_(false)
{
    AV_SCOPE("ClEventRegistration::ClEventRegistration");
}

void ClEventRegistration::bind(ClEventSignals &owner)
{
    AV_SCOPE("ClEventRegistration::bind");
    owner_ = &owner;
}

void ClEventRegistration::arm(uint64_t generation)
{
    AV_SCOPE("ClEventRegistration::arm");
    __atomic_store_n(&armed_, true, __ATOMIC_RELAXED);
    /* Published last: the host may be given this the moment the caller
     * returns, and the generation it reads must be this one. */
    __atomic_store_n(&generation_, generation, __ATOMIC_RELEASE);
}

void ClEventRegistration::disarm()
{
    AV_SCOPE("ClEventRegistration::disarm");
    /* Cleared before the slot is free, so a callback that still names it
     * finds a generation of zero and does nothing. */
    __atomic_store_n(&generation_, uint64_t{0}, __ATOMIC_RELEASE);
    __atomic_store_n(&armed_, false, __ATOMIC_RELEASE);
}

bool ClEventRegistration::armed() const
{
    AV_SCOPE("ClEventRegistration::armed");
    return __atomic_load_n(&armed_, __ATOMIC_ACQUIRE);
}

uint64_t ClEventRegistration::generation() const
{
    AV_SCOPE("ClEventRegistration::generation");
    return __atomic_load_n(&generation_, __ATOMIC_ACQUIRE);
}

/* Marked like anything else, though this runs on the host driver's own thread:
 * the trace table is keyed by thread and a scope on an unbound one does
 * nothing, which is the state a callback arriving after the library was torn
 * down finds. */
void ClEventRegistration::completed()
{
    AV_SCOPE("ClEventRegistration::completed");
    if (owner_ == nullptr) return;
    /* Claimed once: a slot disarmed by a shut-down already reads zero, and
     * then neither the signal nor the slot is this callback's - the owner
     * counts it stale and that is all. */
    const uint64_t generation = __atomic_exchange_n(&generation_, uint64_t{0}, __ATOMIC_ACQ_REL);
    owner_->signal_from_host(generation);
    if (generation == 0u) return;
    owner_->answered();
    /* Free last: the worker may take the slot again the moment this reads
     * false. */
    __atomic_store_n(&armed_, false, __ATOMIC_RELEASE);
}

bool ClEventRegistration::invariants_hold() const
{
    AV_SCOPE("ClEventRegistration::invariants_hold");
    if (owner_ == nullptr) return generation_ == 0u && !armed_;
    return armed_ || generation_ == 0u;
}

}  // namespace av
