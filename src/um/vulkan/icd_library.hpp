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
    constexpr explicit IcdLibrary(ProcessTrace::Filter crashed);
    IcdLibrary(const IcdLibrary &) = delete;
    IcdLibrary &operator=(const IcdLibrary &) = delete;

    VkResult           negotiate(uint32_t *version);
    PFN_vkVoidFunction instance_proc_addr(VkInstance instance, const char *name);
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
        DxgiWindowChains chains;
        SrwLock              present_lock;
        WindowsServiceThread present_thread;
        WindowsThreadWake    present_handed;
        WindowsThreadWake    present_shown;
        icd::IcdRoot root;
    };


    LazyDriver<Seams> driver_;
};

constexpr IcdLibrary::IcdLibrary(ProcessTrace::Filter crashed) : driver_("avvk", crashed) {}

}

#endif
