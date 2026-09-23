/* One guest API context on the device: the virtio-gpu context id the guest
 * named, what its setup bound it to, how it reports completions, the ring its
 * worker consumes, and the worker. Members are declared so the worker is
 * destroyed first: it disconnects the doorbell and joins before the reporter,
 * the ring or the pins go. */
#ifndef AV_DEVICE_CONTEXT_HPP
#define AV_DEVICE_CONTEXT_HPP

#include "core/support/status.hpp"
#include "device/context/context_binding.hpp"
#include "device/context/context_signal.hpp"
#include "device/execution/batch_executor.hpp"
#include "device/execution/blob_imports.hpp"
#include "device/execution/context_shared_objects.hpp"
#include "device/execution/batch_staging.hpp"
#include "device/execution/context_executor.hpp"
#include "device/fence/fence_reporter.hpp"
#include "device/ring/ring_consumer.hpp"
#include "device/threading/worker.hpp"

#include <stdint.h>

namespace av {

class BlobTable;
class CapsetInfo;
class ExecutorLibrary;
class ExecutorShares;
class FenceInterrupts;
class HostSharedObjects;
class ContextSetup;
class Host;
class StateWriter;
class WireRecorder;
class WorkItem;
struct DeviceContextProbe;

class DeviceContext {
public:
    DeviceContext(Host &host, BlobTable &blobs, HostSharedObjects &shared, FenceInterrupts &interrupts,
                  const ExecutorLibrary &library, ExecutorShares &shares, WireRecorder &recorder, uint32_t id);
    DeviceContext(const DeviceContext &) = delete;
    DeviceContext &operator=(const DeviceContext &) = delete;

    /* Main loop: starts the context's worker. */
    [[nodiscard]] Status start();
    [[nodiscard]] Status post(const WorkItem &item);
    /* Main loop. invalid_parameter, with nothing bound, for options the
     * library cannot make; the binding's answer; invalid_parameter for a ring
     * header that is not fresh or a fence page that cannot be prepared; no_memory when the
     * host cannot stage its batches; the worker's answer when the doorbell cannot be connected
     * or the ring handed over. Nothing stays pinned, connected or prepared
     * unless ok. */
    [[nodiscard]] Status  bind(const ContextSetup &setup, const CapsetInfo &capset);
    Status                refused(const char *step, Status status) const;
    const ContextBinding &binding() const;
    uint32_t             id() const;
    bool                 running() const;
    /* Main loop: its line and its worker's. The ring's progress is the
     * worker's own, read as it last wrote it. */
    void                 report(StateWriter &writer);

private:
    WireRecorder  &recorder_;
    uint32_t       id_;
    ContextBinding binding_;
    FenceReporter   fences_;
    /* Before the executor: an import points into an alias until the executor
     * that holds it is gone, and a shared object is the executor's to destroy
     * until it is. */
    BlobImports          imports_;
    ContextSharedObjects shared_;
    ContextExecutor executor_;
    BatchStaging    staging_;
    BatchExecutor   batches_;
    RingConsumer    ring_;
    /* Before the worker: the worker drains it, and is stopped before it goes. */
    ContextSignal   signal_;
    Worker          worker_;

    friend struct DeviceContextProbe;
};

}  // namespace av

#endif  // AV_DEVICE_CONTEXT_HPP
