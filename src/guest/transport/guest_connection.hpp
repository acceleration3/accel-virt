/* The adapter, device and paging queue a process shares across its per-sub-API
 * transports (docs/protocol.md; feature 043). A process opens the rvk adapter
 * and one device once; each KmtTransport it then creates - one per sub-API
 * context - runs its allocations and context on this device and makes them
 * resident through this queue. All the D3DKMT calls go through the injected
 * Kmt, so a test drives it over MockKmt. */
#ifndef AV_GUEST_CONNECTION_HPP
#define AV_GUEST_CONNECTION_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Kmt;
struct GuestConnectionProbe;

class GuestConnection {
public:
    explicit GuestConnection(Kmt &kmt);
    GuestConnection(const GuestConnection &) = delete;
    GuestConnection &operator=(const GuestConnection &) = delete;

    /* Opens the first adapter, a device on it and a paging queue. invalid_state
     * once open; the Kmt's own error otherwise, and nothing is left open. */
    [[nodiscard]] Status open();
    /* Destroys the queue and device and closes the adapter, in reverse. */
    void                 close();

    bool     opened() const;
    Kmt     &kmt() const;
    uint32_t adapter() const;
    uint32_t device() const;
    uint32_t queue() const;
    uint32_t fence_object() const;

    bool invariants_hold() const;

private:
    Kmt      &kmt_;
    uint32_t  adapter_;
    uint32_t  device_;
    uint32_t  queue_;
    uint32_t  fence_object_;

    friend struct GuestConnectionProbe;
};

}  // namespace av

#endif  // AV_GUEST_CONNECTION_HPP
