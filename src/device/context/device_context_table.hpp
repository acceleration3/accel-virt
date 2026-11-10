#ifndef AV_DEVICE_CONTEXT_TABLE_HPP
#define AV_DEVICE_CONTEXT_TABLE_HPP

#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "proto/context/context_capacity.hpp"
#include "device/context/d3d_ordering.hpp"
#include "device/context/device_context.hpp"
#include "device/execution/executor_shares.hpp"
#include "device/fence/device_pulse.hpp"

#include <stdint.h>

namespace av {

class BlobTable;
class HostBlobOwners;
class ExecutorLibrary;
class FenceInterrupts;
class Host;
class StateWriter;
class WireRecorder;
struct DeviceContextTableProbe;

class DeviceContextTable {
public:
    static constexpr uint32_t max_contexts = ContextCapacity::contexts;

    DeviceContextTable(Host &host, BlobTable &blobs, HostBlobOwners &owners, FenceInterrupts &interrupts,
                       const ExecutorLibrary &library, WireRecorder &recorder);
    ~DeviceContextTable();
    DeviceContextTable(const DeviceContextTable &) = delete;
    DeviceContextTable &operator=(const DeviceContextTable &) = delete;

    [[nodiscard]] Status initialize();
    void                 settle_d3d();
    [[nodiscard]] Status create(uint32_t id, const char *name);
    [[nodiscard]] Status destroy(uint32_t id);
    void                 destroy_all();
    DeviceContext       *find(uint32_t id);
    bool                 holds_doorbell(uint32_t index);
    bool                 can_share(uint32_t asking, uint32_t share);
    uint32_t             live() const;
    void                 report(StateWriter &writer);

    bool invariants_hold() const;

private:
    ObjectStorage<DeviceContext> *slot_of(uint32_t id);

    Host                        &host_;
    BlobTable                   &blobs_;
    HostBlobOwners              &owners_;
    FenceInterrupts             &interrupts_;
    const ExecutorLibrary       &library_;
    WireRecorder                &recorder_;
    ExecutorShares               shares_;
    D3dOrdering                  ordering_;
    DevicePulse                  pulse_;
    ObjectStorage<DeviceContext> slots_[max_contexts];

    friend struct DeviceContextTableProbe;
};

}

#endif
