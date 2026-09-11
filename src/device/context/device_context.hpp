/* One guest API context on the device: the virtio-gpu context id the guest
 * named and the worker its work runs on. */
#ifndef AV_DEVICE_CONTEXT_HPP
#define AV_DEVICE_CONTEXT_HPP

#include "core/support/status.hpp"
#include "device/threading/worker.hpp"

#include <stdint.h>

namespace av {

class Host;
class WorkItem;

class DeviceContext {
public:
    DeviceContext(Host &host, uint32_t id);
    DeviceContext(const DeviceContext &) = delete;
    DeviceContext &operator=(const DeviceContext &) = delete;

    /* Main loop: starts the context's worker. */
    [[nodiscard]] Status start();
    [[nodiscard]] Status post(const WorkItem &item);
    uint32_t             id() const;
    bool                 running() const;

private:
    uint32_t id_;
    Worker   worker_;
};

}  // namespace av

#endif  // AV_DEVICE_CONTEXT_HPP
