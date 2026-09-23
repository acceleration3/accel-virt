/* The context states of one session's devices, by the id the guest named each
 * device with.
 *
 * A guest process runs every Direct3D 11 device it makes on one session, so
 * the doorbell, ring and arena a session costs are spent once per process
 * rather than once per device - the desktop's processes make dozens. What a
 * device owns on the host is its pipeline state, an ID3DDeviceContextState;
 * the session swaps in the selected device's before running its commands.
 * Each entry holds one reference to its state. */
#ifndef AV_D3D_CONTEXT_STATES_HPP
#define AV_D3D_CONTEXT_STATES_HPP

#include <stdint.h>

struct ID3DDeviceContextState;

namespace av {

struct D3dContextStatesProbe;

class D3dContextStates {
public:
    /* The most devices one process runs at once. */
    static constexpr uint32_t capacity = 64u;

    D3dContextStates();
    D3dContextStates(const D3dContextStates &) = delete;
    D3dContextStates &operator=(const D3dContextStates &) = delete;

    /* Keeps `state` under `device`. False for id 0, an id already kept, or a
     * full table; the reference stays the caller's then. */
    [[nodiscard]] bool      add(uint32_t device, ID3DDeviceContextState *state);
    ID3DDeviceContextState *find(uint32_t device) const;
    /* The state kept under `device`, no longer kept; its reference passes to
     * the caller. Null for an id that names nothing. */
    ID3DDeviceContextState *remove(uint32_t device);
    /* Any kept state, no longer kept, with its reference; null when none is.
     * How a closing session gives every state back. */
    ID3DDeviceContextState *remove_any();
    uint32_t                count() const;
    /* The device is removed for `reason` (an HRESULT, non-zero); the first
     * reason stands, since it is what went wrong and the rest follow from it.
     * Nothing for a device not kept. */
    void                    mark_removed(uint32_t device, uint32_t reason);
    /* Why the device was removed; 0 while it was not, or for one not kept. */
    uint32_t                removed_reason(uint32_t device) const;

    bool invariants_hold() const;

private:
    uint32_t slot_of(uint32_t device) const;

    uint32_t                ids_[capacity];
    ID3DDeviceContextState *states_[capacity];
    uint32_t                removed_[capacity];
    uint32_t                count_;

    friend struct D3dContextStatesProbe;
};

}  // namespace av

#endif  // AV_D3D_CONTEXT_STATES_HPP
