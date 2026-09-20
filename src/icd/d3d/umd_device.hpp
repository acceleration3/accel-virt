/* One D3D device the runtime made on this adapter (contracts/umd_interface.md).
 *
 * Its memory is the driver's; the runtime's own block holds only a
 * DeviceCookie naming it. It keeps the runtime's two handles and both callback
 * tables as pointers - the runtime may move a callback whenever no thread is
 * inside the driver - publishes its DXGI entry points, and answers its removal
 * code once it has one.
 *
 * 055 makes no device functions, so the create that built this answers
 * E_NOTIMPL and the device is given straight back; feature 056 is what turns
 * that into a device an application can draw with, over this same object. */
#ifndef AV_D3D_UMD_DEVICE_HPP
#define AV_D3D_UMD_DEVICE_HPP

#include "icd/d3d/device_removal.hpp"
#include "icd/d3d/dxgi_base_table.hpp"
#include "icd/d3d/hresult.hpp"

#include <stdint.h>

namespace av {
class StateWriter;
}

namespace av::icd {
class LogSink;
}  // namespace av::icd

namespace av::d3d {

class UmdAdapter;
struct D3d10ddiargCreateDeviceRecord;

class UmdDevice {
public:
    UmdDevice(UmdAdapter &adapter, icd::LogSink &log);
    UmdDevice(const UmdDevice &) = delete;
    UmdDevice &operator=(const UmdDevice &) = delete;

    /* Records what the create gave and publishes the DXGI table at the
     * revision `create.Interface` names. hr_invalid_arg for a create with no
     * runtime device handle or no kernel callbacks; whatever the DXGI seam
     * refuses otherwise. Nothing is recorded unless it answers hr_ok. */
    Hresult adopt(const D3d10ddiargCreateDeviceRecord &create);

    UmdAdapter    &adapter();
    void          *runtime_device() const;
    void          *runtime_core_layer() const;
    const void    *kernel_callbacks() const;
    const void    *core_layer_callbacks() const;
    uint32_t       interface_version() const;
    DxgiBaseTable &dxgi();
    DeviceRemoval &removal();

    /* The removal code, or hr_ok: what every entry point that returns an
     * HRESULT answers first, before it asks the transport anything. */
    Hresult lost(const char *entry);

    void report(StateWriter &writer, uint32_t index) const;

    bool invariants_hold() const;

private:
    UmdAdapter   &adapter_;
    icd::LogSink &log_;
    void         *runtime_device_;
    void         *runtime_core_layer_;
    const void   *kernel_callbacks_;
    const void   *core_layer_callbacks_;
    uint32_t      interface_version_;
    uint32_t      runtime_version_;
    DxgiBaseTable dxgi_;
    DeviceRemoval removal_;
};

}  // namespace av::d3d

#endif  // AV_D3D_UMD_DEVICE_HPP
