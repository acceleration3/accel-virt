#include "um/d3d/umd_drivers.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/d3d/umd_adapter.hpp"
#include "icd/d3d/umd_device.hpp"

namespace av {

UmdDrivers::UmdDrivers(BlockSource &memory, Kmt &kmt, KeyedLog &log)
    : trace_(), d3d11_(memory, kmt, log, trace_), d3d12_(memory, kmt, log, trace_), d3d9_(memory, kmt, log, trace_)
{
    AV_SCOPE("UmdDrivers::UmdDrivers");
    trace_.open();
}

d3d::SharedRowsRequest &UmdDrivers::shared_rows()
{
    AV_SCOPE("UmdDrivers::shared_rows");
    return d3d11_.rows();
}

d3d::Hresult UmdDrivers::build(d3d::UmdDevice &adopted, d3d::D3d10ddiargCreateDeviceRecord &create,
                               uint32_t kmt_adapter)
{
    AV_SCOPE("UmdDrivers::build");
    return d3d11_.build(adopted, create, kmt_adapter);
}

d3d::Hresult UmdDrivers::build12(d3d::UmdDevice &, d3d::D3d12ddiargCreateDeviceRecord &create, uint32_t kmt_adapter)
{
    AV_SCOPE("UmdDrivers::build12");
    return d3d12_.build(create, kmt_adapter);
}

d3d::Hresult UmdDrivers::build9(d3d::UmdAdapter &adapter, void *create, uint32_t kmt_adapter)
{
    AV_SCOPE("UmdDrivers::build9");
    return d3d9_.build(adapter, create, kmt_adapter);
}

d3d::UmdAdapter *UmdDrivers::tear_down9(void *device)
{
    AV_SCOPE("UmdDrivers::tear_down9");
    return d3d9_.tear_down(device);
}

d3d::Hresult UmdDrivers::caps(d3d::UmdAdapter &adapter, d3d::D3d102ddiargGetCapsRecord &args, uint32_t kmt_adapter)
{
    AV_SCOPE("UmdDrivers::caps");
    if (adapter.runtime() == d3d::RuntimeKind::d3d12) return d3d12_.caps(args, kmt_adapter);
    if (adapter.runtime() == d3d::RuntimeKind::d3d9) return d3d9_.caps(args, kmt_adapter);
    return d3d11_.caps(adapter, args, kmt_adapter);
}

d3d::Hresult UmdDrivers::fill_table12(uint32_t table_type, void *table, size_t bytes, uint32_t index,
                                      void *runtime_table)
{
    AV_SCOPE("UmdDrivers::fill_table12");
    return d3d12_.fill_table(table_type, table, bytes, index, runtime_table);
}

void UmdDrivers::tear_down(d3d::UmdDevice &device, void *block)
{
    AV_SCOPE("UmdDrivers::tear_down");
    if (device.adapter().runtime() == d3d::RuntimeKind::d3d12) {
        d3d12_.tear_down(block);
        return;
    }
    d3d11_.tear_down(block);
}

}
