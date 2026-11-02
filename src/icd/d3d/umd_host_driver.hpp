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

    virtual Hresult build(UmdDevice &adopted, D3d10ddiargCreateDeviceRecord &create, uint32_t kmt_adapter) = 0;
    virtual Hresult build12(UmdDevice &adopted, D3d12ddiargCreateDeviceRecord &create, uint32_t kmt_adapter) = 0;
    virtual Hresult build9(UmdAdapter &adapter, void *create, uint32_t kmt_adapter) = 0;
    virtual UmdAdapter *tear_down9(void *device) = 0;
    virtual Hresult caps(UmdAdapter &adapter, D3d102ddiargGetCapsRecord &args, uint32_t kmt_adapter) = 0;
    virtual Hresult fill_table12(uint32_t table_type, void *table, size_t bytes, uint32_t index,
                                 void *runtime_table) = 0;
    virtual void    tear_down(UmdDevice &device, void *block) = 0;

protected:
    UmdHostDriver() = default;
    ~UmdHostDriver() = default;
};

}

#endif
