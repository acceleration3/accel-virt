/* rvkvk.dll's ICD root and what it is constructed over: the process heap, the
 * three thread locks, the log file and D3DKMT through gdi32. One per process, in
 * the exports' storage (rvkvk_exports.cpp), constructed on the loader's first
 * call rather than at DLL attach, where the loader lock forbids opening a file
 * or an event. Translation only: what the root decides is in src/icd. */
#ifndef AV_UM_ICD_LIBRARY_HPP
#define AV_UM_ICD_LIBRARY_HPP

#include "core/support/object_storage.hpp"
#include "icd/vulkan/icd_root.hpp"
#include "um/gdi_kmt.hpp"
#include "um/icd/file_log.hpp"
#include "um/icd/gdi_window_surface.hpp"
#include "um/icd/process_heap.hpp"
#include "um/icd/srw_lock.hpp"
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

    /* The loader calls both under its loader_lock while it scans drivers
     * (Vulkan-Loader bde79ad2dd83, loader.c loader_scanned_icd_add), so the
     * first call's construction is never raced. A process whose D3DKMT event
     * cannot be made gets no root, and the loader is told this is no driver. */
    VkResult           negotiate(uint32_t *version);
    PFN_vkVoidFunction instance_proc_addr(VkInstance instance, const char *name);
    /* The process is crashing: the crash file, if tracing started. */
    long               crashed(void *pointers);

private:
    struct Seams {
        Seams();
        Seams(const Seams &) = delete;
        Seams &operator=(const Seams &) = delete;

        ProcessHeap  heap;
        SrwLock      lock;
        SrwLock      send_lock;
        SrwLock      memory_lock;
        FileLog      log;
        GdiKmt       kmt;
        GdiWindowSurface windows;
        icd::IcdRoot root;
        ProcessTrace trace;
    };

    icd::IcdRoot *root();

    ObjectStorage<Seams> seams_;
    bool                 refused_;
    bool                 started_trace_ = false;
    ProcessTrace::Filter crashed_;
};

constexpr IcdLibrary::IcdLibrary(ProcessTrace::Filter crashed) : seams_(), refused_(false), crashed_(crashed) {}

}  // namespace av

#endif  // AV_UM_ICD_LIBRARY_HPP
