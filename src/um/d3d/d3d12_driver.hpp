#ifndef AV_UM_D3D12_DRIVER_HPP
#define AV_UM_D3D12_DRIVER_HPP

#include "core/support/object_pool.hpp"
#include "core/support/object_storage.hpp"
#include "core/log/driver_log.hpp"
#include "icd/d3d/hresult.hpp"
#include "um/d3d/adapter_sessions.hpp"
#include "um/d3d/d3d12_device.hpp"
#include "um/d3d/d3d12_host_caps.hpp"
#include "um/d3d/heap_slice_offer12.hpp"
#include "um/d3d/d3d_session.hpp"
#include "um/common/srw_lock.hpp"

#include <stddef.h>
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
struct D3d102ddiargGetCapsRecord;
struct D3d12ddiargCreateDeviceRecord;
}

namespace av::d3d12 {

class D3d12Driver {
public:
    D3d12Driver(BlockSource &memory, Kmt &kmt, KeyedLog &log, d3d::D3dCallTrace &trace);
    D3d12Driver(const D3d12Driver &) = delete;
    D3d12Driver &operator=(const D3d12Driver &) = delete;

    d3d::Hresult build(d3d::D3d12ddiargCreateDeviceRecord &create, uint32_t kmt_adapter);
    d3d::Hresult caps(d3d::D3d102ddiargGetCapsRecord &args, uint32_t kmt_adapter);
    d3d::Hresult fill_table(uint32_t table_type, void *table, size_t bytes, uint32_t index, void *runtime_table);
    void         tear_down(void *block);

private:
    static constexpr uint32_t max_adapters = AdapterSessions<D3d12HostCaps>::max_adapters;

    d3d::D3dSession *session(uint32_t slot);

    KeyedLog                     &lines_;
    d3d::D3dCallTrace             &trace_;
    ObjectPool<D3d12Device>         devices_;
    HeapSliceOffer12                offer_;
    AdapterSessions<D3d12HostCaps>  adapters_;
    void                           *list_table_;
};

}

#endif
