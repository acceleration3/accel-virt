/* rvkgl.dll's ICD root and what it is constructed over: the process heap, the
 * two thread locks, the log file, D3DKMT through gdi32, the application's
 * windows, thread-local storage, the loaded opengl32.dll, and the present
 * thread with the lock, D3DKMT waits and events of its own. One per process,
 * in the exports' storage (rvkgl_exports.cpp), constructed on opengl32's first
 * call rather than at DLL attach, where the loader lock forbids opening a file
 * or an event. Translation only: what the root decides is in src/icd/opengl,
 * reached through the entry points GlIcdBinding answers. */
#ifndef AV_UM_GL_ICD_LIBRARY_HPP
#define AV_UM_GL_ICD_LIBRARY_HPP

#include "core/support/object_storage.hpp"
#include "icd/opengl/gl_icd_root.hpp"
#include "icd/opengl/gl_present_loop.hpp"
#include "um/gdi_kmt.hpp"
#include "um/icd/file_log.hpp"
#include "um/icd/process_heap.hpp"
#include "um/icd/srw_lock.hpp"
#include "um/icd/gdi_window_surface.hpp"
#include "um/icd/windows_service_thread.hpp"
#include "um/icd/windows_wake_signal.hpp"
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

    /* opengl32 calls DrvValidateVersion first, under its own lock, so the
     * first call's construction is never raced. Every entry of a process whose
     * D3DKMT event cannot be made answers failure, and opengl32 then takes this
     * for no driver. */
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

        ProcessHeap       heap;
        SrwLock           lock;
        SrwLock           send_lock;
        FileLog           log;
        GdiKmt            kmt;
        GdiWindowSurface  surface;
        TlsThreadSlot     slot;
        LoadedOpengl32    opengl32;
        /* Before the root, which settles every presenter on it as it goes. */
        SrwLock              present_lock;
        GdiKmt               present_kmt;
        WindowsServiceThread present_thread;
        WindowsWakeSignal    present_handed;
        WindowsWakeSignal    present_shown;
        glicd::GlPresentLoop presents;
        glicd::GlIcdRoot  root;
        ProcessTrace      trace;
    };

    bool ready();

    ObjectStorage<Seams> seams_;
    bool                 refused_;
    bool                 started_trace_ = false;
    bool                 started_presents_ = false;
    ProcessTrace::Filter crashed_;
};

constexpr GlIcdLibrary::GlIcdLibrary(ProcessTrace::Filter crashed) : seams_(), refused_(false), crashed_(crashed) {}

}  // namespace av

#endif  // AV_UM_GL_ICD_LIBRARY_HPP
