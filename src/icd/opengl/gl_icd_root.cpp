#include "icd/opengl/gl_icd_root.hpp"
#include "icd/common/window_present_queue.hpp"
#include "icd/opengl/batch_ready.hpp"

#include "core/log/log_line.hpp"
#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/report_gate.hpp"
#include "icd/opengl/granted_profile.hpp"

#include "core/platform/thread_lock.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "core/log/keyed_log.hpp"
#include "guest/transport/context_fault.hpp"
#include "icd/opengl/context_attributes.hpp"
#include "icd/opengl/gl_entry_points.hpp"
#include "icd/opengl/gl_icd_binding.hpp"
#include "icd/opengl/system_opengl.hpp"
#include "icd/opengl/thread_slot.hpp"
#include "icd/common/window_extent.hpp"
#include "icd/common/window_surface.hpp"

#include "gl_dispatch_client.h"
#include "gl_protocol_ids.h"

namespace av::glicd {

namespace {
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

constexpr uint32_t get_error_slot = 261u;
constexpr uint32_t get_integerv_slot = 263u;
constexpr const char *const reset_status_names[] = {"glGetGraphicsResetStatus", "glGetGraphicsResetStatusARB",
                                                    "glGetGraphicsResetStatusEXT", "glGetGraphicsResetStatusKHR"};
constexpr GlExtensionPolicy::Row reported_extensions[] = {
    {"GL_ARB_compatibility", 1u,
     "fullstack_opengl_icd.a_legacy_context_draws_the_fixed_function_pipeline_through_a_display_list"},
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
    {"GL_ARB_clip_control", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_ARB_shader_draw_parameters", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_EXT_texture_compression_s3tc", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_EXT_texture_compression_dxt1", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_EXT_texture_filter_anisotropic", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_EXT_texture_sRGB_decode", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_ARB_texture_compression_bptc", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_ARB_texture_compression_rgtc", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_ARB_ES3_compatibility", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_KHR_shader_subgroup", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_ARB_shader_ballot", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_ARB_shader_viewport_layer_array", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_AMD_vertex_shader_layer", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_ARB_fragment_shader_interlock", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_EXT_shader_image_load_formatted", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_KHR_blend_equation_advanced", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_KHR_blend_equation_advanced_coherent", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_ARB_gl_spirv", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_ARB_vertex_program", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
    {"GL_ARB_fragment_program", 1u, "gl_extension_policy.reports_what_rows_the_tier_and_the_host_share"},
};
constexpr uint32_t reported_count = sizeof reported_extensions / sizeof reported_extensions[0];
constexpr GlExtensionPolicy::Row reported_wgl_extensions[] = {
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

}

GlIcdRoot::GlIcdRoot(BlockSource &memory, Kmt &kmt, ThreadLock &lock, ThreadLock &send_lock,
                     ThreadLock &slice_lock, KeyedLog &log, icd::WindowSurface &surface, ThreadSlot &slot, SystemOpengl &system,
                     GlPresentQueue &presents, GlIcdBinding &binding, AdapterOrder &order)
    : lock_(lock), lines_(log), refused_(lines_), surface_(surface), slot_(slot), system_(system), presents_(presents),
      binding_(binding), formats_(),
      query_(formats_), windows_(memory), currents_(slot), preferred_(kmt, order), adapter_chosen_(false), connection_(memory, kmt, 0u),
      extensions_(reported_extensions, reported_count, reported_wgl_extensions, reported_wgl_count),
      shared_handles_(kmt, lock, memory),
      contexts_(memory, connection_, lock, send_lock, slice_lock, lines_, extensions_, shared_handles_),
      manual_(currents_, contexts_, kmt), callbacks_{}, callback_count_(0u), proc_table_(), pending_(memory, Pool::paged), sync_tokens_()
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
    const ThreadLockHold release(lock_);
    windows_.drop_gone(surface_);
    const Status set = windows_.set_format(window, index);
    if (set == Status::ok) return true;
    fail(set == Status::no_memory ? error_not_enough_memory : error_invalid_pixel_format);
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
    const ThreadLockHold release(lock_);
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
    const ThreadLockHold release(lock_);
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
    static_cast<void>(contexts_.destroy(handle));
    return true;
}

void *GlIcdRoot::set_context(void *dc, uint64_t handle, void *)
{
    AV_SCOPE("GlIcdRoot::set_context");
    const ThreadLockHold release(lock_);
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
    GuestGlContext *const previous = currents_.current();
    if (previous != nullptr && previous != context) {
        previous->flush();
    }
    if (context->state() == GuestGlContext::State::created) {
        choose_adapter(dc);
        const Status made = make_chain(*context);
        if (made != Status::ok) {
            LogLine(lines_).text("wglMakeCurrent: the context chain could not be made, status ").number(static_cast<uint32_t>(made)).text(", error ").number(made_error(made)).write();
            fail(made_error(made));
            return nullptr;
        }
        note_profile(*context);
    }
    ClientTransport *const transport = context->wire();
    if (transport != nullptr) {
        const Status on_window = context->framebuffers().make_current_on(
            *transport, surface_.window_of(dc), formats_.format(context->pixel_format()), surface_);
        if (on_window != Status::ok && context->wire() != nullptr) {
            fail(on_window == Status::invalid_parameter ? error_invalid_pixel_format : error_not_enough_memory);
            return nullptr;
        }
    }
    context->set_current_dc(dc);
    static_cast<void>(currents_.make_current(*context));
    contexts_.reap_held();
    return &proc_table_;
}

bool GlIcdRoot::release_context(uint64_t handle)
{
    AV_SCOPE("GlIcdRoot::release_context");
    const ThreadLockHold release(lock_);
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

bool GlIcdRoot::share_lists(uint64_t first, uint64_t second)
{
    AV_SCOPE("GlIcdRoot::share_lists");
    const ThreadLockHold release(lock_);
    GuestGlContext *const target = contexts_.resolve(first);
    GuestGlContext *const sharer = contexts_.resolve(second);
    if (target == nullptr || sharer == nullptr) {
        fail(error_invalid_handle);
        return false;
    }
    if (target->state() == GuestGlContext::State::lost || shares_into(first, second) ||
        contexts_.join(second, first) != Status::ok) {
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
    for (const char *const spelling : reset_status_names)
        if (__builtin_strcmp(name, spelling) == 0) return reinterpret_cast<void *>(&entry::get_graphics_reset_status);
    if (__builtin_strcmp(name, "avForceContextFault") == 0)
        return reinterpret_cast<void *>(&entry::avForceContextFault);
    if (__builtin_strcmp(name, "avGetShareName") == 0) return reinterpret_cast<void *>(&entry::avGetShareName);
    if (__builtin_strcmp(name, "avRunHeldCalls") == 0) return reinterpret_cast<void *>(&entry::avRunHeldCalls);
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

bool GlIcdRoot::swap_buffers(void *dc)
{
    AV_SCOPE("GlIcdRoot::swap_buffers");
    const Shown shown = show_frame(dc);
    if (shown == Shown::refused) return false;
    currents_.current()->end_frame();
    if (shown == Shown::handed) currents_.current()->pacing().after_present();
    contexts_.reap();
    return true;
}

GlIcdRoot::Shown GlIcdRoot::show_frame(void *dc)
{
    AV_SCOPE("GlIcdRoot::show_frame");
    {
        const ThreadLockHold locked(lock_);
        windows_.drop_gone(surface_);
    }
    GuestGlContext *const context = currents_.current();
    const bool            paced = context != nullptr && context->wire() != nullptr &&
                       windows_.format_of(surface_.window_of(dc)) == context->pixel_format() && !admits(*context);
    return present_frame(dc, paced);
}

bool GlIcdRoot::admits(GuestGlContext &context)
{
    AV_SCOPE("GlIcdRoot::admits");
    presents_.acquire();
    const bool admitted = context.pacing().admits();
    presents_.release();
    return admitted;
}

GlIcdRoot::Shown GlIcdRoot::present_frame(void *dc, bool paced)
{
    AV_SCOPE("GlIcdRoot::present_frame");
    GuestGlContext *const context = currents_.current();
    void *const           window = surface_.window_of(dc);
    if (context == nullptr || windows_.format_of(window) != context->pixel_format()) return Shown::refused;
    ClientTransport *const transport = context->wire();
    if (transport == nullptr) return Shown::nothing;
    const icd::WindowExtent::Look seen = icd::WindowExtent(surface_, window).look();
    if (seen == icd::WindowExtent::Look::gone || seen == icd::WindowExtent::Look::empty) return Shown::nothing;
    WindowFramebuffers &framebuffers = context->framebuffers();
    framebuffers.drop_gone(*transport, surface_);
    if (!framebuffers.present(*transport, context->arrays(), dc, window, surface_, &presents_,
                              context->adapter_luid()))
        return Shown::refused;
    context->flush();
    if (paced) framebuffers.hold_over(context->mark(), context->pacing());
    else framebuffers.hand_over(context->mark());
    framebuffers.follow(*transport, window, formats_.format(context->pixel_format()), surface_);
    return Shown::handed;
}

void GlIcdRoot::viewport(int32_t x, int32_t y, int32_t width, int32_t height)
{
    AV_SCOPE("GlIcdRoot::viewport");
    GuestGlContext *const context = currents_.current();
    ClientTransport *const transport = wire();
    if (context == nullptr || transport == nullptr) return;
    WindowFramebuffers &framebuffers = context->framebuffers();
    if (framebuffers.viewport_resizes(static_cast<uint32_t>(width), static_cast<uint32_t>(height)))
        framebuffers.follow(*transport, framebuffers.window(), formats_.format(context->pixel_format()), surface_);
    glproto::av_glViewport(*transport, x, y, width, height);
}

void GlIcdRoot::flush_current()
{
    AV_SCOPE("GlIcdRoot::flush_current");
    GuestGlContext *const context = currents_.current();
    show_front(context);
    if (context != nullptr) context->framebuffers().show_held_now();
    manual_.flush();
}

void GlIcdRoot::show_front(GuestGlContext *context)
{
    AV_SCOPE("GlIcdRoot::show_front");
    if (context != nullptr && !formats_.format(context->pixel_format()).double_buffer())
        static_cast<void>(show_frame(context->current_dc()));
}

void GlIcdRoot::choose_adapter(void *dc)
{
    AV_SCOPE("GlIcdRoot::choose_adapter");
    if (adapter_chosen_) return;
    adapter_chosen_ = true;
    connection_.prefer(preferred_.choose(dc));
}

void GlIcdRoot::finish_current()
{
    AV_SCOPE("GlIcdRoot::finish_current");
    GuestGlContext *const context = currents_.current();
    show_front(context);
    if (context != nullptr) context->framebuffers().show_held_now();
    manual_.finish();
}

bool GlIcdRoot::set_swap_interval(int32_t interval)
{
    AV_SCOPE("GlIcdRoot::set_swap_interval");
    GuestGlContext *const context = currents_.current();
    if (context == nullptr) {
        fail(error_invalid_operation);
        return false;
    }
    if (!context->pacing().ask(interval)) {
        fail(error_invalid_data);
        return false;
    }
    return true;
}

int32_t GlIcdRoot::swap_interval()
{
    AV_SCOPE("GlIcdRoot::swap_interval");
    GuestGlContext *const context = currents_.current();
    return context != nullptr ? static_cast<int32_t>(context->pacing().blanks()) : 0;
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
    uint64_t               share_handle = 0u;
    if (share != nullptr) {
        GuestGlContext *const target = shared_context(share, share_handle);
        if (target == nullptr) {
            fail(error_invalid_operation);
            return nullptr;
        }
        share_name = target->share_name();
    }
    const uint32_t thread = slot_.thread_id();
    uint64_t       handle = 0u;
    Status         created = Status::ok;
    {
        const ThreadLockHold locked(lock_);
        choose_adapter(dc);
        created = contexts_.create(format, options, handle);
    }
    if (created != Status::ok) {
        fail(error_not_enough_memory);
        return nullptr;
    }
    GuestGlContext *const context = contexts_.resolve(handle);
    Status                grouped = Status::ok;
    if (share_name != 0u) {
        const ThreadLockHold locked(lock_);
        grouped = contexts_.join(handle, share_handle);
        if (grouped == Status::ok) static_cast<void>(context->share(share_name));
    }
    const Status made = grouped == Status::ok ? context->make() : grouped;
    Status       pended = made;
    if (made == Status::ok) {
        const ThreadLockHold locked(lock_);
        pended = put_pending(thread, handle);
    }
    void *const hglrc = pended == Status::ok ? system_.create_context(dc) : nullptr;
    const ThreadLockHold release(lock_);
    const uint64_t unclaimed = take_pending(thread);
    if (hglrc != nullptr && unclaimed == 0u) return hglrc;
    static_cast<void>(contexts_.destroy(handle));
    if (made != Status::ok)
        fail(make_error(made, options));
    else
        fail(pended == Status::no_memory ? error_not_enough_memory : error_invalid_operation);
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
    ClientTransport *const transport = context != nullptr ? context->wire() : nullptr;
    if (transport != nullptr) context->send_forgets();
    return transport;
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
    ClientTransport *const transport = context->wire();
    const uint32_t         local = context->take_error();
    if (local != 0u) return local;
    uint32_t published = 0u;
    if (context->take_host_error(published)) return published;
    if (transport == nullptr) return 0u;
    const uint32_t asked = glproto::av_glGetError(*transport);
    if (asked != 0u) return asked;
    return context->take_host_error(published) ? published : 0u;
}

uint32_t GlIcdRoot::reset_status()
{
    AV_SCOPE("GlIcdRoot::reset_status");
    GuestGlContext *const context = currents_.current();
    if (context == nullptr) return 0u;
    ClientTransport *const transport = context->wire();
    return transport != nullptr ? glproto::av_glGetGraphicsResetStatus(*transport) : context->lost_reset_status();
}

int32_t GlIcdRoot::run_held_calls()
{
    AV_SCOPE("GlIcdRoot::run_held_calls");
    GuestGlContext *const context = currents_.current();
    return context != nullptr && context->run_held() == Status::ok ? 1 : 0;
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
    static_cast<void>(ContextFault(*transport, glproto::kCmd_glGetString).send());
    if (context->wire() == nullptr) return 1;
    lines_.write("the host answered the forced fault; the context goes on");
    return 0;
}

uint64_t GlIcdRoot::mint_sync_token()
{
    AV_SCOPE("GlIcdRoot::mint_sync_token");
    return sync_tokens_.next();
}

void GlIcdRoot::note_profile(const GuestGlContext &context)
{
    AV_SCOPE("GlIcdRoot::note_profile");
    if (!lines_.first("the host granted a profile", 0)) return;
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
    refused_.dropped(api, command, written);
}

void GlIcdRoot::report(StateWriter &writer)
{
    AV_SCOPE("GlIcdRoot::report");
    const ReportGate gate(lock_, writer, "avgl");
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
    bool pending_named = true;
    pending_.for_each([&pending_named](uint32_t thread, const uint64_t &handle) {
        pending_named = pending_named && thread != 0u && handle != 0u;
    });
    if (!pending_named || !pending_.invariants_hold()) return false;
    return extensions_.invariants_hold() && query_.invariants_hold() && windows_.invariants_hold() &&
           currents_.invariants_hold() &&
           connection_.invariants_hold() && contexts_.invariants_hold();
}

uint64_t GlIcdRoot::take_pending(uint32_t thread)
{
    AV_SCOPE("GlIcdRoot::take_pending");
    uint64_t handle = 0u;
    static_cast<void>(pending_.take(thread, handle));
    return handle;
}

Status GlIcdRoot::put_pending(uint32_t thread, uint64_t handle)
{
    AV_SCOPE("GlIcdRoot::put_pending");
    uint32_t slot = 0u;
    return pending_.insert(thread, handle, slot);
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

uint32_t GlIcdRoot::make_error(Status status, const GlContextOptions &options) const
{
    AV_SCOPE("GlIcdRoot::make_error");
    if (status != Status::invalid_parameter) return made_error(status);
    return options.profile() == GlContextOptions::Profile::compatibility ? error_invalid_profile_arb
                                                                          : error_invalid_version_arb;
}

bool GlIcdRoot::shares_into(uint64_t first, uint64_t second) const
{
    AV_SCOPE("GlIcdRoot::shares_into");
    uint64_t at = first;
    for (uint32_t step = 0u; step <= contexts_.live() && at != 0u; step++) {
        if (at == second) return true;
        const GuestGlContext *const context = contexts_.resolve(at);
        at = context != nullptr ? context->share_target() : 0u;
    }
    return false;
}

Status GlIcdRoot::make_chain(GuestGlContext &context)
{
    AV_SCOPE("GlIcdRoot::make_chain");
    while (context.state() == GuestGlContext::State::created) {
        GuestGlContext &link = farthest_unmade(context);
        GuestGlContext *const target = contexts_.resolve(link.share_target());
        if (target != nullptr && !target->shareable()) return Status::invalid_state;
        if (target != nullptr) {
            target->lend();
            static_cast<void>(link.share(target->share_name()));
        }
        const Status made = link.make();
        if (made != Status::ok) return made;
    }
    return Status::ok;
}

GuestGlContext &GlIcdRoot::farthest_unmade(GuestGlContext &context) const
{
    AV_SCOPE("GlIcdRoot::farthest_unmade");
    GuestGlContext *link = &context;
    for (uint32_t step = 0u; step < contexts_.live(); step++) {
        GuestGlContext *const next = contexts_.resolve(link->share_target());
        if (next == nullptr || next->state() != GuestGlContext::State::created) break;
        link = next;
    }
    return *link;
}

GuestGlContext *GlIcdRoot::shared_context(void *share, uint64_t &handle)
{
    AV_SCOPE("GlIcdRoot::shared_context");
    handle = 0u;
    if (callback_count_ <= callback_get_dhglrc) return nullptr;
    using GetDhglrc = uintptr_t (APIENTRY *)(void *);
    const uintptr_t found = reinterpret_cast<GetDhglrc>(callbacks_[callback_get_dhglrc])(share);
    const ThreadLockHold release(lock_);
    GuestGlContext *const target = contexts_.resolve(found);
    if (target == nullptr) return nullptr;
    if (target->state() == GuestGlContext::State::created) {
        choose_adapter(nullptr);
        if (make_chain(*target) != Status::ok) return nullptr;
    }
    if (!target->shareable()) return nullptr;
    handle = found;
    return target;
}

}
