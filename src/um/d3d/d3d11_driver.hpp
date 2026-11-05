#ifndef AV_UM_D3D11_DRIVER_HPP
#define AV_UM_D3D11_DRIVER_HPP

#include "core/support/object_pool.hpp"
#include "core/support/object_storage.hpp"
#include "icd/d3d/hresult.hpp"
#include "icd/d3d/shared_rows_request.hpp"
#include "um/d3d/adapter_sessions.hpp"
#include "um/d3d/buffer_slice_offer11.hpp"
#include "um/d3d/d3d11_device.hpp"
#include "um/d3d/d3d11_host_caps.hpp"
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
class UmdAdapter;
class UmdDevice;
struct D3d10ddiargCreateDeviceRecord;
struct D3d102ddiargGetCapsRecord;
}

namespace av::d3d11 {

class D3d11Driver {
public:
    D3d11Driver(BlockSource &memory, Kmt &kmt, KeyedLog &log, d3d::D3dCallTrace &trace);

    d3d::Hresult build(d3d::UmdDevice &adopted, d3d::D3d10ddiargCreateDeviceRecord &create, uint32_t kmt_adapter);
    d3d::Hresult caps(d3d::UmdAdapter &adapter, d3d::D3d102ddiargGetCapsRecord &args, uint32_t kmt_adapter);
    void         tear_down(void *block);
    d3d::SharedRowsRequest &rows();

private:
    BlockSource             &memory_;
    KeyedLog           &lines_;
    d3d::D3dCallTrace       &trace_;
    d3d::SharedRowsRequest   rows_;
    ObjectPool<D3d11Device>  devices_;
    BufferSliceOffer11       offer_;
    AdapterSessions<D3d11HostCaps> adapters_;
};

}

#endif
