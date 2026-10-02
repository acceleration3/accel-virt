/* dxgkrnl's allocation handles: the D3DKMT_HANDLE each allocation gets, and
 * DxgkCbAcquireHandleData, which resolves one back to the driver's hAllocation
 * and lends it until DxgkCbReleaseHandleData hands the release handle back. An
 * acquisition still outstanding when the table is destroyed fails the test, and
 * so does a release of what was never acquired: both are how the guest punishes
 * a driver that borrows handle data and forgets it.
 * That callback carries no device handle - dxgkrnl answers from the calling
 * thread's DDI - so the fake reaches the table through the one instance that is
 * active, like TestRegistry the harness's only process-lifetime state. Tests
 * are single-threaded; a second active table fails the test. */
#ifndef AV_FAKE_HANDLE_TABLE_HPP
#define AV_FAKE_HANDLE_TABLE_HPP

#include "miniport/ddi/dxgkargcb_get_handle_data_record.hpp"
#include "miniport/ddi/dxgkargcb_release_handle_data_record.hpp"

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
    /* Answers the data and a release handle, or null and no release handle. */
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

}  // namespace av

extern "C" void *AvFakeAcquireHandleData(const av::DxgkargcbGetHandleDataRecord *arguments, void **release_handle);
extern "C" void AvFakeReleaseHandleData(av::DxgkargcbReleaseHandleDataRecord arguments);

#endif  // AV_FAKE_HANDLE_TABLE_HPP
