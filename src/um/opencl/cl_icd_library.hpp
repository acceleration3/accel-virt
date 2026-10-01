/* avcl.dll's ICD root and what it is constructed over: the process heap, the
 * two thread locks, the log file and D3DKMT through gdi32. No window surface
 * and no memory lock, because an OpenCL ICD presents nothing. One per process,
 * in the exports' storage (avcl_exports.cpp), constructed on the loader's
 * first call rather than at DLL attach, where the loader lock forbids opening
 * a file or an event. Translation only: what the root decides is in
 * src/icd/opencl. */
#ifndef AV_UM_CL_ICD_LIBRARY_HPP
#define AV_UM_CL_ICD_LIBRARY_HPP

#include "core/support/object_storage.hpp"
#include "icd/opencl/cl_icd_root.hpp"
#include "um/gdi_kmt.hpp"
#include "um/opencl/windows_gl_share_source.hpp"
#include "um/icd/file_log.hpp"
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

    /* Neither entry is serialised for us. The Vulkan loader holds loader_lock
     * across its driver scan and opengl32 calls DrvValidateVersion first under
     * its own lock, so those two ICDs construct on a call that cannot race;
     * the Khronos OpenCL loader promises nothing - khrIcdOsVendorsEnumerate
     * runs on whichever thread first reached clGetPlatformIDs, with nothing
     * held, and an application may reach it from several at once (research
     * 052 §3, gap 2). Hence the one-shot below. A process whose D3DKMT event
     * cannot be made gets no root, permanently, and the loader is told there
     * is no platform here. */
    cl_int platform_ids(cl_uint num_entries, cl_platform_id *platforms, cl_uint *num_platforms);
    void  *extension_function(const char *name);
    /* The process is crashing: the crash file, if tracing started. */
    long   crashed(void *pointers);

    /* The one-shot's body. Public because the callback InitOnceExecuteOnce
     * requires is handed this and nothing else. */
    void   construct();

private:
    struct Seams {
        Seams();
        Seams(const Seams &) = delete;
        Seams &operator=(const Seams &) = delete;

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

    ObjectStorage<Seams> seams_;
    /* An INIT_ONCE, which is one pointer and zero when untouched
     * (INIT_ONCE_STATIC_INIT), kept opaque so windows.h stays out of a header
     * the tests' include paths also reach. */
    void                *once_;
    ProcessTrace::Filter crashed_;
};

constexpr ClIcdLibrary::ClIcdLibrary(ProcessTrace::Filter crashed)
    : seams_(), once_(nullptr), crashed_(crashed)
{}

}  // namespace av

#endif  // AV_UM_CL_ICD_LIBRARY_HPP
