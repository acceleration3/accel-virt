/* One guest API context on the device: the virtio-gpu context id the guest
 * named, the worker its work runs on, and what its setup bound it to. */
#ifndef AV_DEVICE_CONTEXT_HPP
#define AV_DEVICE_CONTEXT_HPP

#include "core/support/status.hpp"
#include "device/context/context_binding.hpp"
#include "device/threading/worker.hpp"

#include <stdint.h>

namespace av {

class BlobTable;
class CapsetInfo;
class ContextSetup;
class Host;
class WorkItem;

class DeviceContext {
public:
    DeviceContext(Host &host, BlobTable &blobs, uint32_t id);
    DeviceContext(const DeviceContext &) = delete;
    DeviceContext &operator=(const DeviceContext &) = delete;

    /* Main loop: starts the context's worker. */
    [[nodiscard]] Status start();
    [[nodiscard]] Status post(const WorkItem &item);
    /* Main loop: the binding's answer. */
    [[nodiscard]] Status  bind(const ContextSetup &setup, const CapsetInfo &capset);
    const ContextBinding &binding() const;
    uint32_t             id() const;
    bool                 running() const;

private:
    uint32_t       id_;
    Worker         worker_;
    ContextBinding binding_;
};

}  // namespace av

#endif  // AV_DEVICE_CONTEXT_HPP
