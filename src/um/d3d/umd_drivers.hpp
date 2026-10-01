/* The host driver the root is built over (UmdHostDriver): the Direct3D 9,
 * 11 and 12 halves of avumd.dll, each asked by the runtime that opened the
 * adapter or made the device. One per process, in the library's
 * storage beside the root. */
#ifndef AV_UM_UMD_DRIVERS_HPP
#define AV_UM_UMD_DRIVERS_HPP

#include "icd/d3d/umd_host_driver.hpp"
#include "um/d3d/d3d11_driver.hpp"
#include "um/d3d/d3d9_driver.hpp"
#include "um/d3d/d3d12_driver.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class BlockSource;
class Kmt;
}  // namespace av

namespace av::icd {
class LogSink;
}  // namespace av::icd

namespace av {

class UmdDrivers final : public d3d::UmdHostDriver {
public:
    UmdDrivers(BlockSource &memory, Kmt &kmt, icd::LogSink &log);

    d3d::Hresult build(d3d::UmdDevice &adopted, d3d::D3d10ddiargCreateDeviceRecord &create,
                       uint32_t kmt_adapter) override;
    d3d::Hresult build12(d3d::UmdDevice &adopted, d3d::D3d12ddiargCreateDeviceRecord &create,
                         uint32_t kmt_adapter) override;
    d3d::Hresult build9(d3d::UmdAdapter &adapter, void *create, uint32_t kmt_adapter) override;
    d3d::UmdAdapter *tear_down9(void *device) override;
    d3d::Hresult caps(d3d::UmdAdapter &adapter, d3d::D3d102ddiargGetCapsRecord &args, uint32_t kmt_adapter) override;
    d3d::Hresult fill_table12(uint32_t table_type, void *table, size_t bytes, uint32_t index,
                              void *runtime_table) override;
    void         tear_down(d3d::UmdDevice &device, void *block) override;

private:
    d3d11::D3d11Driver d3d11_;
    d3d12::D3d12Driver d3d12_;
    d3d9::D3d9Driver   d3d9_;
};

}  // namespace av

#endif  // AV_UM_UMD_DRIVERS_HPP
