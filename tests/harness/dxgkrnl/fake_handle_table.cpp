#include "harness/dxgkrnl/fake_handle_table.hpp"

#include "harness/framework/test_registry.hpp"

namespace av {

namespace {

FakeHandleTable *active_table = nullptr;

}  // namespace

FakeHandleTable::FakeHandleTable() : data_{}, lookups_(0u)
{
    if (active_table != nullptr)
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "a second FakeHandleTable is active");
    active_table = this;
}

FakeHandleTable::~FakeHandleTable()
{
    if (active_table == this) active_table = nullptr;
}

std::uint32_t FakeHandleTable::insert(void *driver_data)
{
    for (std::uint32_t index = 0u; index < capacity; index++) {
        if (data_[index] != nullptr) continue;
        data_[index] = driver_data;
        return first_handle + index;
    }
    return 0u;
}

void FakeHandleTable::remove(std::uint32_t handle)
{
    if (handle >= first_handle && handle - first_handle < capacity) data_[handle - first_handle] = nullptr;
}

void *FakeHandleTable::lookup(const DxgkargcbGetHandleDataRecord &arguments) const
{
    lookups_++;
    if (arguments.Type != handle_allocation || arguments.hObject < first_handle) return nullptr;
    const std::uint32_t index = arguments.hObject - first_handle;
    return index < capacity ? data_[index] : nullptr;
}

std::uint32_t FakeHandleTable::lookups() const { return lookups_; }

}  // namespace av

extern "C" void *AvFakeGetHandleData(const av::DxgkargcbGetHandleDataRecord *arguments)
{ return av::active_table != nullptr ? av::active_table->lookup(*arguments) : nullptr; }
