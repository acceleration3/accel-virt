/* The device and paging queue a process shares across its per-sub-API
 * transports, on the rvk adapter (docs/protocol.md; feature 043). The adapter
 * is found and owned by the caller - D3DKMT enumerates every adapter in the
 * guest, Microsoft Basic Render among them, and only a PCI identity says which
 * is rvk. Each KmtTransport a process then creates - one per sub-API context -
 * runs its allocations and context on this device and makes them resident
 * through this queue. All the D3DKMT calls go through the injected Kmt, so a
 * test drives it over MockKmt. */
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

    /* Creates a device on `adapter` and a paging queue on the device.
     * invalid_parameter for adapter 0; invalid_state once open; the Kmt's own
     * error otherwise, and nothing is left open. */
    [[nodiscard]] Status open(uint32_t adapter);
    /* Destroys the queue and the device; the adapter stays open, the caller's. */
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
