#include "harness/icd/scripted_d3d_adapter_source.hpp"

namespace av {

ScriptedD3dAdapterSource::ScriptedD3dAdapterSource()
    : adapter_(nullptr), adapter_luid_(0u), device_(nullptr), device_luid_(0u), asked_(0u)
{
}

void ScriptedD3dAdapterSource::name_adapter(void *adapter, std::uint64_t luid)
{
    adapter_ = adapter;
    adapter_luid_ = luid;
}

void ScriptedD3dAdapterSource::name_device(void *device, std::uint64_t luid)
{
    device_ = device;
    device_luid_ = luid;
}

std::uint64_t ScriptedD3dAdapterSource::adapter_luid(void *dxgi_adapter)
{
    ++asked_;
    return dxgi_adapter != nullptr && dxgi_adapter == adapter_ ? adapter_luid_ : 0u;
}

std::uint64_t ScriptedD3dAdapterSource::device_luid(void *d3d10_device)
{
    ++asked_;
    return d3d10_device != nullptr && d3d10_device == device_ ? device_luid_ : 0u;
}

std::uint32_t ScriptedD3dAdapterSource::asked() const { return asked_; }

}
