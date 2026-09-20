/* One runtime's open instance of the rvkgpu adapter (contracts/umd_interface.md).
 * A process holds one per OpenAdapter call: the D3D10/11 runtime and D3D12Core
 * each open their own, and the two are separate objects over one connection
 * and one identity.
 *
 * The adapter's own address is the handle handed back in `hAdapter`, so the
 * pool that made it is what validates a handle coming the other way.
 *
 * Closing while a device made through it is live is refused: the runtime
 * destroys devices first, and letting the adapter go would leave those devices
 * naming a parent that is gone. */
#ifndef AV_D3D_UMD_ADAPTER_HPP
#define AV_D3D_UMD_ADAPTER_HPP

#include "icd/d3d/adapter_identity.hpp"
#include "icd/d3d/ddi_version_list.hpp"
#include "icd/d3d/runtime_kind.hpp"

#include <stdint.h>

namespace av {
class StateWriter;
}

namespace av::d3d {

struct D3dddiAdapterCallbacksRecord;

struct UmdAdapterProbe;

class UmdAdapter {
public:
    UmdAdapter(RuntimeKind runtime, void *runtime_adapter, const D3dddiAdapterCallbacksRecord *callbacks,
               uint32_t interface_version, uint32_t runtime_version);
    UmdAdapter(const UmdAdapter &) = delete;
    UmdAdapter &operator=(const UmdAdapter &) = delete;

    RuntimeKind runtime() const;
    void       *runtime_adapter() const;
    /* The table the runtime gave, as it stands now: the runtime may move its
     * rows whenever no thread is inside the driver, so this is the pointer it
     * handed over and never a copy. */
    const D3dddiAdapterCallbacksRecord *callbacks() const;
    uint32_t                            interface_version() const;
    uint32_t                            runtime_version() const;
    const DdiVersionList               &versions() const;
    AdapterIdentity                    &identity();
    const AdapterIdentity              &identity() const;

    void     device_made();
    void     device_gone();
    uint32_t devices() const;

    void report(StateWriter &writer, uint32_t index) const;

    bool invariants_hold() const;

private:
    RuntimeKind                         runtime_;
    void                               *runtime_adapter_;
    const D3dddiAdapterCallbacksRecord *callbacks_;
    uint32_t                            interface_version_;
    uint32_t                            runtime_version_;
    uint32_t                            devices_;
    DdiVersionList                      versions_;
    AdapterIdentity                     identity_;

    friend struct UmdAdapterProbe;
};

}  // namespace av::d3d

#endif  // AV_D3D_UMD_ADAPTER_HPP
