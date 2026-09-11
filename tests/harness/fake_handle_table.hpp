/* dxgkrnl's allocation handles: the D3DKMT_HANDLE each allocation gets, and
 * DxgkCbGetHandleData, which resolves one back to the driver's hAllocation.
 * That callback carries no device handle - dxgkrnl answers from the calling
 * thread's DDI - so the fake reaches the table through the one instance that is
 * active, like TestRegistry the harness's only process-lifetime state. Tests
 * are single-threaded; a second active table fails the test. */
#ifndef AV_FAKE_HANDLE_TABLE_HPP
#define AV_FAKE_HANDLE_TABLE_HPP

#include "../../src/core/ddi/dxgkargcb_get_handle_data_record.hpp"

#include <cstdint>

namespace av {

class FakeHandleTable {
public:
    static constexpr std::uint32_t capacity = 256u;

    FakeHandleTable();
    ~FakeHandleTable();
    FakeHandleTable(const FakeHandleTable &) = delete;
    FakeHandleTable &operator=(const FakeHandleTable &) = delete;

    /* A new non-zero D3DKMT_HANDLE for `driver_data`; 0 once full. */
    std::uint32_t insert(void *driver_data);
    void          remove(std::uint32_t handle);
    void         *lookup(const DxgkargcbGetHandleDataRecord &arguments) const;
    std::uint32_t lookups() const;

private:
    static constexpr std::uint32_t first_handle = 0x40000000u;
    static constexpr std::int32_t  handle_allocation = 1;

    void                 *data_[capacity];
    mutable std::uint32_t lookups_;
};

}  // namespace av

extern "C" void *AvFakeGetHandleData(const av::DxgkargcbGetHandleDataRecord *arguments);

#endif  // AV_FAKE_HANDLE_TABLE_HPP
