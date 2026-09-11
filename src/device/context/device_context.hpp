/* One guest API context on the device: the virtio-gpu context id the guest
 * named, what its setup bound it to, how it reports completions, the ring its
 * worker consumes, and the worker. Members are declared so the worker is
 * destroyed first: it disconnects the doorbell and joins before the reporter,
 * the ring or the pins go. */
#ifndef AV_DEVICE_CONTEXT_HPP
#define AV_DEVICE_CONTEXT_HPP

#include "core/support/status.hpp"
#include "device/context/context_binding.hpp"
#include "device/fence/fence_reporter.hpp"
#include "device/ring/ring_consumer.hpp"
#include "device/threading/worker.hpp"

#include <stdint.h>

namespace av {

class BlobTable;
class CapsetInfo;
class FenceInterrupts;
class ContextSetup;
class Host;
class WorkItem;
struct DeviceContextProbe;

class DeviceContext {
public:
    DeviceContext(Host &host, BlobTable &blobs, FenceInterrupts &interrupts, uint32_t id);
    DeviceContext(const DeviceContext &) = delete;
    DeviceContext &operator=(const DeviceContext &) = delete;

    /* Main loop: starts the context's worker. */
    [[nodiscard]] Status start();
    [[nodiscard]] Status post(const WorkItem &item);
    /* Main loop. The binding's answer; invalid_parameter for a ring header that
     * is not fresh or a fence page that cannot be prepared; the worker's answer when the doorbell cannot be connected
     * or the ring handed over. Nothing stays pinned, connected or prepared
     * unless ok. */
    [[nodiscard]] Status  bind(const ContextSetup &setup, const CapsetInfo &capset);
    const ContextBinding &binding() const;
    uint32_t             id() const;
    bool                 running() const;

private:
    uint32_t       id_;
    ContextBinding binding_;
    FenceReporter  fences_;
    RingConsumer   ring_;
    Worker         worker_;

    friend struct DeviceContextProbe;
};

}  // namespace av

#endif  // AV_DEVICE_CONTEXT_HPP
