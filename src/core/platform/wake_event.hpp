/* What a thread waiting at PASSIVE for something another context reports -
 * an interrupt's DPC, say - sleeps on instead of spinning a processor: set at
 * DISPATCH or below, waited on at PASSIVE, and unset by the wait it ends (the
 * kernel's synchronization KEVENT). A set nobody waited for wakes the next
 * wait at once, so a waiter checks what it waits for after every wake. */
#ifndef AV_WAKE_EVENT_HPP
#define AV_WAKE_EVENT_HPP

#include "core/platform/platform.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WakeEvent {
public:
    /* Opaque room for the kernel's KEVENT (24 bytes on x64); src/km proves it
     * fits at compile time. */
    static constexpr size_t storage_bytes = 32u;

    explicit WakeEvent(Platform &platform);
    WakeEvent(const WakeEvent &) = delete;
    WakeEvent &operator=(const WakeEvent &) = delete;

    void               set();
    /* Whether the event was set within `ticks` (100 ns). */
    [[nodiscard]] bool wait(uint64_t ticks);

private:
    Platform &platform_;
    alignas(8) unsigned char storage_[storage_bytes];
};

}  // namespace av

#endif  // AV_WAKE_EVENT_HPP
