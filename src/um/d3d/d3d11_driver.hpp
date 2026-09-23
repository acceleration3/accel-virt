/* The Direct3D 11 half of rvkumd.dll that needs the host: the devices the
 * runtime makes, each a D3d11Device in this pool, and each adapter's host
 * capabilities, asked once. One per process, beside the root in the library's
 * storage, and injected into it. */
#ifndef AV_UM_D3D11_DRIVER_HPP
#define AV_UM_D3D11_DRIVER_HPP

#include "core/support/object_pool.hpp"
#include "icd/d3d/umd_host_driver.hpp"
#include "um/d3d/d3d11_device.hpp"
#include "um/d3d/d3d11_host_caps.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class Kmt;
}  // namespace av

namespace av::icd {
class LogSink;
}  // namespace av::icd

namespace av::d3d11 {

class D3d11Driver final : public d3d::UmdHostDriver {
public:
    D3d11Driver(BlockSource &memory, Kmt &kmt, icd::LogSink &log);

    d3d::Hresult build(d3d::UmdDevice &adopted, d3d::D3d10ddiargCreateDeviceRecord &create,
                       uint32_t kmt_adapter) override;
    d3d::Hresult caps(d3d::UmdAdapter &adapter, d3d::D3d102ddiargGetCapsRecord &args,
                      uint32_t kmt_adapter) override;
    void         tear_down(void *block) override;

private:
    /* The host's answers for this kernel adapter, asked on first use; null
     * when the adapter runs no Direct3D. */
    const D3d11HostCaps *caps_of(uint32_t kmt_adapter);
    /* One line of what the runtime asked and what it was answered. */
    void                 note(const char *what, uint64_t first, uint64_t second);

    static constexpr uint32_t max_adapters = 8u;

    BlockSource             &memory_;
    Kmt                     &kmt_;
    icd::LogSink            &log_;
    ObjectPool<D3d11Device>  devices_;
    D3d11HostCaps            caps_[max_adapters];
    uint32_t                 adapters_[max_adapters];
};

}  // namespace av::d3d11

#endif  // AV_UM_D3D11_DRIVER_HPP
