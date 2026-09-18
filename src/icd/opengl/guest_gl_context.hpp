/* One application OpenGL context (data-model.md): the options and pixel format
 * it was created with, the thread it is current on, and - once made - its own
 * rvk OpenGL context on the process's device (a KmtTransport of SubApi::opengl
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
#include "guest/transport/kmt_blob_arena.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "icd/opengl/gl_connection.hpp"
#include "icd/common/locked_transport.hpp"
#include "icd/opengl/client_array_state.hpp"
#include "icd/opengl/gl_extension_list.hpp"
#include "icd/opengl/granted_profile.hpp"
#include "icd/opengl/mapped_buffers.hpp"
#include "icd/opengl/query_shapes.hpp"
#include "icd/opengl/uniform_shapes.hpp"
#include "icd/opengl/window_framebuffers.hpp"
#include "proto/context/gl_context_options.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class BlockSource;
class ClientTransport;
class StateWriter;
}  // namespace av

namespace av::icd {
class LogSink;
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
    static constexpr uint32_t string_bytes = 256u;
    enum : uint32_t {
        gl_vendor = 0x1F00u, gl_renderer = 0x1F01u, gl_version = 0x1F02u, gl_shading_language_version = 0x8B8Cu,
    };
    enum : uint32_t { gl_out_of_memory = 0x0505u, gl_context_lost = 0x0507u, gl_unknown_context_reset = 0x8255u };

    GuestGlContext(GlConnection &connection, BlockSource &memory, icd::ThreadLock &send_lock, icd::LogSink &log,
                   const GlExtensionPolicy &extensions, int32_t pixel_format, const GlContextOptions &options);
    ~GuestGlContext();
    GuestGlContext(const GuestGlContext &) = delete;
    GuestGlContext &operator=(const GuestGlContext &) = delete;

    /* Makes the host context: invalid_state unless created; not_supported with
     * no rvkgpu device; no_memory when the staging cannot be had;
     * invalid_parameter when the driver or the host refused the options -
     * nothing held after any of them. A blob arena the device cannot give is
     * logged, and bulk data then crosses inline. */
    [[nodiscard]] Status make();
    State                state() const;
    /* The thread the context is current on, 0 when none. */
    uint32_t             owner() const;
    void                 set_owner(uint32_t thread);
    int32_t              pixel_format() const;
    const GlContextOptions &options() const;
    /* The ring allocation another context names to share with this one; 0
     * unless made. */
    uint32_t             share_name() const;
    /* wglShareLists: the context whose objects this one is to share, by its
     * handle, looked up when this one is made. invalid_state once made or
     * with a target recorded already. */
    [[nodiscard]] Status share_after(uint64_t target);
    uint64_t             share_target() const;
    /* The options name `share_name`'s context to share with, and, when they
     * ask no version, 1.0 - a share needs one on the wire
     * (contracts/context_options.md) and it is what an empty
     * WGL_ARB_create_context list asks. invalid_state once made. */
    [[nodiscard]] Status share(uint32_t share_name);
    /* What a GL call sends through: null unless made and not lost. A faulted
     * transport makes the context lost here. */
    ClientTransport     *wire();

    ClientArrayState &arrays();
    UniformShapes    &uniforms();
    QueryShapes      &queries();
    /* The OpenGL extensions reported for this context, learned at make. */
    const GlExtensionList &extensions() const;
    /* The windows this context draws on. */
    WindowFramebuffers    &framebuffers();
    /* The buffer ranges the application has mapped. */
    MappedBuffers         &mappings();

    /* The first error recorded is kept until taken. */
    void        record(uint32_t error);
    /* The recorded error; then, once, the loss of a lost context -
     * GL_CONTEXT_LOST for a robust one, GL_OUT_OF_MEMORY for any other
     * (T040); then GL_NO_ERROR, 0. */
    uint32_t    take_error();
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
    static constexpr uint64_t arena_bytes = KmtBlobArena::default_bytes;

    [[nodiscard]] Status build();
    /* One ask: built, learned, and held or given back whole. */
    [[nodiscard]] Status make_once();
    bool                 asked_for_itself() const;
    /* The host's strings and list lengths; false when the host answered none. */
    bool                 learn();
    void                 cap_version();
    void                 teardown();

    GlConnection                  &connection_;
    BlockSource                   &memory_;
    icd::ThreadLock               &send_lock_;
    icd::LogSink                  &log_;
    const GlExtensionPolicy       &policy_;
    int32_t                        pixel_format_;
    GlContextOptions               options_;
    State                          state_;
    uint32_t                       owner_;
    uint32_t                       error_;
    uint64_t                       share_target_;
    /* The device link a made or lost context holds; no_link otherwise. */
    uint32_t                       link_;
    bool                           loss_reported_;
    uint8_t                       *staging_;
    ObjectStorage<KmtTransport>    transport_;
    ObjectStorage<icd::LockedTransport> locked_;
    ObjectStorage<KmtBlobArena>    arena_;
    ClientArrayState               arrays_;
    UniformShapes                  uniforms_;
    QueryShapes                    queries_;
    GrantedProfile                 profile_;
    GlExtensionList                extensions_;
    WindowFramebuffers             framebuffers_;
    MappedBuffers                  mappings_;
    char                           strings_[strings][string_bytes];
    uint64_t                       debug_callback_;
    uint64_t                       debug_user_;

    friend struct GuestGlContextProbe;
};

}  // namespace av::glicd

#endif  // AV_GLICD_GUEST_GL_CONTEXT_HPP
