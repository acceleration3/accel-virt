#ifndef AV_GLICD_GUEST_GL_CONTEXT_HPP
#define AV_GLICD_GUEST_GL_CONTEXT_HPP

#include "core/log/driver_log.hpp"
#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "icd/common/context_link.hpp"
#include "icd/common/host_channel.hpp"
#include "icd/common/icd_connection.hpp"
#include "icd/common/loss_latch.hpp"
#include "icd/opengl/reported_gl_errors.hpp"
#include "icd/opengl/buffer_shadows.hpp"
#include "icd/opengl/client_array_state.hpp"
#include "icd/opengl/gl_extension_list.hpp"
#include "icd/common/present_pacing.hpp"
#include "icd/common/locked_timeline.hpp"
#include "icd/opengl/frames_in_flight.hpp"
#include "icd/opengl/granted_profile.hpp"
#include "icd/opengl/gl_external_objects.hpp"
#include "icd/opengl/mapped_buffers.hpp"
#include "icd/opengl/pinned_buffers.hpp"
#include "icd/opengl/pixel_staging_buffer.hpp"
#include "icd/opengl/query_shapes.hpp"
#include "icd/opengl/uniform_shapes.hpp"
#include "icd/opengl/window_framebuffers.hpp"
#include "proto/context/gl_context_options.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class BlockSource;
class ClientTransport;
class KeyedLog;
class StateWriter;
class ThreadLock;
}

namespace av::glicd {

class GlExtensionPolicy;
struct GuestGlContextProbe;

class GuestGlContext {
public:
    enum class State : uint8_t { created, made, lost };

    static constexpr uint8_t  ring_order = 20u;
    static constexpr uint32_t reply_bytes = 64u * 1024u;
    static constexpr size_t   staging_bytes = 256u * 1024u;
    static constexpr size_t   batch_bytes = 2u * staging_bytes;
    static constexpr uint32_t string_bytes = 256u;
    enum : uint32_t {
        gl_vendor = 0x1F00u, gl_renderer = 0x1F01u, gl_version = 0x1F02u, gl_shading_language_version = 0x8B8Cu,
    };
    enum : uint32_t {
        gl_out_of_memory = 0x0505u, gl_context_lost = 0x0507u, gl_guilty_context_reset = 0x8253u,
        gl_unknown_context_reset = 0x8255u,
    };

    GuestGlContext(icd::IcdConnection &connection, BlockSource &memory, ThreadLock &send_lock,
                   KeyedLog &lines, const GlExtensionPolicy &extensions, icd::SharedHandles &handles, int32_t pixel_format,
                   const GlContextOptions &options);
    ~GuestGlContext();
    GuestGlContext(const GuestGlContext &) = delete;
    GuestGlContext &operator=(const GuestGlContext &) = delete;

    [[nodiscard]] Status make();
    State                state() const;
    uint32_t             owner() const;
    void                 set_owner(uint32_t thread);
    void                *current_dc() const;
    void                 set_current_dc(void *dc);
    int32_t              pixel_format() const;
    const GlContextOptions &options() const;
    uint32_t             share_name() const;
    [[nodiscard]] Status share_after(uint64_t target);
    uint64_t             share_target() const;
    [[nodiscard]] Status share(uint32_t share_name);
    void                 lend();
    ClientTransport     *wire();
    uint32_t             guest_device() const;
    void                 flush();
    [[nodiscard]] Status run_held();
    void                 end_frame();
    BatchMark            mark() const;

    ClientArrayState &arrays();
    PinnedBuffers    &pinned();
    UniformShapes    &uniforms();
    QueryShapes      &queries();
    const GlExtensionList &extensions() const;
    WindowFramebuffers    &framebuffers();
    MappedBuffers         &mappings();
    BufferShadows         &shadows();
    GlExternalObjects     &externals();
    PixelStagingBuffer    &staging();

    void        record(uint32_t error);
    uint32_t    take_error();
    bool     take_host_error(uint32_t &error);
    bool        robust() const;
    uint32_t    lost_reset_status() const;
    const char *string(uint32_t name) const;
    const GrantedProfile &profile() const;
    icd::PresentPacing   &pacing();
    uint64_t              adapter_luid() const;
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
    static constexpr uint64_t arena_bytes = uint64_t{48} << 20u;

    [[nodiscard]] Status build();
    [[nodiscard]] Status make_once();
    bool                 asked_for_itself() const;
    bool                 learn();
    void                 cap_version(uint32_t row, uint32_t most_minor);
    void                 teardown();

    BlockSource                   &memory_;
    KeyedLog                      &lines_;
    const GlExtensionPolicy       &policy_;
    int32_t                        pixel_format_;
    GlContextOptions               options_;
    State                          state_;
    uint32_t                       owner_;
    void                          *current_dc_;
    bool                           been_current_;
    uint32_t                       error_;
    uint64_t                       share_target_;
    icd::ContextLink               link_;
    icd::LossLatch                 loss_;
    bool                           loss_reported_;
    icd::HostChannel               channel_;
    ReportedGlErrors               reported_;
    ClientArrayState               arrays_;
    UniformShapes                  uniforms_;
    QueryShapes                    queries_;
    GrantedProfile                 profile_;
    icd::PresentPacing             pacing_;
    FramesInFlight                 frames_;
    icd::LockedTimeline            timeline_;
    GlExtensionList                extensions_;
    WindowFramebuffers             framebuffers_;
    BufferShadows                  shadows_;
    PixelStagingBuffer             staging_;
    MappedBuffers                  mappings_;
    GlExternalObjects              externals_;
    PinnedBuffers                  pinned_;
    char                           strings_[strings][string_bytes];
    uint64_t                       debug_callback_;
    uint64_t                       debug_user_;
    uint64_t                       luid_;

    friend struct GuestGlContextProbe;
};

}

#endif
