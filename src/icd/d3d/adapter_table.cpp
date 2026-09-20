#include "icd/d3d/adapter_table.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av::d3d {

AdapterTable::AdapterTable(BlockSource &memory) : adapters_(memory), open_(0u)
{
    AV_SCOPE("AdapterTable::AdapterTable");
}

Status AdapterTable::open(UmdAdapter *&opened, RuntimeKind runtime, void *runtime_adapter,
                          const D3dddiAdapterCallbacksRecord *callbacks, uint32_t interface_version,
                          uint32_t runtime_version)
{
    AV_SCOPE("AdapterTable::open");
    opened = nullptr;
    UmdAdapter  *made = nullptr;
    const Status created =
        adapters_.create(made, runtime, runtime_adapter, callbacks, interface_version, runtime_version);
    if (created != Status::ok) return created;
    open_++;
    opened = made;
    return Status::ok;
}

UmdAdapter *AdapterTable::adapter_of(void *handle) const
{
    AV_SCOPE("AdapterTable::adapter_of");
    if (handle == nullptr) return nullptr;
    return adapters_.find(handle);
}

Status AdapterTable::close(void *handle)
{
    AV_SCOPE("AdapterTable::close");
    UmdAdapter *const adapter = adapter_of(handle);
    if (adapter == nullptr) return Status::invalid_parameter;
    if (adapter->devices() != 0u) return Status::conflict;
    const Status destroyed = adapters_.destroy(adapter);
    if (destroyed != Status::ok) return destroyed;
    open_--;
    return Status::ok;
}

uint32_t AdapterTable::open_adapters() const
{
    AV_SCOPE("AdapterTable::open_adapters");
    return open_;
}

void AdapterTable::report(StateWriter &writer) const
{
    AV_SCOPE("AdapterTable::report");
    uint32_t index = 0u;
    adapters_.for_each([&writer, &index](const UmdAdapter &adapter) {
        adapter.report(writer, index);
        index++;
    });
}

bool AdapterTable::invariants_hold() const
{
    AV_SCOPE("AdapterTable::invariants_hold");
    if (adapters_.live() != open_) return false;
    return adapters_.all_of([](const UmdAdapter &adapter) { return adapter.invariants_hold(); });
}

}  // namespace av::d3d
