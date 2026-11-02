#include "um/d3d/presented_allocations.hpp"

#include "core/log/keyed_log.hpp"
#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/d3d/kernel_surface.hpp"
#include "um/d3d/kernel_device.hpp"

namespace av::d3d {

PresentedAllocations::PresentedAllocations(KernelDevice &kernel, KeyedLog &lines, PollClock &clock)
    : kernel_(kernel), lines_(lines), clock_(clock), paging_()
{
    AV_SCOPE("PresentedAllocations::PresentedAllocations");
}

PresentedAllocations::~PresentedAllocations()
{
    AV_SCOPE("PresentedAllocations::~PresentedAllocations");
    if (paging_.constructed()) paging_.destroy();
}

HRESULT PresentedAllocations::make(KernelSurface &surface, HANDLE resource, D3DKMT_HANDLE &allocation)
{
    AV_SCOPE("PresentedAllocations::make");
    const HRESULT made = kernel_.allocate(surface, resource, allocation);
    if (FAILED(made)) {
        if (lines_.first("the kernel refused a presented surface's allocation", made))
            LogLine(lines_)
                .text("the kernel refused a presented surface's allocation ")
                .hex(static_cast<uint32_t>(made), 8u)
                .write();
        return made;
    }
    if (surface.key() == 0u) lines_.once("the miniport minted no identity for a presented surface; nothing can open it");
    static_cast<void>(keep_resident(allocation));
    return S_OK;
}

void PresentedAllocations::give_back(D3DKMT_HANDLE allocation, HANDLE resource)
{
    AV_SCOPE("PresentedAllocations::give_back");
    const HRESULT freed = kernel_.deallocate(allocation, resource);
    if (FAILED(freed) && lines_.first("the kernel would not take an allocation back", freed))
        LogLine(lines_)
            .text("the kernel would not take an allocation back ")
            .hex(static_cast<uint32_t>(freed), 8u)
            .write();
}

HRESULT PresentedAllocations::keep_resident(D3DKMT_HANDLE allocation)
{
    AV_SCOPE("PresentedAllocations::keep_resident");
    if (!paging_.constructed()) static_cast<void>(paging_.construct(kernel_.callbacks(), kernel_.runtime(), clock_));
    const HRESULT resident = paging_.object().make_resident(allocation);
    if (FAILED(resident) && lines_.first("the kernel would not keep a presented surface resident", resident))
        LogLine(lines_)
            .text("the kernel would not keep a presented surface resident ")
            .hex(static_cast<uint32_t>(resident), 8u)
            .write();
    return resident;
}

HRESULT PresentedAllocations::keep_addressable(D3DKMT_HANDLE allocation, uint64_t pages, uint64_t &address)
{
    AV_SCOPE("PresentedAllocations::keep_addressable");
    address = 0u;
    const HRESULT resident = keep_resident(allocation);
    if (FAILED(resident)) return resident;
    const HRESULT mapped = paging_.object().map_address(allocation, pages, address);
    if (FAILED(mapped) && lines_.first("the kernel would not map a presented surface's GPU virtual address", mapped))
        LogLine(lines_)
            .text("the kernel would not map a presented surface's GPU virtual address ")
            .hex(static_cast<uint32_t>(mapped), 8u)
            .write();
    return mapped;
}

void PresentedAllocations::release_address(uint64_t address, uint64_t pages)
{
    AV_SCOPE("PresentedAllocations::release_address");
    if (address == 0u) return;
    const HRESULT freed = paging_.object().unmap_address(address, pages);
    if (FAILED(freed) && lines_.first("the kernel would not free a presented surface's GPU virtual address", freed))
        LogLine(lines_)
            .text("the kernel would not free a presented surface's GPU virtual address ")
            .hex(static_cast<uint32_t>(freed), 8u)
            .write();
}

HRESULT PresentedAllocations::make_context(HANDLE &context)
{
    AV_SCOPE("PresentedAllocations::make_context");
    const HRESULT made = kernel_.make_context(context);
    if (FAILED(made))
        LogLine(lines_).text("the kernel refused the device's context ").hex(static_cast<uint32_t>(made), 8u).write();
    return made;
}

}
