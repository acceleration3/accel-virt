#include "icd/d3d/umd_adapter.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/d3d/ddi/d3dddi_adapter_callbacks_record.hpp"

namespace av::d3d {

UmdAdapter::UmdAdapter(RuntimeKind runtime, void *runtime_adapter, const D3dddiAdapterCallbacksRecord *callbacks,
                       UmdConnection &connection, uint32_t interface_version, uint32_t runtime_version)
    : runtime_(runtime), runtime_adapter_(runtime_adapter), callbacks_(callbacks), connection_(connection),
      interface_version_(interface_version), runtime_version_(runtime_version), devices_(0u), versions_(runtime)
{
    AV_SCOPE("UmdAdapter::UmdAdapter");
}

RuntimeKind UmdAdapter::runtime() const
{
    AV_SCOPE("UmdAdapter::runtime");
    return runtime_;
}

void *UmdAdapter::runtime_adapter() const
{
    AV_SCOPE("UmdAdapter::runtime_adapter");
    return runtime_adapter_;
}

const D3dddiAdapterCallbacksRecord *UmdAdapter::callbacks() const
{
    AV_SCOPE("UmdAdapter::callbacks");
    return callbacks_;
}

UmdConnection &UmdAdapter::connection() const
{
    AV_SCOPE("UmdAdapter::connection");
    return connection_;
}

uint32_t UmdAdapter::interface_version() const
{
    AV_SCOPE("UmdAdapter::interface_version");
    return interface_version_;
}

uint32_t UmdAdapter::runtime_version() const
{
    AV_SCOPE("UmdAdapter::runtime_version");
    return runtime_version_;
}

const DdiVersionList &UmdAdapter::versions() const
{
    AV_SCOPE("UmdAdapter::versions");
    return versions_;
}

void UmdAdapter::device_made()
{
    AV_SCOPE("UmdAdapter::device_made");
    devices_++;
}

void UmdAdapter::device_gone()
{
    AV_SCOPE("UmdAdapter::device_gone");
    if (devices_ == 0u) return;
    devices_--;
}

uint32_t UmdAdapter::devices() const
{
    AV_SCOPE("UmdAdapter::devices");
    return devices_;
}

void UmdAdapter::report(StateWriter &writer, uint32_t index) const
{
    AV_SCOPE("UmdAdapter::report");
    writer.open("adapter", index);
    writer.number("runtime", static_cast<uint64_t>(runtime_));
    writer.hex("interface", interface_version_);
    writer.number("devices", devices_);
    writer.close();
}

bool UmdAdapter::invariants_hold() const
{
    AV_SCOPE("UmdAdapter::invariants_hold");
    if (callbacks_ == nullptr) return false;
    if (!versions_.invariants_hold()) return false;
    if (runtime_ == RuntimeKind::d3d10_11 && !versions_.serves(interface_version_)) return false;
    if (runtime_ == RuntimeKind::d3d12 && interface_version_ != 0u) return false;
    return true;
}

}
