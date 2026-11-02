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

    [[nodiscard]] Status acquire();
    void                *data() const;

    bool invariants_hold() const;

private:
    DxgkrnlInterface &dxgkrnl_;
    uint32_t          allocation_handle_;
    void             *data_;
    void             *release_handle_;

    friend struct AcquiredHandleDataProbe;
};

}

#endif
