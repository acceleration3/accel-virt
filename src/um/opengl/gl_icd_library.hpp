/* avgl.dll's ICD root and what it is constructed over: the process heap, the
 * two thread locks, the log file, D3DKMT through gdi32, the application's
 * windows, thread-local storage, the loaded opengl32.dll, and the present
 * thread with the lock, D3DKMT waits and events of its own. One per process,
 * in the exports' storage (avgl_exports.cpp), constructed on opengl32's first
 * call rather than at DLL attach, where the loader lock forbids opening a file
 * or an event. Translation only: what the root decides is in src/icd/opengl,
 * reached through the entry points GlIcdBinding answers. */
#ifndef AV_UM_GL_ICD_LIBRARY_HPP
#define AV_UM_GL_ICD_LIBRARY_HPP

#include "icd/opengl/gl_icd_root.hpp"
#include "icd/common/window_present_queue.hpp"
#include "icd/opengl/batch_ready.hpp"
#include "um/common/driver_seams.hpp"
#include "um/kmt/gdi_kmt.hpp"
#include "um/common/dxgi_window_chains.hpp"
#include "um/common/lazy_driver.hpp"
#include "um/common/srw_lock.hpp"
#include "um/common/gdi_window_surface.hpp"
#include "um/common/windows_service_thread.hpp"
#include "um/common/windows_thread_wake.hpp"
#include "um/opengl/loaded_opengl32.hpp"
#include "um/opengl/tls_thread_slot.hpp"
#include "um/trace/process_trace.hpp"

#include <stdint.h>

namespace av {

class GlIcdLibrary {
public:
    /* `crashed` is the exports' unhandled-exception filter, which forwards to
     * crashed(). Constexpr: the library is the exports' constinit storage. */
    constexpr explicit GlIcdLibrary(ProcessTrace::Filter crashed);
    GlIcdLibrary(const GlIcdLibrary &) = delete;
    GlIcdLibrary &operator=(const GlIcdLibrary &) = delete;

    /* Every entry of a process LazyDriver refuses answers failure, and
     * opengl32 then takes this for no driver. */
    int32_t   validate_version(uint32_t version);
    void      set_callback_procs(int32_t count, void *const *procs);
    int32_t   describe_pixel_format(void *dc, int32_t index, uint32_t bytes, uint8_t *descriptor);
    int32_t   set_pixel_format(void *dc, int32_t index);
    uintptr_t create_context(void *dc);
    uintptr_t create_layer_context(void *dc, int32_t layer);
    int32_t   delete_context(uintptr_t context);
    void     *set_context(void *dc, uintptr_t context, void *set_proc_table);
    int32_t   release_context(uintptr_t context);
    int32_t   copy_context(uintptr_t source, uintptr_t destination, uint32_t mask);
    int32_t   share_lists(uintptr_t first, uintptr_t second);
    void     *proc_address(const char *name);
    int32_t   describe_layer_plane(void *dc, int32_t format, int32_t plane, uint32_t bytes, void *descriptor);
    int32_t   swap_layer_buffers(void *dc, uint32_t planes);
    int32_t   swap_buffers(void *dc);
    /* The process is crashing: the crash file, if tracing started. */
    long      crashed(void *pointers);

private:
    struct Seams {
        Seams();
        Seams(const Seams &) = delete;
        Seams &operator=(const Seams &) = delete;
        DriverSeams       process;
        SrwLock           lock;
        SrwLock           send_lock;
        GdiWindowSurface  surface;
        TlsThreadSlot     slot;
        LoadedOpengl32    opengl32;
        /* Before the present loop, which presents through them, and the
         * root, whose presenters open and close them. */
        SrwLock              chains_lock;
        DxgiWindowChains     chains;
        /* Before the root, which settles every presenter on it as it goes. */
        SrwLock              present_lock;
        GdiKmt               present_kmt;
        WindowsServiceThread present_thread;
        WindowsThreadWake    present_handed;
        WindowsThreadWake    present_shown;
        glicd::BatchReady    present_ready;
        glicd::GlPresentQueue presents;
        glicd::GlIcdRoot  root;
    };

    bool ready();

    LazyDriver<Seams> driver_;
};

constexpr GlIcdLibrary::GlIcdLibrary(ProcessTrace::Filter crashed) : driver_("avgl: ", "avgl", crashed) {}

}  // namespace av

#endif  // AV_UM_GL_ICD_LIBRARY_HPP
