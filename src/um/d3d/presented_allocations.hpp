#ifndef AV_UM_PRESENTED_ALLOCATIONS_HPP
#define AV_UM_PRESENTED_ALLOCATIONS_HPP

#include "core/support/object_storage.hpp"
#include "um/d3d/d3d11_ddi.hpp"
#include "um/d3d/paging_queue.hpp"

namespace av {
class KeyedLog;
class PollClock;
}

namespace av::d3d {

class KernelDevice;
class KernelSurface;

class PresentedAllocations {
public:
    PresentedAllocations(KernelDevice &kernel, KeyedLog &lines, PollClock &clock);
    ~PresentedAllocations();
    PresentedAllocations(const PresentedAllocations &) = delete;
    PresentedAllocations &operator=(const PresentedAllocations &) = delete;
    PresentedAllocations(PresentedAllocations &&) = delete;
    PresentedAllocations &operator=(PresentedAllocations &&) = delete;

    [[nodiscard]] HRESULT make(KernelSurface &surface, HANDLE resource, D3DKMT_HANDLE &allocation);
    void give_back(D3DKMT_HANDLE allocation, HANDLE resource);
    [[nodiscard]] HRESULT keep_resident(D3DKMT_HANDLE allocation);
    [[nodiscard]] HRESULT keep_addressable(D3DKMT_HANDLE allocation, uint64_t pages, uint64_t &address);
    void release_address(uint64_t address, uint64_t pages);
    [[nodiscard]] HRESULT make_context(HANDLE &context);

private:
    KernelDevice                      &kernel_;
    KeyedLog                          &lines_;
    PollClock                         &clock_;
    ObjectStorage<d3d11::PagingQueue> paging_;
};

}

#endif
