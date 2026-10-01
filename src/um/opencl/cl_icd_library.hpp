/* avcl.dll's ICD root and what it is constructed over: the process heap, the
 * two thread locks, the log file and D3DKMT through gdi32. No window surface
 * and no memory lock, because an OpenCL ICD presents nothing. One per process,
 * in the exports' storage (avcl_exports.cpp), constructed on the loader's
 * first call rather than at DLL attach, where the loader lock forbids opening
 * a file or an event. Translation only: what the root decides is in
 * src/icd/opencl. */
#ifndef AV_UM_CL_ICD_LIBRARY_HPP
#define AV_UM_CL_ICD_LIBRARY_HPP

#include "icd/opencl/cl_icd_root.hpp"
#include "um/gdi_kmt.hpp"
#include "um/opencl/windows_gl_share_source.hpp"
#include "um/icd/file_log.hpp"
#include "um/icd/lazy_driver.hpp"
#include "um/icd/process_heap.hpp"
#include "um/icd/srw_lock.hpp"
#include "um/icd/windows_service_thread.hpp"
#include "um/trace/process_trace.hpp"

#include <stdint.h>

#include <CL/cl.h>

namespace av {

class ClIcdLibrary {
public:
    /* `crashed` is the exports' unhandled-exception filter, which forwards to
     * crashed(). Constexpr: the library is the exports' constinit storage. */
    constexpr explicit ClIcdLibrary(ProcessTrace::Filter crashed);
    ClIcdLibrary(const ClIcdLibrary &) = delete;
    ClIcdLibrary &operator=(const ClIcdLibrary &) = delete;

    /* The Khronos loader may reach either from several threads at once:
     * khrIcdOsVendorsEnumerate runs on whichever first reached
     * clGetPlatformIDs, with nothing held. A process LazyDriver refuses gets
     * no root, and the loader is told there is no platform here. */
    cl_int platform_ids(cl_uint num_entries, cl_platform_id *platforms, cl_uint *num_platforms);
    void  *extension_function(const char *name);
    /* The process is crashing: the crash file, if tracing started. */
    long   crashed(void *pointers);


private:
    struct Seams {
        Seams();
        Seams(const Seams &) = delete;
        Seams &operator=(const Seams &) = delete;
        /* The D3DKMT wait event the driver's waits need; without it the
         * process is refused (LazyDriver). */
        [[nodiscard]] Status initialize();

        ProcessHeap      heap;
        SrwLock          lock;
        SrwLock          send_lock;
        FileLog          log;
        GdiKmt           kmt;
        /* Declared before the root, so it is stopped after the root has let
         * its watcher go. */
        WindowsServiceThread watcher;
        WindowsGlShareSource gl_share;
        clicd::ClIcdRoot root;
        ProcessTrace     trace;
    };

    clicd::ClIcdRoot *root();

    LazyDriver<Seams> driver_;
};

constexpr ClIcdLibrary::ClIcdLibrary(ProcessTrace::Filter crashed) : driver_("avcl: ", "avcl", crashed) {}

}  // namespace av

#endif  // AV_UM_CL_ICD_LIBRARY_HPP
