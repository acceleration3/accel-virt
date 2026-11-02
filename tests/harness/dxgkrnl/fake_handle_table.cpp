#include "harness/dxgkrnl/fake_handle_table.hpp"

#include "harness/framework/test_registry.hpp"

namespace av {

namespace {

FakeHandleTable *active_table = nullptr;

}

FakeHandleTable::FakeHandleTable() : data_{}, lookups_(0u), outstanding_(0u)
{
    if (active_table != nullptr)
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "a second FakeHandleTable is active");
    active_table = this;
}

FakeHandleTable::~FakeHandleTable()
{
    if (outstanding_ != 0u)
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "handle data acquired and never released");
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

void *FakeHandleTable::acquire(const DxgkargcbGetHandleDataRecord &arguments, void **release_handle)
{
    lookups_++;
    *release_handle = nullptr;
    if (arguments.Type != handle_allocation || arguments.hObject < first_handle) return nullptr;
    const std::uint32_t index = arguments.hObject - first_handle;
    if (index >= capacity || data_[index] == nullptr) return nullptr;
    outstanding_++;
    *release_handle = &data_[index];
    return data_[index];
}

void FakeHandleTable::release(const DxgkargcbReleaseHandleDataRecord &arguments)
{
    if (outstanding_ == 0u || arguments.Type != handle_allocation || arguments.ReleaseHandle == nullptr) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "released handle data that was not acquired");
        return;
    }
    outstanding_--;
}

std::uint32_t FakeHandleTable::lookups() const { return lookups_; }
std::uint32_t FakeHandleTable::outstanding() const { return outstanding_; }

}

extern "C" void *AvFakeAcquireHandleData(const av::DxgkargcbGetHandleDataRecord *arguments, void **release_handle)
{
    if (av::active_table == nullptr) {
        *release_handle = nullptr;
        return nullptr;
    }
    return av::active_table->acquire(*arguments, release_handle);
}

extern "C" void AvFakeReleaseHandleData(av::DxgkargcbReleaseHandleDataRecord arguments)
{
    if (av::active_table != nullptr) av::active_table->release(arguments);
}
