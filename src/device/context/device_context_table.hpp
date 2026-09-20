/* A device's contexts, by the ids the guest gives them. Context 0 is
 * virtio-gpu's "no context" and is never created. Destroying a context stops
 * and joins its worker, and lets go of its blobs; reset destroys every context.
 * Main loop only. */
#ifndef AV_DEVICE_CONTEXT_TABLE_HPP
#define AV_DEVICE_CONTEXT_TABLE_HPP

#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "device/context/device_context.hpp"
#include "device/execution/executor_shares.hpp"

#include <stdint.h>

namespace av {

class BlobTable;
class ExecutorLibrary;
class FenceInterrupts;
class HostSharedObjects;
class Host;
class StateWriter;
class WireRecorder;
struct DeviceContextTableProbe;

class DeviceContextTable {
public:
    /* One worker thread each, so a bound on threads as much as on contexts. */
    static constexpr uint32_t max_contexts = 64u;

    DeviceContextTable(Host &host, BlobTable &blobs, HostSharedObjects &shared, FenceInterrupts &interrupts,
                       const ExecutorLibrary &library, WireRecorder &recorder);
    ~DeviceContextTable();
    DeviceContextTable(const DeviceContextTable &) = delete;
    DeviceContextTable &operator=(const DeviceContextTable &) = delete;

    /* The lock the contexts share executors under: invalid_state once made,
     * no_memory when the host has none. */
    [[nodiscard]] Status initialize();
    /* invalid_parameter for id 0, conflict for a live id, no_space when full,
     * or the worker's failure, leaving no context behind. */
    [[nodiscard]] Status create(uint32_t id);
    [[nodiscard]] Status destroy(uint32_t id);
    void                 destroy_all();
    DeviceContext       *find(uint32_t id);
    /* Whether a bound context holds doorbell page `index`. */
    bool                 holds_doorbell(uint32_t index);
    /* Whether context `asking` may share context `share`'s objects: another
     * context, bound, speaking OpenGL (feature 050). Both are on this device's
     * one GPU. */
    bool                 can_share(uint32_t asking, uint32_t share);
    uint32_t             live() const;
    /* Main loop: its line, then each context in slot order. */
    void                 report(StateWriter &writer);

    bool invariants_hold() const;

private:
    ObjectStorage<DeviceContext> *slot_of(uint32_t id);

    Host                        &host_;
    BlobTable                   &blobs_;
    HostSharedObjects           &shared_;
    FenceInterrupts             &interrupts_;
    const ExecutorLibrary       &library_;
    WireRecorder                &recorder_;
    /* Before the contexts, whose executors are withdrawn from it as they go. */
    ExecutorShares               shares_;
    ObjectStorage<DeviceContext> slots_[max_contexts];

    friend struct DeviceContextTableProbe;
};

}  // namespace av

#endif  // AV_DEVICE_CONTEXT_TABLE_HPP
