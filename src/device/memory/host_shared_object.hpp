/* One host resource several guest contexts name (feature 053, data-model.md).
 *
 * A guest context is a host context and a guest device is a host device, so
 * "one host object" cannot mean one VkDeviceMemory: two VkDevices cannot share
 * one. It means one exported descriptor, from which each importer makes its
 * own object over the same storage - which is what feature 047 already does
 * inside a single context, moved up to the whole device.
 *
 * This owns that descriptor. The count is contexts holding the object, not
 * descriptors handed out: an importer duplicates what it is shown, so the one
 * this holds is closed exactly once, when the last holder drops. */
#ifndef AV_HOST_SHARED_OBJECT_HPP
#define AV_HOST_SHARED_OBJECT_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Host;
struct HostSharedObjectProbe;

class HostSharedObject {
public:
    HostSharedObject();
    HostSharedObject(const HostSharedObject &) = delete;
    HostSharedObject &operator=(const HostSharedObject &) = delete;

    /* Takes `descriptor` for `object_id`, with one holder - whoever published
     * it. invalid_parameter for an id of 0, a kind that is not a
     * AV_SHARED_* value, a handle kind that is not an AV_SHARED_HANDLE_*
     * value, a size that is not what its kind has - memory is a length and a
     * fence is not - or a descriptor below 0; invalid_state for a slot already live.
     * The descriptor is this object's on success and the caller's otherwise. */
    [[nodiscard]] Status take(uint64_t object_id, uint32_t kind, uint32_t handle_kind, uint64_t size,
                              int descriptor);
    /* One more context holding it. */
    void                 hold();
    /* One fewer, closing the descriptor through `host` when it was the last.
     * Whether this was the last, which is how a table knows the slot is free. */
    [[nodiscard]] bool   drop(Host &host);
    /* Closes and empties whatever is held, whatever the count: a reset. Safe
     * on a slot holding nothing. */
    void                 give_back(Host &host);

    bool     live() const;
    bool     holds(uint64_t object_id) const;
    uint64_t id() const;
    uint32_t kind() const;
    uint32_t handle_kind() const;
    uint64_t size() const;
    /* The descriptor, which stays this object's: an importer duplicates it. */
    int      descriptor() const;
    uint32_t holders() const;

    bool invariants_hold() const;

private:
    uint64_t id_;
    uint32_t kind_;
    uint32_t handle_kind_;
    uint64_t size_;
    int      descriptor_;
    uint32_t holders_;

    friend struct HostSharedObjectProbe;
};

}  // namespace av

#endif  // AV_HOST_SHARED_OBJECT_HPP
