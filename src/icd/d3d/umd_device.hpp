#ifndef AV_D3D_UMD_DEVICE_HPP
#define AV_D3D_UMD_DEVICE_HPP

#include "icd/d3d/dxgi_base_table.hpp"
#include "icd/d3d/hresult.hpp"

#include <stdint.h>

namespace av {
class StateWriter;
}


namespace av::d3d {

class UmdAdapter;
struct D3d10ddiargCreateDeviceRecord;

struct UmdDeviceProbe;

class UmdDevice {
public:
    explicit UmdDevice(UmdAdapter &adapter);
    UmdDevice(const UmdDevice &) = delete;
    UmdDevice &operator=(const UmdDevice &) = delete;

    Hresult adopt(const D3d10ddiargCreateDeviceRecord &create);

    UmdAdapter    &adapter();
    void          *runtime_device() const;
    void          *runtime_core_layer() const;
    const void    *kernel_callbacks() const;
    const void    *core_layer_callbacks() const;
    uint32_t       interface_version() const;
    DxgiBaseTable &dxgi();

    void report(StateWriter &writer, uint32_t index) const;

    bool invariants_hold() const;

private:
    UmdAdapter   &adapter_;
    void         *runtime_device_;
    void         *runtime_core_layer_;
    const void   *kernel_callbacks_;
    const void   *core_layer_callbacks_;
    uint32_t      interface_version_;
    uint32_t      runtime_version_;
    DxgiBaseTable dxgi_;

    friend struct UmdDeviceProbe;
};

}

#endif
