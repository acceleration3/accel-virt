/* avumd.dll's root and what it is constructed over: the process heap, its
 * lock, the log file and D3DKMT through gdi32. One per process, in the exports'
 * storage (avumd_exports.cpp), constructed on the first OpenAdapter call
 * rather than at DLL attach, where the loader lock forbids opening a file.
 * Translation only: what the root decides is in src/icd/d3d. */
#ifndef AV_UM_UMD_LIBRARY_HPP
#define AV_UM_UMD_LIBRARY_HPP

#include "icd/d3d/hresult.hpp"
#include "icd/d3d/umd_root.hpp"
#include "um/d3d/umd_drivers.hpp"
#include "um/kmt/gdi_kmt.hpp"
#include "um/common/file_log.hpp"
#include "um/common/lazy_driver.hpp"
#include "um/common/process_heap.hpp"
#include "um/common/srw_lock.hpp"
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

    /* The three exports. A process LazyDriver refuses gets no root, and the
     * runtime is told this driver cannot serve the adapter. */
    d3d::Hresult open_adapter10_2(void *args);
    d3d::Hresult open_adapter12(void *args);
    d3d::Hresult open_adapter9(void *args);
    /* The process is crashing: the crash file, if tracing started. */
    long         crashed(void *pointers);

private:
    struct Seams {
        Seams();
        Seams(const Seams &) = delete;
        Seams &operator=(const Seams &) = delete;
        /* The D3DKMT wait event the driver's waits need; without it the
         * process is refused (LazyDriver). */
        [[nodiscard]] Status initialize();

        ProcessHeap   heap;
        SrwLock       lock;
        FileLog       log;
        GdiKmt        kmt;
        UmdDrivers    driver;
        d3d::UmdRoot  root;
        ProcessTrace  trace;
    };

    d3d::UmdRoot *root();

    LazyDriver<Seams> driver_;
};

constexpr UmdLibrary::UmdLibrary(ProcessTrace::Filter crashed) : driver_("avumd: ", "avumd", crashed) {}

}  // namespace av

#endif  // AV_UM_UMD_LIBRARY_HPP
