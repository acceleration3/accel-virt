/* The Direct3D 12 half of avumd.dll that needs the host: the devices
 * D3D12Core makes, each a D3d12Device in this pool, and for each adapter its
 * host capabilities, asked once, the one session its devices share, and where
 * in that session's arena their CPU-visible heaps are. One per process, in the
 * library's UmdDrivers beside the Direct3D 11 half; the root calls it under
 * its own lock. */
#ifndef AV_UM_D3D12_DRIVER_HPP
#define AV_UM_D3D12_DRIVER_HPP

#include "core/support/object_pool.hpp"
#include "core/support/object_storage.hpp"
#include "core/log/driver_log.hpp"
#include "icd/d3d/hresult.hpp"
#include "um/d3d/adapter_sessions.hpp"
#include "um/d3d/d3d12_device.hpp"
#include "um/d3d/d3d12_host_caps.hpp"
#include "um/d3d/d3d_session.hpp"
#include "um/common/srw_lock.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class LogSink;
}  // namespace av

namespace av {
class BlockSource;
class Kmt;
}  // namespace av


namespace av::d3d {
struct D3d102ddiargGetCapsRecord;
struct D3d12ddiargCreateDeviceRecord;
}  // namespace av::d3d

namespace av::d3d12 {

class D3d12Driver {
public:
    D3d12Driver(BlockSource &memory, Kmt &kmt, LogSink &log);
    D3d12Driver(const D3d12Driver &) = delete;
    D3d12Driver &operator=(const D3d12Driver &) = delete;

    d3d::Hresult build(d3d::D3d12ddiargCreateDeviceRecord &create, uint32_t kmt_adapter);
    d3d::Hresult caps(d3d::D3d102ddiargGetCapsRecord &args, uint32_t kmt_adapter);
    /* The device, command list, command queue and extended feature tables;
     * nothing else. */
    d3d::Hresult fill_table(uint32_t table_type, void *table, size_t bytes, uint32_t index, void *runtime_table);
    void         tear_down(void *block);

private:
    static constexpr uint32_t max_adapters = AdapterSessions<D3d12HostCaps>::max_adapters;

    /* The slot's session (AdapterSessions) and the heap arena over it. */
    d3d::D3dSession *session(uint32_t slot);

    BlockSource               &memory_;
    LogSink                   &log_;
    DriverLog                      lines_;
    ObjectPool<D3d12Device>         devices_;
    AdapterSessions<D3d12HostCaps>  adapters_;
    /* D3D12Core's handle for the command-list table this driver filled. */
    void                           *list_table_;
};

}  // namespace av::d3d12

#endif  // AV_UM_D3D12_DRIVER_HPP
