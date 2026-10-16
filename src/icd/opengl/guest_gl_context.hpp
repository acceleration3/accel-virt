/* One application OpenGL context (data-model.md): the options and pixel format
 * it was created with, the thread it is current on, and - once made - its own
 * av OpenGL context on the process's device (a KmtTransport of SubApi::opengl
 * behind the send lock, and a blob arena its bulk data crosses in), with the
 * guest-side state the generated client asks about and the host's strings.
 *
 * created -> (make) made -> lost when its device faults - through its own
 * transport or another context's on the same device. Nothing is on the
 * host until make; a make that fails leaves the context created and holds
 * nothing. The error a WGL or legacy call records locally is what glGetError
 * answers before the host's. */
#ifndef AV_GLICD_GUEST_GL_CONTEXT_HPP
#define AV_GLICD_GUEST_GL_CONTEXT_HPP

#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "icd/common/context_link.hpp"
#include "icd/common/host_channel.hpp"
#include "icd/common/icd_connection.hpp"
#include "icd/opengl/reported_gl_errors.hpp"
#include "icd/opengl/buffer_shadows.hpp"
#include "icd/opengl/client_array_state.hpp"
#include "icd/opengl/gl_extension_list.hpp"
#include "icd/common/present_pacing.hpp"
#include "icd/opengl/frames_in_flight.hpp"
#include "icd/opengl/granted_profile.hpp"
#include "icd/opengl/gl_external_objects.hpp"
#include "icd/opengl/mapped_buffers.hpp"
#include "icd/opengl/pixel_staging_buffer.hpp"
#include "icd/opengl/query_shapes.hpp"
#include "icd/opengl/uniform_shapes.hpp"
#include "icd/opengl/window_framebuffers.hpp"
#include "proto/context/gl_context_options.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class LogSink;
}  // namespace av

namespace av {
class BlockSource;
class ClientTransport;
class StateWriter;
}  // namespace av

namespace av::icd {
class ThreadLock;
}  // namespace av::icd

namespace av::glicd {

class GlExtensionPolicy;
struct GuestGlContextProbe;

class GuestGlContext {
public:
    enum class State : uint8_t { created, made, lost };

    static constexpr uint8_t  ring_order = 20u;
    static constexpr uint32_t reply_bytes = 64u * 1024u;
    static constexpr size_t   staging_bytes = 256u * 1024u;
    /* The calls held before they go as one batch; two of the largest a
     * command can be, and well inside the ring (ring_order). */
    static constexpr size_t   batch_bytes = 2u * staging_bytes;
    static constexpr uint32_t string_bytes = 256u;
    enum : uint32_t {
        gl_vendor = 0x1F00u, gl_renderer = 0x1F01u, gl_version = 0x1F02u, gl_shading_language_version = 0x8B8Cu,
    };
    enum : uint32_t { gl_out_of_memory = 0x0505u, gl_context_lost = 0x0507u, gl_unknown_context_reset = 0x8255u };

    GuestGlContext(icd::IcdConnection &connection, BlockSource &memory, icd::ThreadLock &send_lock, LogSink &log,
                   const GlExtensionPolicy &extensions, icd::SharedHandles &handles, int32_t pixel_format,
                   const GlContextOptions &options);
    ~GuestGlContext();
    GuestGlContext(const GuestGlContext &) = delete;
    GuestGlContext &operator=(const GuestGlContext &) = delete;

    /* Makes the host context: invalid_state unless created; not_supported with
     * no avgpu device; no_memory when the staging cannot be had;
     * invalid_parameter when the driver or the host refused the options -
     * nothing held after any of them. A blob arena the device cannot give is
     * logged, and bulk data then crosses inline. */
    [[nodiscard]] Status make();
    State                state() const;
    /* The thread the context is current on, 0 when none. */
    uint32_t             owner() const;
    void                 set_owner(uint32_t thread);
    /* The device context it was made current on, which a flush that presents
     * puts its frame on (feature 051); null when not current. */
    void                *current_dc() const;
    void                 set_current_dc(void *dc);
    int32_t              pixel_format() const;
    const GlContextOptions &options() const;
    /* The ring allocation another context names to share with this one; 0
     * unless made. */
    uint32_t             share_name() const;
    /* wglShareLists: the context whose objects this one is to share, by its
     * handle, looked up when this one is made. A context made but never
     * current - wglCreateContextAttribsARB makes at once, to answer its
     * errors - holds no object yet, so its host context goes and the next
     * make takes the share. invalid_state once it has been current, when
     * lost, or with a target recorded already. */
    [[nodiscard]] Status share_after(uint64_t target);
    uint64_t             share_target() const;
    /* The options name `share_name`'s context to share with, and, when they
     * ask no version, 1.0 - a share needs one on the wire
     * (contracts/context_options.md) and it is what an empty
     * WGL_ARB_create_context list asks. invalid_state once made. Buffers
     * shared can change through the other context, so this one keeps no
     * copies of them (BufferShadows). */
    [[nodiscard]] Status share(uint32_t share_name);
    /* Another context is to share this one's objects: the same. */
    void                 lend();
    /* What a GL call sends through: null unless made and not lost. A faulted
     * transport makes the context lost here. */
    ClientTransport     *wire();
    /* The kernel's device this context's link holds, which opens a handle
     * another process sent; 0 before the context is linked. */
    uint32_t             guest_device() const;
    /* The calls held so far go to the host: at glFlush, and before another
     * context may depend on them. */
    void                 flush();
    /* The calls held so far go to the host and the host has run them before
     * this returns: a round trip of the transport's no-op behind them, which
     * the host's worker answers only once it has run every command before it.
     * What another API's context is about to name (cl_khr_gl_sharing) was
     * made by them, and that context runs on a worker of its own. ok when
     * answered; invalid_state unless made, or when the round trip failed. */
    [[nodiscard]] Status run_held();
    /* The application ended a frame - glFlush, a swap: what is held goes to
     * the host, and the application goes on only once the host is no more
     * than FramesInFlight::latency_ms behind it. A host that never catches up
     * loses the context. */
    void                 end_frame();
    /* The newest batch sent, for another thread to wait on (BatchMark); one
     * of no batch unless made. */
    BatchMark            mark() const;

    ClientArrayState &arrays();
    UniformShapes    &uniforms();
    QueryShapes      &queries();
    /* The OpenGL extensions reported for this context, learned at make. */
    const GlExtensionList &extensions() const;
    /* The windows this context draws on. */
    WindowFramebuffers    &framebuffers();
    /* The buffer ranges the application has mapped. */
    MappedBuffers         &mappings();
    /* What the host holds of this context's buffers, as far as it knows. */
    BufferShadows         &shadows();
    /* The memory objects and semaphores imported from a Win32 handle. */
    GlExternalObjects     &externals();
    /* The host buffer a pixel transfer too large to cross inline goes
     * through. */
    PixelStagingBuffer    &staging();

    /* The first error recorded is kept until taken. */
    void        record(uint32_t error);
    /* The recorded error; then, once, the loss of a lost context -
     * GL_CONTEXT_LOST for a robust one, GL_OUT_OF_MEMORY for any other
     * (T040); then GL_NO_ERROR, 0. */
    uint32_t    take_error();
    /* The next error the host published for this context, in `error`: false
     * when there is no word to read - no arena, or a host that answers only
     * when asked - and the caller asks it instead. */
    bool     take_host_error(uint32_t &error);
    /* Asked for with both robust access and lose-on-reset. */
    bool        robust() const;
    /* What glGetGraphicsResetStatus answers without the host: for a lost
     * robust context GL_UNKNOWN_CONTEXT_RESET, every time, since a lost
     * context's reset never completes; otherwise GL_NO_ERROR. */
    uint32_t    lost_reset_status() const;
    /* The host's string for `name`, the version capped at 4.6; null for any
     * other name or before make. */
    const char *string(uint32_t name) const;
    /* What the host granted this context, learned at make (feature 051). */
    const GrantedProfile &profile() const;
    /* How many vertical blanks this context's swaps wait for, waited on the
     * adapter it is made on: nothing waits, and every frame is admitted,
     * before it is made. */
    icd::PresentPacing   &pacing();
    /* The LUID of the adapter the context draws on, which a window's chain is
     * made on (ChainedFrames); 0 before the context is made, or when the
     * kernel would not say. */
    uint64_t              adapter_luid() const;
    /* Which ask is being answered, recorded before the make that asks it. */
    void                  asking(GrantedProfile::Rung rung);
    void        set_debug_callback(uint64_t callback, uint64_t user);
    uint64_t    debug_callback() const;
    uint64_t    debug_user() const;

    void report(StateWriter &writer, uint64_t handle) const;

    bool invariants_hold() const;

private:
    enum : uint32_t { vendor_row, renderer_row, version_row, shading_row, strings };
    static constexpr uint32_t string_names[strings] = {gl_vendor, gl_renderer, gl_version,
                                                        gl_shading_language_version};
    /* Room beside the leases for a window's two frames the present thread
     * shows (BackBufferPresenter::slot_count): 16.6 MB at 1920x1080, and the
     * rest still lends what one frame of it would. */
    static constexpr uint64_t arena_bytes = uint64_t{48} << 20u;

    [[nodiscard]] Status build();
    /* One ask: built, learned, and held or given back whole. */
    [[nodiscard]] Status make_once();
    bool                 asked_for_itself() const;
    /* The host's strings and list lengths; false when the host answered none. */
    bool                 learn();
    void                 cap_version();
    void                 teardown();

    BlockSource                   &memory_;
    LogSink                  &log_;
    const GlExtensionPolicy       &policy_;
    int32_t                        pixel_format_;
    GlContextOptions               options_;
    State                          state_;
    uint32_t                       owner_;
    void                          *current_dc_;
    /* Sticky: a context once current may hold objects, and takes no share. */
    bool                           been_current_;
    uint32_t                       error_;
    uint64_t                       share_target_;
    /* The device link a made or lost context holds; no_link otherwise. */
    icd::ContextLink               link_;
    bool                           loss_reported_;
    icd::HostChannel               channel_;
    ReportedGlErrors               reported_;
    ClientArrayState               arrays_;
    UniformShapes                  uniforms_;
    QueryShapes                    queries_;
    GrantedProfile                 profile_;
    icd::PresentPacing             pacing_;
    FramesInFlight                 frames_;
    GlExtensionList                extensions_;
    WindowFramebuffers             framebuffers_;
    BufferShadows                  shadows_;
    PixelStagingBuffer             staging_;
    MappedBuffers                  mappings_;
    GlExternalObjects              externals_;
    char                           strings_[strings][string_bytes];
    uint64_t                       debug_callback_;
    uint64_t                       debug_user_;
    uint64_t                       luid_;

    friend struct GuestGlContextProbe;
};

}  // namespace av::glicd

#endif  // AV_GLICD_GUEST_GL_CONTEXT_HPP
