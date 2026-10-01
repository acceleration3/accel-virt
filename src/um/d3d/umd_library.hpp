/* avumd.dll's root and what it is constructed over: the process heap, its
 * lock, the log file and D3DKMT through gdi32. One per process, in the exports'
 * storage (avumd_exports.cpp), constructed on the first OpenAdapter call
 * rather than at DLL attach, where the loader lock forbids opening a file.
 * Translation only: what the root decides is in src/icd/d3d. */
#ifndef AV_UM_UMD_LIBRARY_HPP
#define AV_UM_UMD_LIBRARY_HPP

#include "core/support/object_storage.hpp"
#include "icd/d3d/hresult.hpp"
#include "icd/d3d/umd_root.hpp"
#include "um/d3d/umd_drivers.hpp"
#include "um/gdi_kmt.hpp"
#include "um/icd/file_log.hpp"
#include "um/icd/process_heap.hpp"
#include "um/icd/srw_lock.hpp"
#include "um/trace/process_trace.hpp"

namespace av {

struct D3d10ddiargOpenAdapterRecordForward;

class UmdLibrary {
public:
    /* `crashed` is the exports' unhandled-exception filter, which forwards to
     * crashed(). Constexpr: the library is the exports' constinit storage. */
    constexpr explicit UmdLibrary(ProcessTrace::Filter crashed);
    UmdLibrary(const UmdLibrary &) = delete;
    UmdLibrary &operator=(const UmdLibrary &) = delete;

    /* The two exports. A process whose log or D3DKMT cannot be opened gets no
     * root, and the runtime is told this driver cannot serve the adapter. */
    d3d::Hresult open_adapter10_2(void *args);
    d3d::Hresult open_adapter12(void *args);
    /* The process is crashing: the crash file, if tracing started. */
    long         crashed(void *pointers);

private:
    struct Seams {
        Seams();
        Seams(const Seams &) = delete;
        Seams &operator=(const Seams &) = delete;

        ProcessHeap   heap;
        SrwLock       lock;
        FileLog       log;
        GdiKmt        kmt;
        UmdDrivers    driver;
        d3d::UmdRoot  root;
        ProcessTrace  trace;
    };

    d3d::UmdRoot *root();

    ObjectStorage<Seams> seams_;
    bool                 refused_;
    bool                 started_trace_ = false;
    ProcessTrace::Filter crashed_;
};

constexpr UmdLibrary::UmdLibrary(ProcessTrace::Filter crashed) : seams_(), refused_(false), crashed_(crashed) {}

}  // namespace av

#endif  // AV_UM_UMD_LIBRARY_HPP
