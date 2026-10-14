/* The device's pulse (docs/protocol.md, "Fence page"): a thread that, while
 * any context it carries is at work for the guest, moves the beat word on
 * every one of their fence pages each period.
 *
 * A context's worker writes its page only between commands, and one command
 * can sit in a host call for seconds - a host driver throttling its queue
 * until the GPU catches up - while nothing the worker writes moves. A guest
 * waiting on such a page cannot tell that from a device that is gone; the
 * beat is how it tells. Every context is beaten, not only the busy one: a
 * context whose work waits behind another's long call is waiting on the same
 * device. A device with nothing at work beats nothing, so a guest still sees
 * a device that dropped its work go quiet - and the thread sleeps until a
 * worker stirs it, rather than waking each period for nothing. It runs only
 * while it carries a page. */
#ifndef AV_DEVICE_PULSE_HPP
#define AV_DEVICE_PULSE_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class FenceReporter;
class Host;
struct DevicePulseProbe;

class DevicePulse {
public:
    /* One per context a device can hold (DeviceContextTable::max_contexts). */
    static constexpr uint32_t capacity = 64u;
    /* Far inside what a guest waits through before it calls a quiet device
     * gone (HostWatch::silence_seconds), and rare enough to cost nothing. */
    static constexpr uint64_t period_ns = 250000000u;

    explicit DevicePulse(Host &host);
    ~DevicePulse();
    DevicePulse(const DevicePulse &) = delete;
    DevicePulse &operator=(const DevicePulse &) = delete;

    /* Main loop. no_memory when the host has no mutex or event; invalid_state
     * once done. */
    [[nodiscard]] Status initialize();
    /* Main loop: `reporter`'s page is beaten from now until it is dropped,
     * and the thread started if it was not. invalid_state before initialize
     * or for a reporter carried already; no_space when full; no_memory when
     * no thread starts, carrying nothing. */
    [[nodiscard]] Status carry(FenceReporter &reporter);
    /* Main loop: before `reporter`'s page goes, and the thread stopped with
     * the last. Nothing for one not carried. */
    void                 drop(FenceReporter &reporter);
    uint32_t             carried() const;
    /* Any thread: a worker went to work. Wakes a pulse that went dormant,
     * and costs one load otherwise. */
    void                 stir();
    /* The thread's body, entered through AvDevicePulseRun. */
    void                 run();
    /* One pass: every carried page beaten if any of them is at work, and
     * `working` says whether one was. False, beating nothing, once the pulse
     * is stopping. */
    bool                 tick(bool &working);
    /* After a pass that found nothing at work: whether the thread may sleep
     * until stirred, which it may only if, now that it is marked dormant,
     * nothing is at work still. */
    bool                 rest();

    bool invariants_hold() const;

private:
    void stop_thread();
    /* Under the lock. */
    bool any_working() const;

    Host          &host_;
    void          *lock_;
    void          *wake_;
    void          *thread_;
    bool           stopping_;
    /* Shared with every worker that stirs it, so stored and read atomically. */
    bool           dormant_;
    FenceReporter *carried_[capacity];

    friend struct DevicePulseProbe;
};

}  // namespace av

#endif  // AV_DEVICE_PULSE_HPP
