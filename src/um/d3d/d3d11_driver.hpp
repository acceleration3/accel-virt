/* The Direct3D 11 half of avumd.dll that needs the host: the devices the
 * runtime makes, each a D3d11Device in this pool, and for each adapter its
 * host capabilities, asked once, and the one session its devices share. One
 * per process, in the library's UmdDrivers beside the Direct3D 12 half; the
 * root calls it under its own lock. */
#ifndef AV_UM_D3D11_DRIVER_HPP
#define AV_UM_D3D11_DRIVER_HPP

#include "core/support/object_pool.hpp"
#include "core/support/object_storage.hpp"
#include "icd/d3d/hresult.hpp"
#include "um/d3d/d3d11_device.hpp"
#include "um/d3d/d3d11_host_caps.hpp"
#include "um/d3d/d3d_session.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class Kmt;
}  // namespace av

namespace av::icd {
class LogSink;
}  // namespace av::icd

namespace av::d3d {
class UmdAdapter;
class UmdDevice;
struct D3d10ddiargCreateDeviceRecord;
struct D3d102ddiargGetCapsRecord;
}  // namespace av::d3d

namespace av::d3d11 {

class D3d11Driver {
public:
    D3d11Driver(BlockSource &memory, Kmt &kmt, icd::LogSink &log);

    d3d::Hresult build(d3d::UmdDevice &adopted, d3d::D3d10ddiargCreateDeviceRecord &create, uint32_t kmt_adapter);
    d3d::Hresult caps(d3d::UmdAdapter &adapter, d3d::D3d102ddiargGetCapsRecord &args, uint32_t kmt_adapter);
    void         tear_down(void *block);

private:
    /* The slot of this kernel adapter, its host capabilities asked on first
     * use; max_adapters when the adapter runs no Direct3D or none is free. */
    uint32_t             slot_of(uint32_t kmt_adapter);
    /* The slot's session, made on first use. */
    d3d::D3dSession        &session(uint32_t slot);

    static constexpr uint32_t max_adapters = 8u;

    BlockSource             &memory_;
    Kmt                     &kmt_;
    icd::LogSink            &log_;
    ObjectPool<D3d11Device>  devices_;
    D3d11HostCaps            caps_[max_adapters];
    ObjectStorage<d3d::D3dSession> sessions_[max_adapters];
    uint32_t                 adapters_[max_adapters];
};

}  // namespace av::d3d11

#endif  // AV_UM_D3D11_DRIVER_HPP
