#include "um/d3d/presented_allocations.hpp"

#include "core/log/keyed_log.hpp"
#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/d3d/kernel_surface.hpp"
#include "um/d3d/kernel_device.hpp"

namespace av::d3d {

PresentedAllocations::PresentedAllocations(KernelDevice &kernel, KeyedLog &lines, const Kmt &kmt)
    : kernel_(kernel), lines_(lines), kmt_(kmt), paging_()
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
    /* An identity is asked for, which the miniport mints and dxgkrnl hands
     * whoever opens the surface: with none, nothing can open it. */
    if (surface.key() == 0u) lines_.once("the miniport minted no identity for a presented surface; nothing can open it");
    if (!paging_.constructed()) static_cast<void>(paging_.construct(kernel_.callbacks(), kernel_.runtime(), kmt_));
    const HRESULT resident = paging_.object().make_resident(allocation);
    if (FAILED(resident) && lines_.first("the kernel would not keep a presented surface resident", resident))
        LogLine(lines_)
            .text("the kernel would not keep a presented surface resident ")
            .hex(static_cast<uint32_t>(resident), 8u)
            .write();
    return S_OK;
}

HRESULT PresentedAllocations::make_context(HANDLE &context)
{
    AV_SCOPE("PresentedAllocations::make_context");
    const HRESULT made = kernel_.make_context(context);
    if (FAILED(made))
        LogLine(lines_).text("the kernel refused the device's context ").hex(static_cast<uint32_t>(made), 8u).write();
    return made;
}

}  // namespace av::d3d
