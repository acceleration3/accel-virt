/* The device's shared objects, by the ids the guest gives them (feature 053).
 *
 * This is where kb 11's risk 6 is answered: "device object identity must be
 * global, not per-open". A blob belongs to the context that made it and an
 * import belongs to the session that took it, but a shared object belongs to
 * the device - so a second context, of a second guest device, of a second
 * guest process, naming the same id reaches the same host descriptor.
 *
 * Every worker reaches it, so every method takes its own lock. A descriptor
 * handed out is safe to use after the lock is released because handing it out
 * takes a reference: nothing can close it while the caller is importing it. */
#ifndef AV_HOST_SHARED_OBJECTS_HPP
#define AV_HOST_SHARED_OBJECTS_HPP

#include "core/support/status.hpp"
#include "device/memory/host_shared_object.hpp"

#include <stdint.h>

namespace av {

class Host;
class StateWriter;
struct HostSharedObjectsProbe;

class HostSharedObjects {
public:
    /* As many as one session may import (ImportTable::max_imports): a device
     * that holds a thousand blobs may publish a quarter of them. */
    static constexpr uint32_t max_objects = 256u;

    explicit HostSharedObjects(Host &host);
    ~HostSharedObjects();
    HostSharedObjects(const HostSharedObjects &) = delete;
    HostSharedObjects &operator=(const HostSharedObjects &) = delete;

    /* Main loop, once: no_memory when the host has no lock to give,
     * invalid_state once done. */
    [[nodiscard]] Status initialize();

    /* Any worker. `descriptor` recorded under `object_id` with one holder.
     * invalid_state before initialize; conflict for an id already published -
     * a silent replacement is what a hostile guest would drive at the worst
     * moment; no_space when the table is full; the object's answer for a field
     * no object can have.
     *
     * The descriptor is this table's whatever the answer, and a refusal closes
     * it: a caller that exported one has nowhere to close it, and the two
     * callers that publish - a context and a test - would each have to carry a
     * Host to do so. */
    [[nodiscard]] Status publish(uint64_t object_id, uint32_t kind, uint32_t handle_kind, uint64_t size,
                                 int descriptor);
    /* Any worker. One more holder of `object_id`, with what it is in the three
     * out parameters. not_found for an id this device does not hold;
     * invalid_parameter for a kind or a `claimed_size` that disagrees with it -
     * a guest that could open an image as a fence could have the host read a
     * semaphore as memory, and one that could open it longer than it is could
     * have the host read past its end.
     * Nothing is held unless ok, and the descriptor stays this table's: an
     * importer that keeps one duplicates it. */
    [[nodiscard]] Status open(uint64_t object_id, uint32_t kind, uint64_t claimed_size,
                              uint32_t &handle_kind, uint64_t &size, int &descriptor);
    /* Any worker. One fewer holder, closing the descriptor when it was the
     * last. An id this device does not hold is ignored: a release is
     * idempotent, because a context tearing down releases what it may have
     * already lost. */
    void                 close(uint64_t object_id);
    /* Main loop. Every object, whatever its count: a device reset leaves no
     * context to hold one. */
    void                 release_all();

    uint32_t live() const;
    /* Main loop: its line, then one per object. `busy=true` and nothing more
     * while a worker holds it. */
    void     report(StateWriter &writer);

    /* Every live slot has a distinct non-zero id, and the count matches. */
    bool invariants_hold() const;

private:
    /* The slot holding `object_id`, or null. The caller holds the lock. */
    HostSharedObject *slot_of(uint64_t object_id);

    Host            &host_;
    void            *lock_;
    HostSharedObject objects_[max_objects];

    friend struct HostSharedObjectsProbe;
};

}  // namespace av

#endif  // AV_HOST_SHARED_OBJECTS_HPP
