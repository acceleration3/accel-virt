/* avvk.dll's ICD root and what it is constructed over: the process heap, the
 * thread locks, the log file, D3DKMT through gdi32, the windows' GDI
 * surfaces and DXGI chains, and the present thread with its lock and wake
 * events. One per process, in
 * the exports' storage (avvk_exports.cpp), constructed on the loader's first
 * call rather than at DLL attach, where the loader lock forbids opening a file
 * or an event. Translation only: what the root decides is in src/icd. */
#ifndef AV_UM_ICD_LIBRARY_HPP
#define AV_UM_ICD_LIBRARY_HPP

#include "icd/vulkan/icd_root.hpp"
#include "um/common/driver_seams.hpp"
#include "um/common/dxgi_window_chains.hpp"
#include "um/common/gdi_window_surface.hpp"
#include "um/common/lazy_driver.hpp"
#include "um/common/srw_lock.hpp"
#include "um/common/windows_service_thread.hpp"
#include "um/common/windows_thread_wake.hpp"
#include "um/trace/process_trace.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {

class IcdLibrary {
public:
    /* `crashed` is the exports' unhandled-exception filter, which forwards to
     * crashed(). Constexpr: the library is the exports' constinit storage. */
    constexpr explicit IcdLibrary(ProcessTrace::Filter crashed);
    IcdLibrary(const IcdLibrary &) = delete;
    IcdLibrary &operator=(const IcdLibrary &) = delete;

    /* A process LazyDriver refuses gets no root, and the loader is told this
     * is no driver. */
    VkResult           negotiate(uint32_t *version);
    PFN_vkVoidFunction instance_proc_addr(VkInstance instance, const char *name);
    /* The process is crashing: the crash file, if tracing started. */
    long               crashed(void *pointers);

private:
    struct Seams {
        Seams();
        Seams(const Seams &) = delete;
        Seams &operator=(const Seams &) = delete;
        DriverSeams  process;
        SrwLock      lock;
        SrwLock      send_lock;
        SrwLock      memory_lock;
        GdiWindowSurface windows;
        SrwLock      chains_lock;
        /* Before the root, which closes its swapchains' chains as it goes. */
        DxgiWindowChains chains;
        /* Before the root, whose swapchains' frames the thread shows. */
        SrwLock              present_lock;
        WindowsServiceThread present_thread;
        WindowsThreadWake    present_handed;
        WindowsThreadWake    present_shown;
        icd::IcdRoot root;
    };


    LazyDriver<Seams> driver_;
};

constexpr IcdLibrary::IcdLibrary(ProcessTrace::Filter crashed) : driver_("avvk", crashed) {}

}  // namespace av

#endif  // AV_UM_ICD_LIBRARY_HPP
