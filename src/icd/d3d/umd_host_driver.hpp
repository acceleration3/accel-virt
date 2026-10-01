/* The part of the driver that needs the host: what a Direct3D 11 adapter can
 * do, which only the host GPU's own device can answer, and turning an adopted
 * device into one the runtime can draw with - the device-functions table
 * filled, the host session opened, the device's own state made. The root
 * adopts the device - its cookie, its DXGI table, its place in the table - and
 * hands the rest here.
 *
 * Virtual because there are two implementations, the one that matters and the
 * one that makes the root's lifetime testable on Linux: the real device is
 * written against the WDK's own DDI types (src/um/d3d), which do not exist on
 * the machine the root's tests run on. A root with no builder refuses every
 * device, as it did before there was one. */
#ifndef AV_D3D_UMD_HOST_DRIVER_HPP
#define AV_D3D_UMD_HOST_DRIVER_HPP

#include "icd/d3d/hresult.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av::d3d {

class UmdAdapter;
class UmdDevice;
struct D3d10ddiargCreateDeviceRecord;
struct D3d102ddiargGetCapsRecord;
struct D3d12ddiargCreateDeviceRecord;

class UmdHostDriver {
public:
    UmdHostDriver(const UmdHostDriver &) = delete;
    UmdHostDriver &operator=(const UmdHostDriver &) = delete;

    /* Makes the device `adopted` stands for, writing where it lives into the
     * extension slot of the runtime's block (DeviceTable::extension_offset) and
     * filling the runtime's device-functions table. Anything but hr_ok leaves
     * nothing made: the root forgets the adopted device and answers the same. */
    /* `kmt_adapter` is the D3DKMT handle of the adapter the device is on. */
    virtual Hresult build(UmdDevice &adopted, D3d10ddiargCreateDeviceRecord &create, uint32_t kmt_adapter) = 0;
    /* Direct3D 12's create: the same contract over D3D12Core's arguments, the
     * device's own tables coming later through fill_table12. */
    virtual Hresult build12(UmdDevice &adopted, D3d12ddiargCreateDeviceRecord &create, uint32_t kmt_adapter) = 0;
    /* Direct3D 9's create: d3d9.dll sizes no block, so the device is the
     * driver's own, its handle written into the D3DDDIARG_CREATEDEVICE
     * `create` points at (hDevice) with the device-functions table filled.
     * Anything but hr_ok leaves nothing made. */
    virtual Hresult build9(UmdAdapter &adapter, void *create, uint32_t kmt_adapter) = 0;
    /* d3d9.dll destroying the device `device` names, which build9 handed out:
     * the adapter it was made on, so its count can drop; null for a handle
     * this driver never handed out, which is left alone. */
    virtual UmdAdapter *tear_down9(void *device) = 0;
    /* One GetCaps question about `adapter`, answered in the caller's buffer;
     * the adapter's runtime says which DDI's question it is (the two records
     * are one shape). */
    virtual Hresult caps(UmdAdapter &adapter, D3d102ddiargGetCapsRecord &args, uint32_t kmt_adapter) = 0;
    /* One of the device tables D3D12Core asks an adapter for, filled into
     * the `bytes` at `table`. hr_invalid_arg for a table smaller than the
     * revision this driver fills; hr_not_impl for a table it has none of. */
    virtual Hresult fill_table12(uint32_t table_type, void *table, size_t bytes, uint32_t index,
                                 void *runtime_table) = 0;
    /* The runtime is destroying `device`, whose block this is. */
    virtual void    tear_down(UmdDevice &device, void *block) = 0;

protected:
    UmdHostDriver() = default;
    ~UmdHostDriver() = default;
};

}  // namespace av::d3d

#endif  // AV_D3D_UMD_HOST_DRIVER_HPP
