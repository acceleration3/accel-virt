/* One context's share of the device's shared objects (feature 053).
 *
 * The device's `HostSharedObjects` holds what an object *is* - one descriptor,
 * refcounted - and this holds what one context made of it: the wire handle its
 * executor answered, which the guest names in that API's own commands. The two
 * are apart because they die at different times. A context faulting, or its
 * process exiting, takes this table with it and drops its references; the
 * object itself lives while anyone else holds it, which is what US1 scenario 3
 * and US3 scenario 2 both ask for.
 *
 * Worker, except configure. Modelled on `BlobImports`, which it sits beside in
 * `DeviceContext` for the same reason: both hold what an executor was handed. */
#ifndef AV_CONTEXT_SHARED_OBJECTS_HPP
#define AV_CONTEXT_SHARED_OBJECTS_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class ContextExecutor;
class HostSharedObjects;
struct ContextSharedObjectsProbe;

/* One row. A type of its own so the table's slots are complete
 * (tools/header_scan.py reads a class's members only to its first nested
 * type). */
struct ContextSharedObjectSlot {
    uint64_t object_id_;
    uint64_t wire_handle_;
    uint32_t kind_;
    bool     published_;
};

class ContextSharedObjects {
public:
    /* As many as the ICD's own table of exported handles holds
     * (icd::SharedHandles): a guest that shared more than this could not have
     * named them. */
    static constexpr uint32_t max_objects = 64u;

    ContextSharedObjects(HostSharedObjects &objects, ContextExecutor &executor);
    /* Main loop, after the worker destroyed the executor, which released every
     * object it made: what is dropped here is this context's references on the
     * device's table, so an object nobody else holds goes with it. The same
     * order BlobImports is destroyed in, and for the same reason. */
    ~ContextSharedObjects();
    ContextSharedObjects(const ContextSharedObjects &) = delete;
    ContextSharedObjects &operator=(const ContextSharedObjects &) = delete;

    /* Worker. `wire_handle`, an object this context's guest made, published
     * under `object_id` for every context on this device to open. ok, and
     * nothing changed, when this context already published that id with that
     * handle; conflict for a different handle under it or an id someone else
     * published; not_supported when the executor exports nothing; no_space
     * when this context holds max_objects. */
    [[nodiscard]] Status publish(uint64_t object_id, uint32_t kind, uint64_t wire_handle, uint64_t size);
    /* Worker. The object `object_id` names, opened into this context, with
     * `wire_handle` what the guest names it by. not_found for an id this
     * device does not hold; invalid_parameter for a kind that disagrees with
     * it or a `size` that does not; conflict when this context already holds
     * it - a second handle for one object in one context is an object the
     * guest could free twice;
     * not_supported when the executor imports nothing; no_space when this
     * context holds max_objects. Nothing is held unless ok. */
    [[nodiscard]] Status open(uint64_t object_id, uint32_t kind, uint64_t size, uint64_t &wire_handle);
    /* Worker. What this context made of `object_id`, destroyed, and its
     * reference dropped. An id this context never held does nothing, as
     * releasing a blob it never imported does. */
    void                 release(uint64_t object_id);
    /* Main loop, after the worker destroyed the executor: every reference this
     * context still holds, let go of. What the destructor does, and what a
     * context being torn down does before it is destroyed. */
    void                 release_all();
    uint32_t             held() const;

    /* A slot is empty or names one object with one wire handle and one kind,
     * and no object appears twice. */
    bool invariants_hold() const;

private:
    /* The slot holding `object_id`, or null. */
    ContextSharedObjectSlot       *slot_of(uint64_t object_id);
    const ContextSharedObjectSlot *slot_of(uint64_t object_id) const;
    /* A free slot, or null when every one is taken. */
    ContextSharedObjectSlot       *free_slot();
    void                           give_back(ContextSharedObjectSlot &slot);

    HostSharedObjects       &objects_;
    ContextExecutor         &executor_;
    ContextSharedObjectSlot  slots_[max_objects];

    friend struct ContextSharedObjectsProbe;
};

}  // namespace av

#endif  // AV_CONTEXT_SHARED_OBJECTS_HPP
