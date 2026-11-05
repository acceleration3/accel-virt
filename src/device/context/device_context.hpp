#ifndef AV_DEVICE_CONTEXT_HPP
#define AV_DEVICE_CONTEXT_HPP

#include "core/support/status.hpp"
#include "device/context/context_binding.hpp"
#include "device/context/context_signal.hpp"
#include "device/execution/batch_executor.hpp"
#include "device/execution/blob_imports.hpp"
#include "device/execution/batch_staging.hpp"
#include "device/execution/context_executor.hpp"
#include "device/fence/fence_reporter.hpp"
#include "device/context/ring_consumer.hpp"
#include "device/threading/worker.hpp"
#include "device/context/context_label.hpp"

#include <stdint.h>

namespace av {

class D3dOrdering;
class DevicePulse;

class BlobTable;
class CapsetInfo;
class ExecutorLibrary;
class ExecutorShares;
class FenceInterrupts;
class ContextSetup;
class HostBlobOwners;
class Host;
class StateWriter;
class WireRecorder;
class WorkItem;
struct DeviceContextProbe;

class DeviceContext {
public:
    DeviceContext(Host &host, BlobTable &blobs, HostBlobOwners &owners, FenceInterrupts &interrupts,
                  const ExecutorLibrary &library, ExecutorShares &shares, WireRecorder &recorder, D3dOrdering &ordering,
                  DevicePulse &pulse, uint32_t id, const char *name);
    ~DeviceContext();
    DeviceContext(const DeviceContext &) = delete;
    DeviceContext &operator=(const DeviceContext &) = delete;

    [[nodiscard]] Status start();
    [[nodiscard]] Status post(const WorkItem &item);
    [[nodiscard]] Status  bind(const ContextSetup &setup, const CapsetInfo &capset);
    Status                refused(const char *step, Status status) const;
    const ContextBinding &binding() const;
    uint32_t             id() const;
    bool                 running() const;
    void                 report(StateWriter &writer);

private:
    D3dOrdering          &ordering_;
    DevicePulse          &pulse_;
    WireRecorder  &recorder_;
    uint32_t       id_;
    ContextLabel   label_;
    ContextBinding binding_;
    FenceReporter   fences_;
    BlobImports          imports_;
    ContextExecutor executor_;
    BatchStaging    staging_;
    BatchExecutor   batches_;
    RingConsumer    ring_;
    ContextSignal   signal_;
    Worker          worker_;

    friend struct DeviceContextProbe;
};

}

#endif
