/* Why one device is gone, said once (contracts/umd_interface.md, "Device
 * removal"). The transport's context fault is the cause; the D3D spelling is
 * DXGI_ERROR_DEVICE_REMOVED for an adapter-wide loss and DXGI_ERROR_DEVICE_RESET
 * when this device's own context was faulted.
 *
 * The first cause is the one kept, because GetDeviceRemovedReason must name
 * what happened rather than what happened last: a removal cascades, and the
 * second reason is a consequence of the first. */
#ifndef AV_D3D_DEVICE_REMOVAL_HPP
#define AV_D3D_DEVICE_REMOVAL_HPP

#include "icd/d3d/hresult.hpp"

#include <stdint.h>

namespace av::icd {
class LogSink;
}  // namespace av::icd

namespace av::d3d {

struct DeviceRemovalProbe;

class DeviceRemoval {
public:
    DeviceRemoval();
    DeviceRemoval(const DeviceRemoval &) = delete;
    DeviceRemoval &operator=(const DeviceRemoval &) = delete;

    /* hr_ok is not a removal and is ignored. A later reason is counted and
     * dropped; the first one stands. */
    void    remove(Hresult reason);
    bool    removed() const;
    /* hr_ok while live; the first reason once removed. */
    Hresult reason() const;
    /* The reason, logged the first time a caller notices it, naming the entry
     * point that did. hr_ok while live. */
    Hresult check(icd::LogSink &log, const char *entry);
    /* How many causes arrived after the first. */
    uint32_t later_causes() const;

    bool invariants_hold() const;

private:
    Hresult  reason_;
    uint32_t later_causes_;
    bool     reported_;

    friend struct DeviceRemovalProbe;
};

}  // namespace av::d3d

#endif  // AV_D3D_DEVICE_REMOVAL_HPP
