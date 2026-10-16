/* avgl.dll's decisions (research.md §1-§4, contracts/icd_interface.md): what
 * each Drv* entry, each WGL extension function and each generated entry's
 * hook answers. One per process - in the exports' storage in the DLL, the one
 * under test in a test - bound to GlIcdBinding for its life.
 *
 * Nothing reaches the device before a context is made: a context is made at
 * its first DrvSetContext, or at once for wglCreateContextAttribsARB, whose
 * errors the call itself must report. The process's device is opened with the
 * first context made and closed with the last one deleted. A GL call takes no
 * root lock: it reaches the calling thread's current context, which no other
 * thread may delete. */
#ifndef AV_GLICD_GL_ICD_ROOT_HPP
#define AV_GLICD_GL_ICD_ROOT_HPP

#include "core/log/keyed_log.hpp"
#include "core/trace/state_reporter.hpp"
#include "icd/opengl/current_contexts.hpp"
#include "icd/common/icd_connection.hpp"
#include "icd/common/refused_calls.hpp"
#include "icd/common/shared_handles.hpp"
#include "icd/opengl/gl_context_table.hpp"
#include "icd/opengl/gl_extension_policy.hpp"
#include "icd/opengl/manual_calls.hpp"
#include "icd/opengl/pixel_format_query.hpp"
#include "icd/opengl/pixel_format_table.hpp"
#include "icd/opengl/window_table.hpp"

#include "gl_icd_table.h"
#include "icd/opengl/gl_icd_binding.hpp"

#include <stdint.h>

namespace av {
class LogSink;
}  // namespace av

namespace av {
class BlockSource;
class ClientTransport;
class Kmt;
class StateWriter;
}  // namespace av

namespace av::icd {
class ThreadLock;
}  // namespace av::icd

namespace av::icd {
class WindowSurface;
}  // namespace av::icd

namespace av::glicd {

class GlPresentLoop;
class SystemOpengl;
class ThreadSlot;
struct GlIcdRootProbe;

class GlIcdRoot final : public StateReporter {
public:
    /* The version opengl32 asks DrvValidateVersion about for this ICD. */
    static constexpr uint32_t icd_version = 2u;
    static constexpr uint32_t max_callbacks = 9u;
    /* opengl32's callback table order (research.md §1). */
    enum : uint32_t { callback_get_dhglrc = 2u };
    enum : uint32_t {
        error_invalid_handle = 6u, error_not_enough_memory = 8u, error_not_supported = 50u,
        error_invalid_parameter = 87u, error_busy = 170u, error_invalid_pixel_format = 2000u,
        error_invalid_operation = 4317u, error_invalid_version_arb = 0x2095u, error_invalid_profile_arb = 0x2096u,
        error_invalid_data = 13u,
    };
    enum : uint32_t { gl_invalid_operation = 0x0502u, gl_invalid_enum = 0x0500u, gl_invalid_value = 0x0501u };
    static constexpr uint32_t swap_main_plane = 1u;

    GlIcdRoot(BlockSource &memory, Kmt &kmt, icd::ThreadLock &lock, icd::ThreadLock &send_lock, LogSink &log,
              icd::WindowSurface &surface, ThreadSlot &slot, SystemOpengl &system, GlPresentLoop &presents,
              GlIcdBinding &binding);
    ~GlIcdRoot();
    GlIcdRoot(const GlIcdRoot &) = delete;
    GlIcdRoot &operator=(const GlIcdRoot &) = delete;

    /* The Drv* entries, in contracts/icd_interface.md's order. A DC and an
     * HGLRC are opaque; a DHGLRC is a GlContextTable handle. */
    bool     validate_version(uint32_t version);
    void     set_callback_procs(int32_t count, void *const *procs);
    int32_t  describe_pixel_format(void *dc, int32_t index, uint32_t bytes, uint8_t *descriptor);
    bool     set_pixel_format(void *dc, int32_t index);
    uint64_t create_context(void *dc);
    uint64_t create_layer_context(void *dc, int32_t layer);
    bool     delete_context(uint64_t handle);
    /* The proc table opengl32 installs for the thread, or null. */
    void    *set_context(void *dc, uint64_t handle, void *set_proc_table);
    bool     release_context(uint64_t handle);
    bool     copy_context(uint64_t source, uint64_t destination, uint32_t mask);
    bool     share_lists(uint64_t first, uint64_t second);
    void    *get_proc_address(const char *name);
    bool     describe_layer_plane(void *dc, int32_t format, int32_t plane, uint32_t bytes, void *descriptor);
    bool     swap_layer_buffers(void *dc, uint32_t planes);
    bool     swap_buffers(void *dc);

    /* WGL_ARB_create_context(_profile): opengl32's HGLRC, or null with the
     * ARB's error set. */
    void       *create_context_attribs(void *dc, void *share, const int32_t *attributes);
    const char *extensions_string(void *dc);
    /* WGL_ARB_pixel_format: PixelFormatQuery's answers, false with the ARB's
     * error set for its refusals. A DC with no window has no format to
     * describe and chooses none. */
    bool        pixel_format_attribs(void *dc, int32_t format, int32_t layer, uint32_t count, const int32_t *names,
                                     int32_t *ints, float *floats);
    /* glFlush: a single-buffered pixel format has no back buffer to swap, so
     * the flush is what puts the frame on the window, as it does on Windows
     * (feature 051, US2 edge case) - unpaced: the swap interval counts swaps,
     * and an application flushing between its draws (Photoshop CS6 does,
     * hundreds of times as its first canvas is made) would otherwise wait a
     * vertical blank for every one. */
    void        flush_current();
    /* wglSwapIntervalEXT: false for an interval no extension this ICD reports
     * defines, or with no current context, with the error set. */
    bool        set_swap_interval(int32_t interval);
    /* wglGetSwapIntervalEXT: 0 with no current context. */
    int32_t     swap_interval();
    bool        choose_pixel_format(void *dc, const int32_t *ints, const float *floats, uint32_t max,
                                    int32_t *formats, uint32_t *count);

    /* What the entries and hooks reach: the calling thread's context. */
    GuestGlContext  *current();
    ManualCalls     &manual();
    ClientTransport *wire();
    /* A GL 1.1 command the core profile lacks: GL_INVALID_OPERATION on the
     * current context, no host call; 0, what a legacy query answers. */
    uintptr_t        record_legacy();
    /* glGetError: the current context's own error first, then the host's; 0
     * with nothing current or the context lost. */
    uint32_t         get_error();
    /* glGetGraphicsResetStatus: the host's answer, or a lost context's own
     * (GuestGlContext::lost_reset_status); 0 with nothing current. */
    uint32_t         reset_status();
    /* The probe's avForceContextFault (T040): the calling thread's context
     * sent a call its host cannot decode. 1 when the context is lost after
     * it (or was already), 0 when the host answered; 0 with
     * ERROR_INVALID_OPERATION with nothing current. */
    int32_t          force_context_fault();
    /* The calling thread's context's share name - its ring allocation's
     * D3DKMT handle, by which a context of this process made by another ICD
     * names it to share its objects (cl_khr_gl_sharing); 0 with nothing
     * current or a context not yet made. */
    uint32_t         current_share_name();
    /* The OpenCL ICD's other question (cl_khr_gl_sharing): the calling
     * thread's context's held calls are run by the host before an OpenCL
     * context names what they made - a texture glGenTextures named but whose
     * bind is still held does not exist on the host yet. 1 when they ran (or
     * nothing was held), 0 with nothing current or the context lost. */
    int32_t          run_held_calls();
    uint64_t         mint_sync_token();
    /* A command whose encoding overflowed and was not sent: said once per
     * command, since an application sends it again every frame. */
    void             note_dropped(const char *api, const char *command, size_t written);

    void report(StateWriter &writer) override;

    bool invariants_hold() const;

private:
    /* What putting the frame on a window came to: refused, nothing to show
     * (a lost context, a hidden window), or handed to the present thread. */
    enum class Shown { refused, nothing, handed };

    /* The calling thread's context's frame for the DC's window, unpaced. */
    Shown show_frame(void *dc);
    uint64_t take_pending(uint32_t thread);
    [[nodiscard]] Status put_pending(uint32_t thread, uint64_t handle);
    void     fail(uint32_t error);
    uint32_t made_error(Status status) const;
    uint32_t make_error(Status status, const GlContextOptions &options) const;
    /* The live context an HGLRC names, through opengl32's callback. */
    GuestGlContext *shared_context(void *share);
    bool            shares_into(uint64_t first, uint64_t second) const;
    /* Makes `context` after the unmade contexts it shares through. */
    [[nodiscard]] Status make_chain(GuestGlContext &context);
    /* The last created context along `context`'s share targets. */
    GuestGlContext      &farthest_unmade(GuestGlContext &context) const;
    /* What profile the host granted, named once per process. */
    void                 note_profile(const GuestGlContext &context);

    icd::ThreadLock  &lock_;
    LogSink     &log_;
    KeyedLog    lines_;
    icd::RefusedCalls refused_;
    icd::WindowSurface    &surface_;
    ThreadSlot       &slot_;
    SystemOpengl     &system_;
    GlPresentLoop                &presents_;
    GlIcdBinding     &binding_;
    PixelFormatTable  formats_;
    PixelFormatQuery  query_;
    WindowTable       windows_;
    CurrentContexts   currents_;
    icd::IcdConnection      connection_;
    GlExtensionPolicy extensions_;
    /* The NT handles this process holds for shared objects, which every
     * context's imports resolve through (feature 053, US2). Before the
     * contexts, which name it. */
    icd::SharedHandles shared_handles_;
    GlContextTable    contexts_;
    ManualCalls       manual_;
    void             *callbacks_[max_callbacks];
    uint32_t          callback_count_;
    generated::GlProcTable proc_table_;
    /* The context each thread's wglCreateContextAttribsARB made and opengl32
     * has yet to claim, by thread. */
    KeyedSlots<uint32_t, uint64_t> pending_;
    uint64_t          sync_tokens_;

    friend struct GlIcdRootProbe;
};

}  // namespace av::glicd

#endif  // AV_GLICD_GL_ICD_ROOT_HPP
