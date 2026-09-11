/* The device's fence interrupts: workers raise them, and they reach the guest
 * only while the guest notifiers that carry them exist. The main loop connects
 * the notifiers when the driver starts and releases them in reset once every
 * context is gone; one mutex holds every connect, release and raise apart, so a
 * worker never touches a notifier being set up or torn down. */
#ifndef AV_FENCE_INTERRUPTS_HPP
#define AV_FENCE_INTERRUPTS_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Host;
struct FenceInterruptsProbe;

class FenceInterrupts {
public:
    explicit FenceInterrupts(Host &host);
    ~FenceInterrupts();
    FenceInterrupts(const FenceInterrupts &) = delete;
    FenceInterrupts &operator=(const FenceInterrupts &) = delete;

    /* Main loop. no_memory when the host has no mutex; invalid_state once done. */
    [[nodiscard]] Status initialize();
    /* Main loop. invalid_state unless initialized; ok when already connected;
     * the host's answer otherwise. */
    [[nodiscard]] Status connect();
    /* Main loop. */
    void                 disconnect();
    /* Any thread. Nothing while disconnected, or for a line past the fence queues. */
    void                 raise(uint32_t line);
    bool                 connected() const;

    bool invariants_hold() const;

private:
    Host       &host_;
    void       *lock_;
    bool        connected_;

    friend struct FenceInterruptsProbe;
};

}  // namespace av

#endif  // AV_FENCE_INTERRUPTS_HPP
