#include "executor/opencl/cl_event_signals.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ClEventSignals::ClEventSignals(BlockSource &blocks)
    : slots_(blocks, Pool::paged), cursor_(0u), generation_(1u), taken_(0u), signalled_(0u), stale_(0u), wake_()
{
    AV_SCOPE("ClEventSignals::ClEventSignals");
}

void ClEventSignals::bind(void (*signal)(void *context), void *context)
{
    AV_SCOPE("ClEventSignals::bind");
    wake_.bind(signal, context);
}

bool ClEventSignals::bound() const
{
    AV_SCOPE("ClEventSignals::bound");
    return wake_.bound();
}

ClEventRegistration *ClEventSignals::take()
{
    AV_SCOPE("ClEventSignals::take");
    const uint32_t made = slots_.slots();
    uint32_t       slot = KeyedSlots<uint32_t, ClEventRegistration>::no_slot;
    for (uint32_t looked = 0u; looked < made && slot == KeyedSlots<uint32_t, ClEventRegistration>::no_slot; looked++)
        if (!slots_.at((cursor_ + looked) % made)->armed()) slot = (cursor_ + looked) % made;
    if (slot == KeyedSlots<uint32_t, ClEventRegistration>::no_slot) {
        if (slots_.insert(made, ClEventRegistration(), slot) != Status::ok) return nullptr;
        slots_.at(slot)->bind(*this);
    }
    cursor_ = slot + 1u;
    ClEventRegistration *const taken = slots_.at(slot);
    taken->arm(generation_);
    __atomic_fetch_add(&taken_, 1u, __ATOMIC_ACQ_REL);
    return taken;
}

void ClEventSignals::give_back(ClEventRegistration *registration)
{
    AV_SCOPE("ClEventSignals::give_back");
    if (registration == nullptr || !registration->armed()) return;
    registration->disarm();
    __atomic_fetch_sub(&taken_, 1u, __ATOMIC_ACQ_REL);
}

void ClEventSignals::answered()
{
    AV_SCOPE("ClEventSignals::answered");
    __atomic_fetch_sub(&taken_, 1u, __ATOMIC_ACQ_REL);
}

void ClEventSignals::shut_down()
{
    AV_SCOPE("ClEventSignals::shut_down");
    /* Every registration made so far goes stale at once. The slots stay where
     * they are: the host may still hold pointers to them. */
    ++generation_;
    slots_.for_each([](uint32_t, ClEventRegistration &registration) { registration.disarm(); });
    __atomic_store_n(&taken_, 0u, __ATOMIC_RELEASE);
    wake_.clear();
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
    if (wake_.raise()) __atomic_fetch_add(&signalled_, uint64_t{1}, __ATOMIC_RELAXED);
}

uint32_t ClEventSignals::outstanding() const
{
    AV_SCOPE("ClEventSignals::outstanding");
    return __atomic_load_n(&taken_, __ATOMIC_ACQUIRE);
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
    if (generation_ == 0u) return false;
    uint32_t taken = 0u;
    bool     holds = true;
    slots_.for_each([this, &taken, &holds](uint32_t, const ClEventRegistration &registration) {
        holds = holds && registration.invariants_hold();
        /* One that is not armed carries no epoch - its own invariant is what
         * says so - so there is nothing more to ask of it here. */
        if (!registration.armed()) return;
        holds = holds && registration.generation() == generation_;
        ++taken;
    });
    return holds && taken == taken_;
}

}  // namespace av
