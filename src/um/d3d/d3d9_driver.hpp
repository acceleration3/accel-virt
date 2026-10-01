/* The Direct3D 9 half of avumd.dll: the devices d3d9.dll makes, each a
 * D3d9Device in this pool, and for each adapter its host capabilities, asked
 * once, and the one session its devices share. One per process, in the
 * library's UmdDrivers beside the Direct3D 11 and 12 halves; the root calls
 * it under its own lock.
 *
 * d3d9.dll makes several devices per process - one for each DirectDraw
 * object it opens to enumerate, validate and create (reverse-engineering.md
 * §6) - so nothing here assumes one. */
#ifndef AV_UM_D3D9_DRIVER_HPP
#define AV_UM_D3D9_DRIVER_HPP

#include "core/support/object_pool.hpp"
#include "core/support/object_storage.hpp"
#include "icd/d3d/hresult.hpp"
#include "um/d3d/d3d9_device.hpp"
#include "um/d3d/d3d9_host_caps.hpp"
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
struct D3d102ddiargGetCapsRecord;
}  // namespace av::d3d

namespace av::d3d9 {

class D3d9Driver {
public:
    D3d9Driver(BlockSource &memory, Kmt &kmt, icd::LogSink &log);
    D3d9Driver(const D3d9Driver &) = delete;
    D3d9Driver &operator=(const D3d9Driver &) = delete;

    /* `create` is the runtime's D3DDDIARG_CREATEDEVICE. */
    d3d::Hresult     build(d3d::UmdAdapter &adapter, void *create, uint32_t kmt_adapter);
    d3d::Hresult     caps(d3d::D3d102ddiargGetCapsRecord &args, uint32_t kmt_adapter);
    /* The adapter the device was made on; null for a handle that is not one
     * of this driver's devices. */
    d3d::UmdAdapter *tear_down(void *device);

private:
    static constexpr uint32_t max_adapters = 8u;

    /* The slot of this kernel adapter, its host capabilities asked on first
     * use; max_adapters when the adapter runs no Direct3D 9 or none is free. */
    uint32_t         slot_of(uint32_t kmt_adapter);
    d3d::D3dSession &session(uint32_t slot);

    BlockSource                   &memory_;
    Kmt                           &kmt_;
    icd::LogSink                  &log_;
    ObjectPool<D3d9Device>         devices_;
    D3d9HostCaps                   caps_[max_adapters];
    ObjectStorage<d3d::D3dSession> sessions_[max_adapters];
    uint32_t                       adapters_[max_adapters];
};

}  // namespace av::d3d9

#endif  // AV_UM_D3D9_DRIVER_HPP
