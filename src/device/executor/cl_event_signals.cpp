#include "device/executor/cl_event_signals.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ClEventSignals::ClEventSignals()
    : slots_(), generation_(1u), taken_(0u), signalled_(0u), stale_(0u),
      signal_(nullptr), context_(nullptr)
{
    AV_SCOPE("ClEventSignals::ClEventSignals");
    for (uint32_t index = 0u; index < max_registrations; ++index) slots_[index].bind(*this);
}

void ClEventSignals::bind(void (*signal)(void *context), void *context)
{
    AV_SCOPE("ClEventSignals::bind");
    signal_ = signal;
    context_ = context;
}

bool ClEventSignals::bound() const
{
    AV_SCOPE("ClEventSignals::bound");
    return signal_ != nullptr;
}

ClEventRegistration *ClEventSignals::take()
{
    AV_SCOPE("ClEventSignals::take");
    for (uint32_t index = 0u; index < max_registrations; ++index) {
        if (slots_[index].armed()) continue;
        slots_[index].arm(generation_);
        ++taken_;
        return &slots_[index];
    }
    return nullptr;
}

void ClEventSignals::give_back(ClEventRegistration *registration)
{
    AV_SCOPE("ClEventSignals::give_back");
    if (registration == nullptr || !registration->armed()) return;
    registration->disarm();
    --taken_;
}

void ClEventSignals::shut_down()
{
    AV_SCOPE("ClEventSignals::shut_down");
    /* Every registration made so far goes stale at once. The slots stay where
     * they are: the host may still hold pointers to them. */
    ++generation_;
    for (uint32_t index = 0u; index < max_registrations; ++index) slots_[index].disarm();
    taken_ = 0u;
    signal_ = nullptr;
    context_ = nullptr;
}

/* Marked like anything else, though this runs on the host driver's own thread:
 * the trace table is keyed by thread and a scope on an unbound one does
 * nothing, which is the state a callback arriving after the library was torn
 * down finds. */
void ClEventSignals::signal_from_host(uint64_t generation)
{
    AV_SCOPE("ClEventSignals::signal_from_host");
    if (generation == 0u || generation != __atomic_load_n(&generation_, __ATOMIC_ACQUIRE)) {
        __atomic_fetch_add(&stale_, uint64_t{1}, __ATOMIC_RELAXED);
        return;
    }
    void (*signal)(void *) = __atomic_load_n(&signal_, __ATOMIC_ACQUIRE);
    void *context = __atomic_load_n(&context_, __ATOMIC_ACQUIRE);
    if (signal == nullptr) return;
    __atomic_fetch_add(&signalled_, uint64_t{1}, __ATOMIC_RELAXED);
    signal(context);
}

uint32_t ClEventSignals::outstanding() const
{
    AV_SCOPE("ClEventSignals::outstanding");
    return taken_;
}

uint64_t ClEventSignals::signalled() const
{
    AV_SCOPE("ClEventSignals::signalled");
    return __atomic_load_n(&signalled_, __ATOMIC_RELAXED);
}

uint64_t ClEventSignals::stale() const
{
    AV_SCOPE("ClEventSignals::stale");
    return __atomic_load_n(&stale_, __ATOMIC_RELAXED);
}

bool ClEventSignals::invariants_hold() const
{
    AV_SCOPE("ClEventSignals::invariants_hold");
    if (taken_ > max_registrations) return false;
    if (generation_ == 0u) return false;
    uint32_t taken = 0u;
    for (uint32_t index = 0u; index < max_registrations; ++index) {
        if (!slots_[index].invariants_hold()) return false;
        if (!slots_[index].armed()) {
            if (slots_[index].generation() != 0u) return false;
            continue;
        }
        if (slots_[index].generation() != generation_) return false;
        ++taken;
    }
    return taken == taken_;
}

}  // namespace av
