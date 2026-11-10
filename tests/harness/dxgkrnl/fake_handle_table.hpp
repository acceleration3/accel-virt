#ifndef AV_FAKE_HANDLE_TABLE_HPP
#define AV_FAKE_HANDLE_TABLE_HPP

#include "miniport/ddi/dxgkargcb_get_handle_data_record.hpp"
#include "miniport/ddi/dxgkargcb_release_handle_data_record.hpp"

#include <cstdint>

namespace av {

class FakeHandleTable {
public:
    static constexpr std::uint32_t capacity = 1024u;

    FakeHandleTable();
    ~FakeHandleTable();
    FakeHandleTable(const FakeHandleTable &) = delete;
    FakeHandleTable &operator=(const FakeHandleTable &) = delete;

    std::uint32_t insert(void *driver_data);
    void          remove(std::uint32_t handle);
    void         *acquire(const DxgkargcbGetHandleDataRecord &arguments, void **release_handle);
    void          release(const DxgkargcbReleaseHandleDataRecord &arguments);
    std::uint32_t lookups() const;
    std::uint32_t outstanding() const;

private:
    static constexpr std::uint32_t first_handle = 0x40000000u;
    static constexpr std::int32_t  handle_allocation = 1;

    void         *data_[capacity];
    std::uint32_t lookups_;
    std::uint32_t outstanding_;
};

}

extern "C" void *AvFakeAcquireHandleData(const av::DxgkargcbGetHandleDataRecord *arguments, void **release_handle);
extern "C" void AvFakeReleaseHandleData(av::DxgkargcbReleaseHandleDataRecord arguments);

#endif
