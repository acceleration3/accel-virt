#ifndef AV_GLICD_GL_ICD_ROOT_HPP
#define AV_GLICD_GL_ICD_ROOT_HPP

#include "core/trace/state_reporter.hpp"
#include "icd/opengl/current_contexts.hpp"
#include "icd/common/icd_connection.hpp"
#include "icd/common/id_mint.hpp"
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
class BlockSource;
class KeyedLog;
class ClientTransport;
class Kmt;
class StateWriter;
class ThreadLock;
}

namespace av::icd {
class WindowSurface;
}

namespace av::glicd {

class SystemOpengl;
class ThreadSlot;
struct GlIcdRootProbe;

class GlIcdRoot final : public StateReporter {
public:
    static constexpr uint32_t icd_version = 2u;
    static constexpr uint32_t max_callbacks = 9u;
    enum : uint32_t { callback_get_dhglrc = 2u };
    enum : uint32_t {
        error_invalid_handle = 6u, error_not_enough_memory = 8u, error_not_supported = 50u,
        error_invalid_parameter = 87u, error_busy = 170u, error_invalid_pixel_format = 2000u,
        error_invalid_operation = 4317u, error_invalid_version_arb = 0x2095u, error_invalid_profile_arb = 0x2096u,
        error_invalid_data = 13u,
    };
    enum : uint32_t { gl_invalid_operation = 0x0502u, gl_invalid_enum = 0x0500u, gl_invalid_value = 0x0501u };
    static constexpr uint32_t swap_main_plane = 1u;

    GlIcdRoot(BlockSource &memory, Kmt &kmt, ThreadLock &lock, ThreadLock &send_lock, ThreadLock &slice_lock, KeyedLog &log,
              icd::WindowSurface &surface, ThreadSlot &slot, SystemOpengl &system, GlPresentQueue &presents,
              GlIcdBinding &binding);
    ~GlIcdRoot();
    GlIcdRoot(const GlIcdRoot &) = delete;
    GlIcdRoot &operator=(const GlIcdRoot &) = delete;

    bool     validate_version(uint32_t version);
    void     set_callback_procs(int32_t count, void *const *procs);
    int32_t  describe_pixel_format(void *dc, int32_t index, uint32_t bytes, uint8_t *descriptor);
    bool     set_pixel_format(void *dc, int32_t index);
    uint64_t create_context(void *dc);
    uint64_t create_layer_context(void *dc, int32_t layer);
    bool     delete_context(uint64_t handle);
    void    *set_context(void *dc, uint64_t handle, void *set_proc_table);
    bool     release_context(uint64_t handle);
    bool     copy_context(uint64_t source, uint64_t destination, uint32_t mask);
    bool     share_lists(uint64_t first, uint64_t second);
    void    *get_proc_address(const char *name);
    bool     describe_layer_plane(void *dc, int32_t format, int32_t plane, uint32_t bytes, void *descriptor);
    bool     swap_layer_buffers(void *dc, uint32_t planes);
    bool     swap_buffers(void *dc);

    void       *create_context_attribs(void *dc, void *share, const int32_t *attributes);
    const char *extensions_string(void *dc);
    bool        pixel_format_attribs(void *dc, int32_t format, int32_t layer, uint32_t count, const int32_t *names,
                                     int32_t *ints, float *floats);
    void        flush_current();
    bool        set_swap_interval(int32_t interval);
    int32_t     swap_interval();
    bool        choose_pixel_format(void *dc, const int32_t *ints, const float *floats, uint32_t max,
                                    int32_t *formats, uint32_t *count);

    GuestGlContext  *current();
    ManualCalls     &manual();
    ClientTransport *wire();
    uintptr_t        record_legacy();
    uint32_t         get_error();
    uint32_t         reset_status();
    int32_t          force_context_fault();
    uint32_t         current_share_name();
    int32_t          run_held_calls();
    uint64_t         mint_sync_token();
    void             note_dropped(const char *api, const char *command, size_t written);

    void report(StateWriter &writer) override;

    bool invariants_hold() const;

private:
    enum class Shown { refused, nothing, handed };

    Shown show_frame(void *dc);
    Shown present_frame(void *dc);
    void  present_held(GuestGlContext &context);
    uint64_t take_pending(uint32_t thread);
    [[nodiscard]] Status put_pending(uint32_t thread, uint64_t handle);
    void     fail(uint32_t error);
    uint32_t made_error(Status status) const;
    uint32_t make_error(Status status, const GlContextOptions &options) const;
    GuestGlContext *shared_context(void *share, uint64_t &handle);
    bool            shares_into(uint64_t first, uint64_t second) const;
    [[nodiscard]] Status make_chain(GuestGlContext &context);
    GuestGlContext      &farthest_unmade(GuestGlContext &context) const;
    void                 note_profile(const GuestGlContext &context);

    ThreadLock       &lock_;
    KeyedLog    &lines_;
    icd::RefusedCalls refused_;
    icd::WindowSurface    &surface_;
    ThreadSlot       &slot_;
    SystemOpengl     &system_;
    GlPresentQueue               &presents_;
    GlIcdBinding     &binding_;
    PixelFormatTable  formats_;
    PixelFormatQuery  query_;
    WindowTable       windows_;
    CurrentContexts   currents_;
    icd::IcdConnection      connection_;
    GlExtensionPolicy extensions_;
    icd::SharedHandles shared_handles_;
    GlContextTable    contexts_;
    ManualCalls       manual_;
    void             *callbacks_[max_callbacks];
    uint32_t          callback_count_;
    generated::GlProcTable proc_table_;
    KeyedSlots<uint32_t, uint64_t> pending_;
    icd::IdMint<uint64_t> sync_tokens_;

    friend struct GlIcdRootProbe;
};

}

#endif
