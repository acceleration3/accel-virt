#ifndef AV_UM_ALLOCATION_LENDER_HPP
#define AV_UM_ALLOCATION_LENDER_HPP

#include "core/support/status.hpp"
#include "um/d3d/d3d11_ddi.hpp"

#include <stdint.h>

namespace av {
class HostAllocation;
}

namespace av::d3d {

class D3dSession;

class AllocationLender {
public:
    explicit AllocationLender(D3dSession &session);
    AllocationLender(const AllocationLender &) = delete;
    AllocationLender &operator=(const AllocationLender &) = delete;

    void   bind(const D3DDDI_DEVICECALLBACKS *callbacks, HANDLE runtime);
    [[nodiscard]] Status lend(D3DKMT_HANDLE allocation, uint64_t bytes, HostAllocation *&made) const;
    void  close(HostAllocation &made) const;
    void  retire(HostAllocation &made) const;

private:
    D3dSession                    &session_;
    const D3DDDI_DEVICECALLBACKS *callbacks_;
    HANDLE                         runtime_;
};

}

#endif
