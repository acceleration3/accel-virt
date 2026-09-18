#include "icd/opengl/gl_icd_root.hpp"

#include "icd/opengl/granted_profile.hpp"
#include "icd/opengl/window_visibility.hpp"

#include "core/support/scope_exit.hpp"
#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/common/extension_policy.hpp"
#include "icd/common/log_sink.hpp"
#include "guest/transport/context_fault.hpp"
#include "icd/common/thread_lock.hpp"
#include "icd/opengl/context_attributes.hpp"
#include "icd/opengl/gl_entry_points.hpp"
#include "icd/opengl/gl_icd_binding.hpp"
#include "icd/opengl/system_opengl.hpp"
#include "icd/opengl/thread_slot.hpp"
#include "icd/common/window_surface.hpp"

#include "gl_dispatch_client.h"
#include "gl_protocol_ids.h"

namespace av::glicd {

namespace {

/* glGetError's slot in opengl32's table (tools/codegen/profiles/opengl_proc_table.txt). */
constexpr uint32_t get_error_slot = 261u;
constexpr uint32_t get_integerv_slot = 263u;
constexpr uint32_t line_bytes = 160u;
/* ARB_robustness, KHR_robustness, EXT_robustness and GL 4.5 all name one query. */
constexpr const char *const reset_status_names[] = {"glGetGraphicsResetStatus", "glGetGraphicsResetStatusARB",
                                                    "glGetGraphicsResetStatusEXT", "glGetGraphicsResetStatusKHR"};
/* The GL extensions reported: a row arrives with the test that proves its
 * extension through this ICD (tools/check_extension_policy.py), and is still
 * only reported where the host's own list has it. */
constexpr icd::ExtensionPolicy::Row reported_extensions[] = {
    {"GL_ARB_compatibility", 1u,
     "fullstack_opengl_icd.a_legacy_context_draws_the_fixed_function_pipeline_through_a_display_list"},
};
constexpr uint32_t reported_count = sizeof reported_extensions / sizeof reported_extensions[0];
/* The WGL extensions the functions DrvGetProcAddress answers make whole, each
 * with the test that proves it. */
constexpr icd::ExtensionPolicy::Row reported_wgl_extensions[] = {
    {"WGL_ARB_extensions_string", 1u, "gl_icd_root.answers_gl_and_wgl_names_it_implements"},
    {"WGL_ARB_pixel_format", 1u, "gl_icd_root.answers_pixel_format_attributes_and_choices_through_wgl"},
    {"WGL_ARB_create_context", 1u, "gl_icd_root.a_create_with_attributes_makes_the_context_and_opengl32s_hglrc_names_it"},
    {"WGL_ARB_create_context_profile", 1u, "gl_icd_root.a_create_with_attributes_answers_the_arb_errors"},
    {"WGL_ARB_create_context_robustness", 1u, "context_attributes.flags_and_the_reset_strategy_become_option_flags"},
    {"WGL_ARB_multisample", 1u, "back_buffer_presenter.a_multisampled_frame_is_resolved_before_it_is_read"},
    {"WGL_ARB_framebuffer_sRGB", 1u, "back_buffer_presenter.an_srgb_format_stores_srgb_colour"},
    {"WGL_EXT_swap_control", 1u, "gl_icd_root_set_swap_interval.a_swap_waits_for_the_blanks_its_interval_asks_for"},
};
constexpr uint32_t reported_wgl_count = sizeof reported_wgl_extensions / sizeof reported_wgl_extensions[0];

}  // namespace

GlIcdRoot::GlIcdRoot(BlockSource &memory, Kmt &kmt, icd::ThreadLock &lock, icd::ThreadLock &send_lock,
                     icd::LogSink &log, icd::WindowSurface &surface, ThreadSlot &slot, SystemOpengl &system,
                     GlIcdBinding &binding)
    : lock_(lock), log_(log), surface_(surface), slot_(slot), system_(system), binding_(binding), formats_(),
      query_(formats_), windows_(), currents_(slot), connection_(kmt),
      extensions_(reported_extensions, reported_count, reported_wgl_extensions, reported_wgl_count),
      contexts_(memory, connection_, send_lock, log, extensions_),
      manual_(currents_), callbacks_{}, callback_count_(0u), proc_table_(), pending_threads_{}, pending_handles_{}, sync_tokens_(0u), profile_noted_(false)
{
    AV_SCOPE("GlIcdRoot::GlIcdRoot");
    generated::fill_proc_table(proc_table_);
    proc_table_.slots[get_error_slot] = reinterpret_cast<GlProc>(&entry::get_error);
    proc_table_.slots[get_integerv_slot] = reinterpret_cast<GlProc>(&entry::get_integerv);
    binding_.bind(*this);
}

GlIcdRoot::~GlIcdRoot()
{
    AV_SCOPE("GlIcdRoot::~GlIcdRoot");
    binding_.unbind();
}

bool GlIcdRoot::validate_version(uint32_t version)
{
    AV_SCOPE("GlIcdRoot::validate_version");
    return version == icd_version;
}

void GlIcdRoot::set_callback_procs(int32_t count, void *const *procs)
{
    AV_SCOPE("GlIcdRoot::set_callback_procs");
    if (procs == nullptr || count < 0) return;
    const uint32_t kept = static_cast<uint32_t>(count) < max_callbacks ? static_cast<uint32_t>(count) : max_callbacks;
    for (uint32_t index = 0u; index < kept; index++) callbacks_[index] = procs[index];
    callback_count_ = kept;
}

int32_t GlIcdRoot::describe_pixel_format(void *dc, int32_t index, uint32_t bytes, uint8_t *descriptor)
{
    AV_SCOPE("GlIcdRoot::describe_pixel_format");
    if (surface_.window_of(dc) == nullptr) return 0;
    return formats_.describe(index, bytes, descriptor);
}

bool GlIcdRoot::set_pixel_format(void *dc, int32_t index)
{
    AV_SCOPE("GlIcdRoot::set_pixel_format");
    void *const window = surface_.window_of(dc);
    if (window == nullptr || !formats_.contains(index)) {
        fail(error_invalid_pixel_format);
        return false;
    }
    lock_.acquire();
    ScopeExit release([&] { lock_.release(); });
    Status    set = windows_.set_format(window, index);
    if (set == Status::no_space) {
        windows_.drop_gone(surface_);
        set = windows_.set_format(window, index);
    }
    if (set == Status::ok) return true;
    fail(set == Status::no_space ? error_not_enough_memory : error_invalid_pixel_format);
    return false;
}

uint64_t GlIcdRoot::create_context(void *dc)
{
    AV_SCOPE("GlIcdRoot::create_context");
    return create_layer_context(dc, 0);
}

uint64_t GlIcdRoot::create_layer_context(void *dc, int32_t layer)
{
    AV_SCOPE("GlIcdRoot::create_layer_context");
    if (layer != 0) {
        fail(error_invalid_parameter);
        return 0u;
    }
    lock_.acquire();
    ScopeExit      release([&] { lock_.release(); });
    const uint64_t pending = take_pending(slot_.thread_id());
    if (pending != 0u) return pending;
    const int32_t format = windows_.format_of(surface_.window_of(dc));
    if (format == 0) {
        fail(error_invalid_pixel_format);
        return 0u;
    }
    uint64_t handle = 0u;
    if (contexts_.create(format, GlContextOptions(), handle) != Status::ok) fail(error_not_enough_memory);
    return handle;
}

bool GlIcdRoot::delete_context(uint64_t handle)
{
    AV_SCOPE("GlIcdRoot::delete_context");
    lock_.acquire();
    ScopeExit             release([&] { lock_.release(); });
    GuestGlContext *const context = contexts_.resolve(handle);
    if (context == nullptr) {
        fail(error_invalid_handle);
        return false;
    }
    if (currents_.current_elsewhere(*context)) {
        fail(error_busy);
        return false;
    }
    if (currents_.current() == context) currents_.release();
    /* The handle resolved above, so the table holds its context and gives it back. */
    static_cast<void>(contexts_.destroy(handle));
    return true;
}

void *GlIcdRoot::set_context(void *dc, uint64_t handle, void *)
{
    AV_SCOPE("GlIcdRoot::set_context");
    lock_.acquire();
    ScopeExit             release([&] { lock_.release(); });
    GuestGlContext *const context = contexts_.resolve(handle);
    if (context == nullptr) {
        fail(error_invalid_handle);
        return nullptr;
    }
    if (windows_.format_of(surface_.window_of(dc)) != context->pixel_format()) {
        fail(error_invalid_pixel_format);
        return nullptr;
    }
    if (currents_.current_elsewhere(*context)) {
        fail(error_busy);
        return nullptr;
    }
    if (context->state() == GuestGlContext::State::created) {
        const Status made = make_chain(*context);
        if (made != Status::ok) {
            fail(made_error(made));
            return nullptr;
        }
        note_profile(*context);
    }
    ClientTransport *const transport = context->wire();
    if (transport != nullptr) {
        const Status on_window = context->framebuffers().make_current_on(
            *transport, surface_.window_of(dc), formats_.format(context->pixel_format()), surface_);
        /* A context the host lost meanwhile is made current all the same, as
         * a lost context is: every call on it then does nothing. */
        if (on_window != Status::ok && context->wire() != nullptr) {
            fail(on_window == Status::invalid_parameter ? error_invalid_pixel_format : error_not_enough_memory);
            return nullptr;
        }
    }
    context->set_current_dc(dc);
    static_cast<void>(currents_.make_current(*context));
    return &proc_table_;
}

bool GlIcdRoot::release_context(uint64_t handle)
{
    AV_SCOPE("GlIcdRoot::release_context");
    lock_.acquire();
    ScopeExit             release([&] { lock_.release(); });
    GuestGlContext *const context = contexts_.resolve(handle);
    if (context == nullptr || currents_.current() != context) return false;
    context->set_current_dc(nullptr);
    currents_.release();
    return true;
}

bool GlIcdRoot::copy_context(uint64_t, uint64_t, uint32_t)
{
    AV_SCOPE("GlIcdRoot::copy_context");
    fail(error_not_supported);
    return false;
}

/* `second` is to share `first`'s objects. The host shares at creation, so
 * both must still be unmade; `second` is made after `first` (make_chain). */
bool GlIcdRoot::share_lists(uint64_t first, uint64_t second)
{
    AV_SCOPE("GlIcdRoot::share_lists");
    lock_.acquire();
    ScopeExit             release([&] { lock_.release(); });
    GuestGlContext *const target = contexts_.resolve(first);
    GuestGlContext *const sharer = contexts_.resolve(second);
    if (target == nullptr || sharer == nullptr) {
        fail(error_invalid_handle);
        return false;
    }
    const bool unmade = target->state() == GuestGlContext::State::created &&
                        sharer->state() == GuestGlContext::State::created;
    if (!unmade || shares_into(first, second) || sharer->share_after(first) != Status::ok) {
        fail(error_invalid_operation);
        return false;
    }
    return true;
}

void *GlIcdRoot::get_proc_address(const char *name)
{
    AV_SCOPE("GlIcdRoot::get_proc_address");
    if (name == nullptr) return nullptr;
    if (__builtin_strcmp(name, "wglCreateContextAttribsARB") == 0)
        return reinterpret_cast<void *>(&entry::wglCreateContextAttribsARB);
    if (__builtin_strcmp(name, "wglGetExtensionsStringARB") == 0)
        return reinterpret_cast<void *>(&entry::wglGetExtensionsStringARB);
    if (__builtin_strcmp(name, "wglGetPixelFormatAttribivARB") == 0)
        return reinterpret_cast<void *>(&entry::wglGetPixelFormatAttribivARB);
    if (__builtin_strcmp(name, "wglGetPixelFormatAttribfvARB") == 0)
        return reinterpret_cast<void *>(&entry::wglGetPixelFormatAttribfvARB);
    if (__builtin_strcmp(name, "wglChoosePixelFormatARB") == 0)
        return reinterpret_cast<void *>(&entry::wglChoosePixelFormatARB);
    if (__builtin_strcmp(name, "wglSwapIntervalEXT") == 0)
        return reinterpret_cast<void *>(&entry::wglSwapIntervalEXT);
    if (__builtin_strcmp(name, "wglGetSwapIntervalEXT") == 0)
        return reinterpret_cast<void *>(&entry::wglGetSwapIntervalEXT);
    /* A lost context answers its reset status without the host. */
    for (const char *const spelling : reset_status_names)
        if (__builtin_strcmp(name, spelling) == 0) return reinterpret_cast<void *>(&entry::get_graphics_reset_status);
    if (__builtin_strcmp(name, "rvkForceContextFault") == 0)
        return reinterpret_cast<void *>(&entry::rvkForceContextFault);
    for (uint32_t row = 0u; row < generated::kNamedEntryCount; row++)
        if (__builtin_strcmp(generated::kNamedEntries[row].name, name) == 0)
            return reinterpret_cast<void *>(generated::kNamedEntries[row].address);
    return nullptr;
}

bool GlIcdRoot::describe_layer_plane(void *, int32_t, int32_t, uint32_t, void *)
{
    AV_SCOPE("GlIcdRoot::describe_layer_plane");
    return false;
}

bool GlIcdRoot::swap_layer_buffers(void *dc, uint32_t planes)
{
    AV_SCOPE("GlIcdRoot::swap_layer_buffers");
    return (planes & swap_main_plane) != 0u && swap_buffers(dc);
}

/* The calling thread's context puts its frame for the DC's window there. */
bool GlIcdRoot::swap_buffers(void *dc)
{
    AV_SCOPE("GlIcdRoot::swap_buffers");
    lock_.acquire();
    windows_.drop_gone(surface_);
    lock_.release();
    GuestGlContext *const context = currents_.current();
    void *const           window = surface_.window_of(dc);
    if (context == nullptr || windows_.format_of(window) != context->pixel_format()) return false;
    ClientTransport *const transport = context->wire();
    /* A lost context has nothing to show and fails nothing by swapping. */
    if (transport == nullptr) return true;
    /* Nor does a window nobody can see: the frame is read back through the
     * host GPU and blitted for a client area that is not there, and the
     * application's render thread waits for both (feature 051, US2.3). */
    if (!WindowVisibility(surface_, window).showing()) return true;
    WindowFramebuffers &framebuffers = context->framebuffers();
    framebuffers.drop_gone(*transport, surface_);
    if (!framebuffers.present(*transport, context->arrays(), dc, window,
                              formats_.format(context->pixel_format()), surface_))
        return false;
    /* The frame is on its window; what the interval buys is the wait after it
     * (feature 051, US2.2). */
    context->pace();
    return true;
}

void GlIcdRoot::flush_current()
{
    AV_SCOPE("GlIcdRoot::flush_current");
    const GuestGlContext *const context = currents_.current();
    if (context != nullptr && !formats_.format(context->pixel_format()).double_buffer())
        static_cast<void>(swap_buffers(context->current_dc()));
    manual_.flush();
}

bool GlIcdRoot::set_swap_interval(int32_t interval)
{
    AV_SCOPE("GlIcdRoot::set_swap_interval");
    GuestGlContext *const context = currents_.current();
    if (context == nullptr) {
        fail(error_invalid_operation);
        return false;
    }
    if (!context->interval().ask(interval)) {
        fail(error_invalid_data);
        return false;
    }
    return true;
}

int32_t GlIcdRoot::swap_interval()
{
    AV_SCOPE("GlIcdRoot::swap_interval");
    GuestGlContext *const context = currents_.current();
    return context != nullptr ? static_cast<int32_t>(context->interval().blanks()) : 0;
}

void *GlIcdRoot::create_context_attribs(void *dc, void *share, const int32_t *attributes)
{
    AV_SCOPE("GlIcdRoot::create_context_attribs");
    ContextAttributes attribs(attributes);
    const uint32_t    refused = attribs.decode();
    if (refused != 0u) {
        fail(refused);
        return nullptr;
    }
    const int32_t format = windows_.format_of(surface_.window_of(dc));
    if (format == 0) {
        fail(error_invalid_pixel_format);
        return nullptr;
    }
    const GlContextOptions options = attribs.options();
    uint32_t               share_name = 0u;
    if (share != nullptr) {
        GuestGlContext *const target = shared_context(share);
        if (target == nullptr) {
            fail(error_invalid_operation);
            return nullptr;
        }
        share_name = target->share_name();
    }
    const uint32_t thread = slot_.thread_id();
    uint64_t       handle = 0u;
    lock_.acquire();
    const Status created = contexts_.create(format, options, handle);
    lock_.release();
    if (created != Status::ok) {
        fail(error_not_enough_memory);
        return nullptr;
    }
    GuestGlContext *const context = contexts_.resolve(handle);
    /* A context just created takes a share. */
    if (share_name != 0u) static_cast<void>(context->share(share_name));
    const Status made = context->make();
    lock_.acquire();
    const bool pended = made == Status::ok && put_pending(thread, handle);
    lock_.release();
    void *const hglrc = pended ? system_.create_context(dc) : nullptr;
    lock_.acquire();
    ScopeExit      release([&] { lock_.release(); });
    const uint64_t unclaimed = take_pending(thread);
    if (hglrc != nullptr && unclaimed == 0u) return hglrc;
    static_cast<void>(contexts_.destroy(handle));
    fail(made != Status::ok ? make_error(made, options) : error_invalid_operation);
    return nullptr;
}

const char *GlIcdRoot::extensions_string(void *)
{
    AV_SCOPE("GlIcdRoot::extensions_string");
    return extensions_.wgl_extensions();
}

bool GlIcdRoot::pixel_format_attribs(void *dc, int32_t format, int32_t layer, uint32_t count, const int32_t *names,
                                     int32_t *ints, float *floats)
{
    AV_SCOPE("GlIcdRoot::pixel_format_attribs");
    const Status answered = surface_.window_of(dc) != nullptr
                                ? query_.attribs(format, layer, count, names, ints, floats)
                                : Status::not_found;
    if (answered == Status::ok) return true;
    fail(answered == Status::not_found ? error_invalid_pixel_format : error_invalid_parameter);
    return false;
}

bool GlIcdRoot::choose_pixel_format(void *dc, const int32_t *ints, const float *floats, uint32_t max,
                                    int32_t *formats, uint32_t *count)
{
    AV_SCOPE("GlIcdRoot::choose_pixel_format");
    const bool   windowed = surface_.window_of(dc) != nullptr;
    const Status chosen = windowed ? query_.choose(ints, floats, max, formats, count)
                                   : query_.choose(ints, floats, 0u, nullptr, count);
    if (chosen != Status::ok) {
        fail(error_invalid_parameter);
        return false;
    }
    if (!windowed) *count = 0u;
    return true;
}

GuestGlContext *GlIcdRoot::current()
{
    AV_SCOPE("GlIcdRoot::current");
    return currents_.current();
}
ManualCalls    &GlIcdRoot::manual()
{
    AV_SCOPE("GlIcdRoot::manual");
    return manual_;
}

ClientTransport *GlIcdRoot::wire()
{
    AV_SCOPE("GlIcdRoot::wire");
    GuestGlContext *const context = currents_.current();
    return context != nullptr ? context->wire() : nullptr;
}

uintptr_t GlIcdRoot::record_legacy()
{
    AV_SCOPE("GlIcdRoot::record_legacy");
    GuestGlContext *const context = currents_.current();
    if (context != nullptr) context->record(gl_invalid_operation);
    return 0u;
}

uint32_t GlIcdRoot::get_error()
{
    AV_SCOPE("GlIcdRoot::get_error");
    GuestGlContext *const context = currents_.current();
    if (context == nullptr) return 0u;
    /* The wire first: that is where a context finds out it was lost, which
     * is the error it reports next. */
    ClientTransport *const transport = context->wire();
    const uint32_t         local = context->take_error();
    if (local != 0u) return local;
    return transport != nullptr ? glproto::rvk_glGetError(*transport) : 0u;
}

uint32_t GlIcdRoot::reset_status()
{
    AV_SCOPE("GlIcdRoot::reset_status");
    GuestGlContext *const context = currents_.current();
    if (context == nullptr) return 0u;
    ClientTransport *const transport = context->wire();
    return transport != nullptr ? glproto::rvk_glGetGraphicsResetStatus(*transport) : context->lost_reset_status();
}

int32_t GlIcdRoot::force_context_fault()
{
    AV_SCOPE("GlIcdRoot::force_context_fault");
    GuestGlContext *const context = currents_.current();
    if (context == nullptr) {
        fail(error_invalid_operation);
        return 0;
    }
    ClientTransport *const transport = context->wire();
    if (transport == nullptr) return 1;
    /* A glGetString with no name: the OpenGL decoder cannot take it, and an
     * OpenGL context only skips the Vulkan command 045's fault sends. */
    static_cast<void>(ContextFault(*transport, glproto::kCmd_glGetString).send());
    if (context->wire() == nullptr) return 1;
    log_.write("rvkgl: the host answered the forced fault; the context goes on");
    return 0;
}

uint64_t GlIcdRoot::mint_sync_token()
{
    AV_SCOPE("GlIcdRoot::mint_sync_token");
    return ++sync_tokens_;
}

/* What the host granted the first context this process made, named once: a
 * driver whose compatibility profile is missing is the difference between an
 * application running and every legacy call refusing, and the host's own
 * version string says which driver answered (feature 051, US1.4). */
void GlIcdRoot::note_profile(const GuestGlContext &context)
{
    AV_SCOPE("GlIcdRoot::note_profile");
    if (profile_noted_) return;
    profile_noted_ = true;
    const GrantedProfile &profile = context.profile();
    char                  line[line_bytes];
    TextBuffer            text(line, sizeof line);
    text.append("rvkgl: the host granted a ");
    if (profile.mask() == GrantedProfile::compatibility_bit)
        text.append("compatibility");
    else if (profile.mask() == GrantedProfile::core_bit)
        text.append("core");
    else if (profile.known())
        text.append("profileless");
    else
        text.append("silent");
    text.append(" context, version ");
    text.append_decimal(profile.major());
    text.append(".");
    text.append_decimal(profile.minor());
    text.append(", ");
    const char *const version = context.string(GuestGlContext::gl_version);
    text.append(version != nullptr ? version : "(no version string)");
    log_.write(text.text());
}

void GlIcdRoot::note_dropped(const char *api, const char *command, size_t written)
{
    AV_SCOPE("GlIcdRoot::note_dropped");
    char       line[line_bytes];
    TextBuffer text(line, sizeof line);
    text.append("rvkgl: ");
    text.append(api);
    text.append(" ");
    text.append(command);
    text.append(" not sent: its encoding overflowed at byte ");
    text.append_decimal(written);
    log_.write(text.text());
}

void GlIcdRoot::report(StateWriter &writer)
{
    AV_SCOPE("GlIcdRoot::report");
    writer.open("root", 0u);
    if (!lock_.try_acquire()) {
        writer.flag("busy", true);
        writer.close();
        return;
    }
    ScopeExit release([&] { lock_.release(); });
    writer.flag("busy", false);
    writer.number("callbacks", callback_count_);
    writer.number("current_anywhere", currents_.anywhere());
    writer.close();
    connection_.report(writer);
    windows_.report(writer);
    contexts_.report(writer);
}

bool GlIcdRoot::invariants_hold() const
{
    AV_SCOPE("GlIcdRoot::invariants_hold");
    if (callback_count_ > max_callbacks || proc_table_.entries != static_cast<int32_t>(generated::kProcTableSlots))
        return false;
    for (const GlProc slot : proc_table_.slots)
        if (slot == nullptr) return false;
    for (uint32_t row = 0u; row < max_pending; row++)
        if ((pending_threads_[row] == 0u) != (pending_handles_[row] == 0u)) return false;
    /* The query's own invariants are the format table's. */
    return extensions_.invariants_hold() && query_.invariants_hold() && windows_.invariants_hold() &&
           currents_.invariants_hold() &&
           connection_.invariants_hold() && contexts_.invariants_hold();
}

uint64_t GlIcdRoot::take_pending(uint32_t thread)
{
    AV_SCOPE("GlIcdRoot::take_pending");
    for (uint32_t row = 0u; row < max_pending; row++) {
        /* A thread id is never 0, so a row naming it holds a handle too. */
        if (pending_threads_[row] != thread) continue;
        const uint64_t handle = pending_handles_[row];
        pending_threads_[row] = 0u;
        pending_handles_[row] = 0u;
        return handle;
    }
    return 0u;
}

bool GlIcdRoot::put_pending(uint32_t thread, uint64_t handle)
{
    AV_SCOPE("GlIcdRoot::put_pending");
    for (uint32_t row = 0u; row < max_pending; row++) {
        if (pending_handles_[row] != 0u) continue;
        pending_threads_[row] = thread;
        pending_handles_[row] = handle;
        return true;
    }
    return false;
}

void GlIcdRoot::fail(uint32_t error)
{
    AV_SCOPE("GlIcdRoot::fail");
    slot_.set_last_error(error);
}

uint32_t GlIcdRoot::made_error(Status status) const
{
    AV_SCOPE("GlIcdRoot::made_error");
    if (status == Status::not_supported) return error_not_supported;
    return status == Status::no_memory ? error_not_enough_memory : error_invalid_operation;
}

/* WGL_ARB_create_context: a refused version is ERROR_INVALID_VERSION_ARB. The
 * host does not say which option it refused, so a context that asked for the
 * compatibility profile is taken to have been refused that. */
uint32_t GlIcdRoot::make_error(Status status, const GlContextOptions &options) const
{
    AV_SCOPE("GlIcdRoot::make_error");
    if (status != Status::invalid_parameter) return made_error(status);
    return options.profile() == GlContextOptions::Profile::compatibility ? error_invalid_profile_arb
                                                                          : error_invalid_version_arb;
}

/* Whether `first` already shares, through its recorded targets, into `second`
 * - a share that would close a loop. */
bool GlIcdRoot::shares_into(uint64_t first, uint64_t second) const
{
    AV_SCOPE("GlIcdRoot::shares_into");
    uint64_t at = first;
    for (uint32_t step = 0u; step < GlContextTable::max_contexts && at != 0u; step++) {
        if (at == second) return true;
        const GuestGlContext *const context = contexts_.resolve(at);
        at = context != nullptr ? context->share_target() : 0u;
    }
    return false;
}

/* The unmade contexts `context` shares through, made from the far end so each
 * names a made context to share; a target deleted meanwhile shares nothing,
 * and a lost one fails the make. */
Status GlIcdRoot::make_chain(GuestGlContext &context)
{
    AV_SCOPE("GlIcdRoot::make_chain");
    GuestGlContext *chain[GlContextTable::max_contexts];
    uint32_t        length = 0u;
    for (GuestGlContext *at = &context;
         at != nullptr && at->state() == GuestGlContext::State::created && length < GlContextTable::max_contexts;
         at = contexts_.resolve(at->share_target()))
        chain[length++] = at;
    while (length != 0u) {
        GuestGlContext &link = *chain[--length];
        GuestGlContext *const target = contexts_.resolve(link.share_target());
        if (target != nullptr && target->wire() == nullptr) return Status::invalid_state;
        if (target != nullptr) static_cast<void>(link.share(target->share_name()));
        const Status made = link.make();
        if (made != Status::ok) return made;
    }
    return Status::ok;
}

GuestGlContext *GlIcdRoot::shared_context(void *share)
{
    AV_SCOPE("GlIcdRoot::shared_context");
    if (callback_count_ <= callback_get_dhglrc) return nullptr;
    using GetDhglrc = uintptr_t (APIENTRY *)(void *);
    const uintptr_t handle = reinterpret_cast<GetDhglrc>(callbacks_[callback_get_dhglrc])(share);
    lock_.acquire();
    ScopeExit             release([&] { lock_.release(); });
    GuestGlContext *const target = contexts_.resolve(handle);
    if (target == nullptr) return nullptr;
    if (target->state() == GuestGlContext::State::created && make_chain(*target) != Status::ok) return nullptr;
    /* wire() is what notices a transport that faulted since. */
    return target->wire() != nullptr ? target : nullptr;
}

}  // namespace av::glicd
