#ifndef AV_UM_PAGING_QUEUE_HPP
#define AV_UM_PAGING_QUEUE_HPP

#include "um/d3d/d3d11_ddi.hpp"

#include <stdint.h>

namespace av {
class PollClock;
}

namespace av::d3d11 {

class PagingQueue {
public:
    PagingQueue(const D3DDDI_DEVICECALLBACKS &kernel, HANDLE device, PollClock &clock);
    ~PagingQueue();
    PagingQueue(const PagingQueue &) = delete;
    PagingQueue &operator=(const PagingQueue &) = delete;

    [[nodiscard]] HRESULT make_resident(D3DKMT_HANDLE allocation);

private:
    [[nodiscard]] HRESULT open();
    [[nodiscard]] HRESULT wait_for(uint64_t fence) const;

    const D3DDDI_DEVICECALLBACKS &kernel_;
    HANDLE                        device_;
    PollClock                    &clock_;
    D3DKMT_HANDLE                 queue_;
    const volatile uint64_t      *fence_;
};

}

#endif
