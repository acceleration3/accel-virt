#include "icd/common/report_gate.hpp"
#include "icd/opengl/gl_icd_root.hpp"

#include "core/support/log_line.hpp"
#include "icd/common/thread_lock_hold.hpp"
#include "icd/opengl/granted_profile.hpp"
#include "icd/opengl/window_visibility.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/common/extension_policy.hpp"
#include "core/support/log_sink.hpp"
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

/* GL_EXT_memory_object_win32 and GL_EXT_semaphore_win32, answered here rather
 * than by the generated table (feature 053). */
struct SharedObjectEntry {
    const char *name;
    void       *address;
};

const SharedObjectEntry shared_object_entries[] = {
    {"glDeleteMemoryObjectsEXT", reinterpret_cast<void *>(&entry::glDeleteMemoryObjectsEXT)},
    {"glDeleteSemaphoresEXT", reinterpret_cast<void *>(&entry::glDeleteSemaphoresEXT)},
    {"glBufferStorageMemEXT", reinterpret_cast<void *>(&entry::glBufferStorageMemEXT)},
    {"glTextureStorageMem2DEXT", reinterpret_cast<void *>(&entry::glTextureStorageMem2DEXT)},
    {"glSignalSemaphoreEXT", reinterpret_cast<void *>(&entry::glSignalSemaphoreEXT)},
    {"glWaitSemaphoreEXT", reinterpret_cast<void *>(&entry::glWaitSemaphoreEXT)},
    /* The rest of what the four rows promise (feature 053, T037). The queries
     * and parameters are answered here because they are about an object the
     * host has not been told of; the ten storage commands are generated and
     * are here to substitute the host's name before they go out; the two name
     * imports are answered to refuse them, which is a driver saying no rather
     * than an entry point that is not there. */
    {"glNamedBufferStorageMemEXT", reinterpret_cast<void *>(&entry::glNamedBufferStorageMemEXT)},
    {"glTexStorageMem1DEXT", reinterpret_cast<void *>(&entry::glTexStorageMem1DEXT)},
    {"glTexStorageMem2DEXT", reinterpret_cast<void *>(&entry::glTexStorageMem2DEXT)},
    {"glTexStorageMem2DMultisampleEXT", reinterpret_cast<void *>(&entry::glTexStorageMem2DMultisampleEXT)},
    {"glTexStorageMem3DEXT", reinterpret_cast<void *>(&entry::glTexStorageMem3DEXT)},
    {"glTexStorageMem3DMultisampleEXT", reinterpret_cast<void *>(&entry::glTexStorageMem3DMultisampleEXT)},
    {"glTextureStorageMem1DEXT", reinterpret_cast<void *>(&entry::glTextureStorageMem1DEXT)},
    {"glTextureStorageMem2DMultisampleEXT",
     reinterpret_cast<void *>(&entry::glTextureStorageMem2DMultisampleEXT)},
    {"glTextureStorageMem3DEXT", reinterpret_cast<void *>(&entry::glTextureStorageMem3DEXT)},
    {"glTextureStorageMem3DMultisampleEXT",
     reinterpret_cast<void *>(&entry::glTextureStorageMem3DMultisampleEXT)},
};

/* glGetError's slot in opengl32's table (tools/codegen/profiles/opengl_proc_table.txt). */
constexpr uint32_t get_error_slot = 261u;
constexpr uint32_t get_integerv_slot = 263u;
/* ARB_robustness, KHR_robustness, EXT_robustness and GL 4.5 all name one query. */
constexpr const char *const reset_status_names[] = {"glGetGraphicsResetStatus", "glGetGraphicsResetStatusARB",
                                                    "glGetGraphicsResetStatusEXT", "glGetGraphicsResetStatusKHR"};
/* The GL extensions reported: a row arrives with the test that proves its
 * extension through this ICD (tools/check_extension_policy.py), and is still
 * only reported where the host's own list has it. */
constexpr icd::ExtensionPolicy::Row reported_extensions[] = {
    {"GL_ARB_compatibility", 1u,
     "fullstack_opengl_icd.a_legacy_context_draws_the_fixed_function_pipeline_through_a_display_list"},
    /* The external objects (feature 053, T037), each answered over the host's
     * own fd spelling: a guest has no file descriptors, which is the whole
     * reason the Win32 names exist. All four are reported together or not at
     * all - the registry has each _win32 row require its base extension, and
     * the base extensions are what the host's fd ones make possible. */
    {"GL_EXT_memory_object", 1u,
     "manual_calls.every_storage_command_names_the_hosts_memory_object", "GL_EXT_memory_object_fd"},
    {"GL_EXT_memory_object_win32", 1u,
     "manual_calls.an_import_puts_the_hosts_name_on_the_wire_and_a_delete_releases_the_object",
     "GL_EXT_memory_object_fd"},
    {"GL_EXT_semaphore", 1u,
     "manual_calls.the_queries_and_parameters_are_answered_without_the_host", "GL_EXT_semaphore_fd"},
    {"GL_EXT_semaphore_win32", 1u,
     "manual_calls.an_imported_name_crosses_as_the_hosts_own_on_every_command_that_takes_one",
     "GL_EXT_semaphore_fd"},
    /* What OpenGL 2.x and 3.0 promoted to core, which a 4.6 context does
     * through the same commands. An application of that era asks the
     * extension string for them instead of the version: Photoshop CS6's GPU
     * check turns its canvas acceleration off without NPOT, framebuffer
     * objects and the shading language named there. */
    {"GL_ARB_texture_non_power_of_two", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_ARB_framebuffer_object", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_EXT_framebuffer_object", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_EXT_framebuffer_blit", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_ARB_shading_language_100", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_ARB_shader_objects", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_ARB_vertex_shader", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_ARB_fragment_shader", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_ARB_vertex_buffer_object", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_ARB_pixel_buffer_object", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_ARB_texture_float", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_ARB_texture_rectangle", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_ARB_multitexture", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
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
                     LogSink &log, icd::WindowSurface &surface, ThreadSlot &slot, SystemOpengl &system,
                     GlPresentLoop &presents, GlIcdBinding &binding)
    : lock_(lock), log_(log), lines_(log, "avgl: "), surface_(surface), slot_(slot), system_(system), presents_(presents),
      binding_(binding), formats_(),
      query_(formats_), windows_(), currents_(slot), connection_(kmt),
      extensions_(reported_extensions, reported_count, reported_wgl_extensions, reported_wgl_count),
      shared_handles_(kmt, lock),
      contexts_(memory, connection_, send_lock, log, extensions_, shared_handles_),
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
    const icd::ThreadLockHold release(lock_);
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
    const icd::ThreadLockHold release(lock_);
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
    const icd::ThreadLockHold release(lock_);
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
    const icd::ThreadLockHold release(lock_);
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
    /* A context the thread leaves may be read from another next (a shared
     * object, a context the application makes current elsewhere): what it
     * holds goes first. */
    GuestGlContext *const previous = currents_.current();
    if (previous != nullptr && previous != context) previous->flush();
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
    const icd::ThreadLockHold release(lock_);
    GuestGlContext *const context = contexts_.resolve(handle);
    if (context == nullptr || currents_.current() != context) return false;
    context->flush();
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
    const icd::ThreadLockHold release(lock_);
    GuestGlContext *const target = contexts_.resolve(first);
    GuestGlContext *const sharer = contexts_.resolve(second);
    if (target == nullptr || sharer == nullptr) {
        fail(error_invalid_handle);
        return false;
    }
    /* Only the context taking the share must hold no object yet. The one it
     * shares may be in use: a made one is named by its ring, and an unmade
     * one is made first (make_chain). */
    if (target->state() == GuestGlContext::State::lost || shares_into(first, second) ||
        sharer->share_after(first) != Status::ok) {
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
    if (__builtin_strcmp(name, "avForceContextFault") == 0)
        return reinterpret_cast<void *>(&entry::avForceContextFault);
    if (__builtin_strcmp(name, "avGetShareName") == 0) return reinterpret_cast<void *>(&entry::avGetShareName);
    /* Before the generated table on purpose: four of these the generator never
     * emitted, and the other four it did - and those must substitute the
     * host's name for the one the application holds, so this ICD's answer has
     * to win (feature 053, T036). */
    for (const SharedObjectEntry &row : shared_object_entries)
        if (__builtin_strcmp(name, row.name) == 0) return row.address;
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

/* The calling thread's context puts its frame for the DC's window there, and
 * waits out the interval after it (feature 051, US2.2). */
bool GlIcdRoot::swap_buffers(void *dc)
{
    AV_SCOPE("GlIcdRoot::swap_buffers");
    const Shown shown = show_frame(dc);
    if (shown == Shown::handed) currents_.current()->pace();
    return shown != Shown::refused;
}

GlIcdRoot::Shown GlIcdRoot::show_frame(void *dc)
{
    AV_SCOPE("GlIcdRoot::show_frame");
    lock_.acquire();
    windows_.drop_gone(surface_);
    lock_.release();
    GuestGlContext *const context = currents_.current();
    void *const           window = surface_.window_of(dc);
    if (context == nullptr || windows_.format_of(window) != context->pixel_format()) return Shown::refused;
    ClientTransport *const transport = context->wire();
    /* A lost context has nothing to show and fails nothing by swapping. */
    if (transport == nullptr) return Shown::nothing;
    /* Nor does a window nobody can see: the frame is read back through the
     * host GPU and blitted for a client area that is not there (feature 051,
     * US2.3). */
    if (!WindowVisibility(surface_, window).showing()) return Shown::nothing;
    /* Minecraft swapped 175 times a second and every frame was read back and
     * blitted onto its window for DWM to show 60 of. */
    if (!context->admits()) return Shown::nothing;
    WindowFramebuffers &framebuffers = context->framebuffers();
    framebuffers.drop_gone(*transport, surface_);
    if (!framebuffers.present(*transport, context->arrays(), dc, window, surface_, &presents_,
                              context->adapter_luid()))
        return Shown::refused;
    /* The frame's read-back goes out with the calls before it, and the present
     * thread is handed the frame with the batch it went in. Only then may the
     * window's new size replace the room the frame is being read into. */
    context->flush();
    framebuffers.hand_over(context->mark());
    framebuffers.follow(*transport, window, formats_.format(context->pixel_format()), surface_);
    return Shown::handed;
}

void GlIcdRoot::flush_current()
{
    AV_SCOPE("GlIcdRoot::flush_current");
    const GuestGlContext *const context = currents_.current();
    if (context != nullptr && !formats_.format(context->pixel_format()).double_buffer())
        static_cast<void>(show_frame(context->current_dc()));
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
        target->lend();
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
    const icd::ThreadLockHold release(lock_);
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
    /* From the word the host publishes, without a round trip: an error shows
     * once the host has run the command that raised it, which for a command
     * still queued is a later call - where OpenGL would have answered it
     * already, having run it. */
    uint32_t published = 0u;
    if (context->take_host_error(published)) return published;
    return transport != nullptr ? glproto::av_glGetError(*transport) : 0u;
}

uint32_t GlIcdRoot::reset_status()
{
    AV_SCOPE("GlIcdRoot::reset_status");
    GuestGlContext *const context = currents_.current();
    if (context == nullptr) return 0u;
    ClientTransport *const transport = context->wire();
    return transport != nullptr ? glproto::av_glGetGraphicsResetStatus(*transport) : context->lost_reset_status();
}

uint32_t GlIcdRoot::current_share_name()
{
    AV_SCOPE("GlIcdRoot::current_share_name");
    const GuestGlContext *const context = currents_.current();
    return context != nullptr ? context->share_name() : 0u;
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
    log_.write("avgl: the host answered the forced fault; the context goes on");
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
    LogLine line(lines_);
    line.text("the host granted a ");
    if (profile.mask() == GrantedProfile::compatibility_bit)
        line.text("compatibility");
    else if (profile.mask() == GrantedProfile::core_bit)
        line.text("core");
    else if (profile.known())
        line.text("profileless");
    else
        line.text("silent");
    line.text(" context, version ").number(profile.major()).text(".").number(profile.minor()).write();
}

void GlIcdRoot::note_dropped(const char *api, const char *command, size_t written)
{
    AV_SCOPE("GlIcdRoot::note_dropped");
    LogLine(lines_).text(api).text(" ").text(command).text(" not sent: its encoding overflowed at byte ").number(written).write();
}

void GlIcdRoot::report(StateWriter &writer)
{
    AV_SCOPE("GlIcdRoot::report");
    const icd::ReportGate gate(lock_, writer, "root");
    if (!gate.held()) return;
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
        if (target != nullptr) {
            target->lend();
            static_cast<void>(link.share(target->share_name()));
        }
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
    const icd::ThreadLockHold release(lock_);
    GuestGlContext *const target = contexts_.resolve(handle);
    if (target == nullptr) return nullptr;
    if (target->state() == GuestGlContext::State::created && make_chain(*target) != Status::ok) return nullptr;
    /* wire() is what notices a transport that faulted since. */
    return target->wire() != nullptr ? target : nullptr;
}

}  // namespace av::glicd
