/* One DxgkCbAcquireHandleData acquisition: the driver's data behind a
 * D3DKMT allocation handle, borrowed until the release handle is handed back.
 * The destructor releases it, so no caller repeats that on every path. */
#ifndef AV_ACQUIRED_HANDLE_DATA_HPP
#define AV_ACQUIRED_HANDLE_DATA_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class DxgkrnlInterface;

class AcquiredHandleData {
public:
    AcquiredHandleData(DxgkrnlInterface &dxgkrnl, uint32_t allocation_handle);
    ~AcquiredHandleData();
    AcquiredHandleData(const AcquiredHandleData &) = delete;
    AcquiredHandleData &operator=(const AcquiredHandleData &) = delete;

    /* invalid_parameter when dxgkrnl knows no such handle: STATUS_NOT_FOUND is
     * a status dxgkrnl logs as "Driver returned an invalid NTSTATUS code" and
     * hands to the caller, which is what made D3DKMTCreateAllocation2 fail with
     * it (feature 037). */
    [[nodiscard]] Status acquire();
    /* Null until acquire() answers ok. */
    void                *data() const;

    bool invariants_hold() const;

private:
    DxgkrnlInterface &dxgkrnl_;
    uint32_t          allocation_handle_;
    void             *data_;
    void             *release_handle_;

    friend struct AcquiredHandleDataProbe;
};

}  // namespace av

#endif  // AV_ACQUIRED_HANDLE_DATA_HPP
