#ifndef AV_UM_D3D9_DRIVER_HPP
#define AV_UM_D3D9_DRIVER_HPP

#include "core/log/driver_log.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/object_storage.hpp"
#include "icd/d3d/hresult.hpp"
#include "um/d3d/buffer_slice_offer9.hpp"
#include "um/d3d/adapter_sessions.hpp"
#include "um/d3d/d3d9_device.hpp"
#include "um/d3d/d3d9_host_caps.hpp"
#include "um/d3d/d3d_session.hpp"

#include <stdint.h>

namespace av {
class KeyedLog;
}

namespace av {
class BlockSource;
class Kmt;
}


namespace av::d3d {
class D3dCallTrace;
class UmdAdapter;
struct D3d102ddiargGetCapsRecord;
}

namespace av::d3d9 {

class D3d9Driver {
public:
    D3d9Driver(BlockSource &memory, Kmt &kmt, KeyedLog &log, d3d::D3dCallTrace &trace);
    D3d9Driver(const D3d9Driver &) = delete;
    D3d9Driver &operator=(const D3d9Driver &) = delete;

    d3d::Hresult     build(d3d::UmdAdapter &adapter, void *create, uint32_t kmt_adapter);
    d3d::Hresult     caps(d3d::D3d102ddiargGetCapsRecord &args, uint32_t kmt_adapter);
    d3d::UmdAdapter *tear_down(void *device);

private:
    uint32_t slot_of(uint32_t kmt_adapter);

    BlockSource                   &memory_;
    KeyedLog               &lines_;
    d3d::D3dCallTrace       &trace_;
    ObjectPool<D3d9Device>         devices_;
    BufferSliceOffer9              offer_;
    AdapterSessions<D3d9HostCaps>  adapters_;
};

}

#endif
