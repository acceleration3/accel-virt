#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/gl_icd_rig.hpp"
#include "icd/opengl/buffer_shadows.hpp"
#include "icd/opengl/gl_entry_points.hpp"
#include "icd/opengl/gl_icd_binding.hpp"
#include "icd/opengl/gl_icd_root.hpp"
#include "icd/opengl/guest_gl_context.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/escape/av_context_record.hpp"
#include "proto/submission/fault_code.hpp"

#include "gl_protocol_ids.h"

#include <cstdint>
#include <cstring>
#include <pthread.h>

using av::GlIcdRig;
using av::MockKmt;
using av::Status;
using av::glicd::GlIcdRoot;
using Root = GlIcdRoot;

namespace av {

struct KeyedSlotsProbe {
    template <typename Key, typename Entry>
    static void corrupt_count(KeyedSlots<Key, Entry> &table, std::uint32_t count) { table.count_ = count; }
};

}

namespace av::icd {

struct IcdConnectionProbe {
    static Status find_adapter(IcdConnection &connection) { return connection.search_.find_avgpu(connection.luid_); }
    static void lose_adapter(IcdConnection &connection) { static_cast<void>(connection.search_.close()); }
};

}

namespace av::glicd {

struct GlIcdRootProbe {
    static GuestGlContext *context(GlIcdRoot &root, std::uint64_t handle) { return root.contexts_.resolve(handle); }
    static void plant_pending(GlIcdRoot &root, uint32_t thread, uint64_t handle)
    {
        uint32_t slot = 0u;
        static_cast<void>(root.pending_.insert(thread, handle, slot));
    }
    static void corrupt_pending(GlIcdRoot &root, uint32_t count) { KeyedSlotsProbe::corrupt_count(root.pending_, count); }
    static void empty_slot(GlIcdRoot &root) { root.proc_table_.slots[4] = nullptr; }
    static void miscount_table(GlIcdRoot &root) { root.proc_table_.entries = 3; }
    static void overcount_callbacks(GlIcdRoot &root) { root.callback_count_ = 10u; }
    static GlExtensionPolicy &extensions(GlIcdRoot &root) { return root.extensions_; }
    static PixelFormatTable &formats(GlIcdRoot &root) { return root.formats_; }
    static WindowTable      &windows(GlIcdRoot &root) { return root.windows_; }
    static av::icd::IcdConnection     &connection(GlIcdRoot &root) { return root.connection_; }
    static GlContextTable   &contexts(GlIcdRoot &root) { return root.contexts_; }
};

struct GlExtensionPolicyProbe {
    static void blank_wgl(GlExtensionPolicy &policy) { policy.wgl_[0] = '\0'; }
};

struct PixelFormatTableProbe {
    static void repeat_row(PixelFormatTable &table) { table.rows_[5] = table.rows_[1]; }
};

struct WindowTableProbe {
    static void plant_unformatted(WindowTable &table)
    {
        std::uint32_t slot = 0u;
        static_cast<void>(table.formats_.insert(0x77u, 0, slot));
    }
};

struct GlContextTableProbe {
    static void zero_next(GlContextTable &table) { table.next_ = 0u; }
};

}

namespace {

using ClearFn = void (APIENTRY *)(GLbitfield);
using NewListFn = void (APIENTRY *)(unsigned int, unsigned int);
using EndListFn = void (APIENTRY *)();
using CallListsFn = void (APIENTRY *)(int, unsigned int, const void *);
using GenListsFn = unsigned int (APIENTRY *)(int);
using BeginFn = void (APIENTRY *)(unsigned int);
using GetErrorFn = GLenum (APIENTRY *)();
using GetStringFn = const GLubyte *(APIENTRY *)(GLenum);
using GetIntegervFn = void (APIENTRY *)(GLenum, GLint *);
using AttribsFn = void *(APIENTRY *)(void *, void *, const std::int32_t *);
using ExtensionsFn = const char *(APIENTRY *)(void *);
using AttribivFn = std::int32_t (APIENTRY *)(void *, std::int32_t, std::int32_t, std::uint32_t, const std::int32_t *,
                                             std::int32_t *);
using AttribfvFn = std::int32_t (APIENTRY *)(void *, std::int32_t, std::int32_t, std::uint32_t, const std::int32_t *,
                                             float *);
using ChooseFn = std::int32_t (APIENTRY *)(void *, const std::int32_t *, const float *, std::uint32_t,
                                           std::int32_t *, std::uint32_t *);

constexpr std::uint32_t slot_new_list = 0u;
constexpr std::uint32_t slot_end_list = 1u;
constexpr std::uint32_t slot_call_lists = 3u;
constexpr std::uint32_t slot_gen_lists = 5u;
constexpr std::uint32_t slot_begin = 7u;
constexpr std::uint32_t slot_clear = 203u;
constexpr std::uint32_t slot_get_error = 261u;
constexpr std::uint32_t slot_get_integerv = 263u;
constexpr std::uint32_t slot_get_string = 275u;
constexpr GLenum        gl_renderer = 0x1F01u;
constexpr GLenum        gl_viewport = 0x0BA2u;
constexpr GLenum        gl_num_extensions = 0x821Du;
constexpr GLenum        gl_invalid_operation = 0x0502u;

std::uint32_t sent(const GlIcdRig &rig, std::uint32_t id)
{
    av::glicd::GuestGlContext *const current = const_cast<GlIcdRig &>(rig).root().current();
    if (current != nullptr) current->flush();
    std::uint32_t count = 0u;
    for (std::uint32_t index = 0u; index < const_cast<GlIcdRig &>(rig).kmt().commands(); index++)
        if (const_cast<GlIcdRig &>(rig).kmt().command_id(index) == id) count++;
    return count;
}

void check_error(GlIcdRig &rig, std::uint32_t expected)
{
    CHECK_EQ(rig.threads().last_error(), expected);
    rig.threads().clear_last_error();
}

}

TEST(gl_icd_root, validates_only_version_2_and_keeps_nine_callbacks)
{ GlIcdRig rig;
  CHECK(rig.root().validate_version(2u));
  CHECK(!rig.root().validate_version(1u));
  CHECK(!rig.root().validate_version(3u));
  void *procs[12] = {};
  rig.root().set_callback_procs(-1, procs);
  rig.root().set_callback_procs(3, nullptr);
  char bytes[512];
  av::StateWriter none(bytes, sizeof bytes);
  rig.root().report(none);
  CHECK(std::strncmp(none.dump(), "avgl/0 busy=false callbacks=0 ", 30u) == 0);
  rig.root().set_callback_procs(12, procs);
  av::StateWriter nine(bytes, sizeof bytes);
  rig.root().report(nine);
  CHECK(std::strncmp(nine.dump(), "avgl/0 busy=false callbacks=9 ", 30u) == 0);
  rig.root().set_callback_procs(2, procs);
  av::StateWriter two(bytes, sizeof bytes);
  rig.root().report(two);
  CHECK(std::strncmp(two.dump(), "avgl/0 busy=false callbacks=2 ", 30u) == 0);
  CHECK(rig.root().invariants_hold()); }

TEST(gl_icd_root, describes_formats_only_for_a_dc_on_a_window)
{ GlIcdRig rig;
  CHECK(rig.opengl32().load(2u, 1u) == av::FakeOpengl32::Load::loaded);
  std::uint8_t descriptor[40] = {};
  CHECK_EQ(rig.opengl32().describe_pixel_format(rig.dc(), 1, sizeof descriptor, descriptor), 24);
  CHECK_EQ(descriptor[0], 40u);
  std::uint8_t untouched[40] = {};
  CHECK_EQ(rig.opengl32().describe_pixel_format(rig.memory_dc(), 1, sizeof untouched, untouched), 0);
  CHECK_EQ(untouched[0], 0u); }

TEST(gl_icd_root, a_window_takes_one_format_and_a_memory_dc_none)
{ GlIcdRig rig;
  rig.load_with_format();
  CHECK(!rig.opengl32().set_pixel_format(rig.dc(), 1));
  check_error(rig, Root::error_invalid_pixel_format);
  CHECK(!rig.opengl32().set_pixel_format(rig.dc(), 2));
  check_error(rig, Root::error_invalid_pixel_format);
  CHECK(!rig.opengl32().set_pixel_format(rig.memory_dc(), 1));
  check_error(rig, Root::error_invalid_pixel_format);
  CHECK(!rig.opengl32().set_pixel_format(rig.other_dc(), 0));
  check_error(rig, Root::error_invalid_pixel_format);
  CHECK(!rig.opengl32().set_pixel_format(rig.other_dc(), 25));
  check_error(rig, Root::error_invalid_pixel_format);
  CHECK(rig.opengl32().set_pixel_format(rig.other_dc(), 24));
  CHECK(rig.root().invariants_hold()); }

TEST(gl_icd_root, windows_past_the_old_bound_keep_their_formats)
{ GlIcdRig rig;
  rig.load_with_format();
  constexpr std::uint32_t many = 1000u;
  static int dcs[many];
  static int windows[many];
  const std::uint32_t before = av::glicd::GlIcdRootProbe::windows(rig.root()).held();
  for (std::uint32_t index = 0u; index < many; index++) {
      rig.surface().add_dc(&dcs[index], &windows[index], 8u, 8u);
      CHECK(rig.opengl32().set_pixel_format(&dcs[index], 1 + static_cast<std::int32_t>(index % 2u)));
  }
  CHECK_EQ(av::glicd::GlIcdRootProbe::windows(rig.root()).held(), before + many);
  CHECK_EQ(av::glicd::GlIcdRootProbe::windows(rig.root()).format_of(&windows[999]), 2);
  rig.surface().close_window(&windows[3]);
  static int dc = 0, window = 0;
  rig.surface().add_dc(&dc, &window, 8u, 8u);
  CHECK(rig.opengl32().set_pixel_format(&dc, 1));
  CHECK_EQ(av::glicd::GlIcdRootProbe::windows(rig.root()).held(), before + many);
  CHECK(av::glicd::GlIcdRootProbe::windows(rig.root()).format_of(&windows[3]) == 0);
  CHECK(rig.root().invariants_hold()); }

TEST(gl_icd_root, a_window_with_no_memory_for_its_format_is_out_of_memory)
{ GlIcdRig rig;
  rig.load_with_format();
  static int dc = 0, window = 0;
  rig.surface().add_dc(&dc, &window, 8u, 8u);
  rig.platform().fail_allocation_at(1u);
  const std::uint32_t held = av::glicd::GlIcdRootProbe::windows(rig.root()).held();
  bool refused = false;
  for (std::uint32_t step = 0u; step < 64u && !refused; step++) {
      static int more_dcs[64], more_windows[64];
      rig.surface().add_dc(&more_dcs[step], &more_windows[step], 8u, 8u);
      refused = !rig.opengl32().set_pixel_format(&more_dcs[step], 1);
  }
  CHECK(refused);
  check_error(rig, Root::error_not_enough_memory);
  CHECK(av::glicd::GlIcdRootProbe::windows(rig.root()).held() >= held);
  CHECK(rig.root().invariants_hold()); }

TEST(gl_icd_root, a_swap_forgets_the_windows_that_are_gone)
{ GlIcdRig rig;
  rig.load_with_format();
  void *const context = rig.current_context();
  CHECK(context != nullptr);
  CHECK(rig.opengl32().set_pixel_format(rig.other_dc(), 1));
  CHECK_EQ(av::glicd::GlIcdRootProbe::windows(rig.root()).held(), 2u);
  rig.surface().close_window(rig.surface().window_of(rig.other_dc()));
  static_cast<void>(rig.root().swap_buffers(rig.dc()));
  CHECK_EQ(av::glicd::GlIcdRootProbe::windows(rig.root()).held(), 1u);
  CHECK(rig.root().invariants_hold()); }

TEST(gl_icd_root, nothing_reaches_the_device_until_a_context_is_made_current)
{ GlIcdRig rig;
  rig.load_with_format();
  void *const warm = rig.current_context();
  CHECK(warm != nullptr);
  CHECK(rig.opengl32().delete_context(warm));
  const std::uint32_t windows = rig.platform().live_allocations();
  const std::uint32_t enumerated = rig.kmt().calls(MockKmt::Call::enumerate_adapters);
  void *const context = rig.opengl32().create_context(rig.dc());
  CHECK(context != nullptr);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::enumerate_adapters), enumerated);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  rig.host().gl_make("host gpu", "4.6.0");
  rig.host().gl_policy(0);
  rig.host().gl_window(1u, 1u);
  CHECK(rig.opengl32().make_current(rig.dc(), context));
  CHECK(rig.kmt().open_objects() > 0u);
  CHECK_EQ(rig.opengl32().table_entries(), 336u);
  for (std::uint32_t slot = 0u; slot < 336u; slot++) CHECK(rig.opengl32().slot(slot) != nullptr);
  CHECK(rig.opengl32().slot(slot_get_error) == reinterpret_cast<void *>(&av::glicd::entry::get_error));
  CHECK(rig.opengl32().slot(slot_get_integerv) == reinterpret_cast<void *>(&av::glicd::entry::get_integerv));
  const std::uint32_t queued = rig.kmt().queued_answers();
  CHECK(rig.opengl32().make_current(rig.dc(), context));
  CHECK_EQ(rig.kmt().queued_answers(), queued);
  CHECK(rig.root().invariants_hold());
  CHECK(rig.opengl32().delete_context(context));
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.platform().live_allocations(), windows); }

TEST(gl_icd_root, a_context_needs_layer_0_and_a_window_with_a_format)
{ GlIcdRig rig;
  rig.load_with_format();
  CHECK_EQ(rig.root().create_layer_context(rig.dc(), 1), 0u);
  check_error(rig, Root::error_invalid_parameter);
  CHECK_EQ(rig.root().create_context(rig.other_dc()), 0u);
  check_error(rig, Root::error_invalid_pixel_format);
  CHECK_EQ(rig.root().create_context(rig.memory_dc()), 0u);
  check_error(rig, Root::error_invalid_pixel_format);
  CHECK_EQ(rig.root().create_context(rig.dc()), 1u);
  CHECK_EQ(rig.threads().last_error(), 0u); }

TEST(gl_icd_root, contexts_past_the_old_bound_and_one_with_no_memory)
{ GlIcdRig rig;
  rig.load_with_format();
  for (std::uint32_t index = 0u; index < 600u; index++) CHECK_EQ(rig.root().create_context(rig.dc()), index + 1u);
  CHECK_EQ(rig.root().invariants_hold(), true);
  rig.platform().fail_allocation_at(1u);
  std::uint32_t made = 600u;
  while (made < 2000u && rig.root().create_context(rig.dc()) != 0u) made++;
  CHECK(made < 2000u);
  check_error(rig, Root::error_not_enough_memory);
  CHECK(rig.root().invariants_hold()); }

TEST(gl_icd_root, set_context_refuses_a_handle_it_did_not_make_and_a_window_of_another_format)
{ GlIcdRig rig;
  rig.load_with_format();
  CHECK(rig.opengl32().set_pixel_format(rig.other_dc(), 2));
  const std::uint64_t handle = rig.root().create_context(rig.dc());
  CHECK(rig.root().set_context(rig.dc(), handle + 1u, nullptr) == nullptr);
  check_error(rig, Root::error_invalid_handle);
  CHECK(rig.root().set_context(rig.dc(), handle + 0x100000000ull, nullptr) == nullptr);
  check_error(rig, Root::error_invalid_handle);
  CHECK(rig.root().set_context(rig.other_dc(), handle, nullptr) == nullptr);
  check_error(rig, Root::error_invalid_pixel_format);
  CHECK(rig.root().set_context(rig.memory_dc(), handle, nullptr) == nullptr);
  check_error(rig, Root::error_invalid_pixel_format);
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(gl_icd_root, a_context_current_on_another_thread_is_busy_and_left_alone)
{ GlIcdRig rig;
  rig.load_with_format();
  void *const context = rig.current_context();
  CHECK(context != nullptr);
  const std::uint64_t handle = rig.opengl32().driver_context(context);
  rig.threads().switch_to(2u);
  CHECK(!rig.opengl32().make_current(rig.dc(), context));
  check_error(rig, Root::error_busy);
  CHECK(!rig.root().release_context(handle));
  CHECK(!rig.root().delete_context(handle));
  check_error(rig, Root::error_busy);
  rig.threads().switch_to(1u);
  CHECK(rig.opengl32().slot(slot_clear) != nullptr);
  CHECK(rig.root().current() != nullptr);
  CHECK(rig.root().release_context(handle));
  CHECK(!rig.root().release_context(handle));
  CHECK(!rig.root().release_context(0x77u));
  rig.threads().switch_to(2u);
  CHECK(rig.opengl32().make_current(rig.dc(), context));
  CHECK(rig.root().invariants_hold()); }

TEST(gl_icd_root, a_make_that_fails_answers_why_and_leaves_the_context_to_retry)
{ GlIcdRig rig;
  rig.load_with_format();
  const std::uint64_t handle = rig.root().create_context(rig.dc());
  rig.kmt().fail_at(MockKmt::Call::enumerate_adapters, 1u, MockKmt::invalid_parameter);
  CHECK(rig.root().set_context(rig.dc(), handle, nullptr) == nullptr);
  check_error(rig, Root::error_not_supported);
  rig.platform().fail_allocation_at(1u);
  CHECK(rig.root().set_context(rig.dc(), handle, nullptr) == nullptr);
  check_error(rig, Root::error_not_supported);
  av::icd::DeviceLink *held = nullptr;
  CHECK_STATUS(av::glicd::GlIcdRootProbe::connection(rig.root()).acquire(held), Status::ok);
  rig.platform().fail_allocation_at(1u);
  CHECK(rig.root().set_context(rig.dc(), handle, nullptr) == nullptr);
  check_error(rig, Root::error_not_enough_memory);
  av::glicd::GlIcdRootProbe::connection(rig.root()).release(*held);
  rig.host().arena_import(0x77u, 1u);
  rig.kmt().withhold_answer_at(2u);
  CHECK(rig.root().set_context(rig.dc(), handle, nullptr) == nullptr);
  check_error(rig, Root::error_invalid_operation);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK(rig.root().current() == nullptr);
  rig.host().gl_make("host gpu", "4.6");
  rig.host().gl_policy(0);
  rig.host().gl_window(1u, 1u);
  CHECK(rig.root().set_context(rig.dc(), handle, nullptr) != nullptr);
  CHECK(rig.root().invariants_hold()); }

TEST(gl_icd_root, deleting_the_current_context_releases_it_and_the_last_closes_the_device)
{ GlIcdRig rig;
  rig.load_with_format();
  void *const first = rig.current_context();
  const std::uint64_t other = rig.root().create_context(rig.dc());
  CHECK(rig.root().delete_context(other));
  CHECK(!rig.root().delete_context(other));
  check_error(rig, Root::error_invalid_handle);
  CHECK(rig.opengl32().delete_context(first));
  CHECK(rig.root().current() == nullptr);
  CHECK(rig.root().wire() == nullptr);
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(gl_icd_root, copy_and_layer_planes_are_declined)
{ GlIcdRig rig;
  CHECK(!rig.root().copy_context(1u, 2u, 0u));
  check_error(rig, Root::error_not_supported);
  CHECK(!rig.root().describe_layer_plane(rig.dc(), 1, 1, 32u, nullptr)); }

TEST(gl_icd_root, answers_gl_and_wgl_names_it_implements)
{ GlIcdRig rig;
  CHECK(rig.opengl32().driver_proc_address("wglCreateContextAttribsARB") ==
        reinterpret_cast<void *>(&av::glicd::entry::wglCreateContextAttribsARB));
  CHECK(rig.opengl32().driver_proc_address("wglGetExtensionsStringARB") ==
        reinterpret_cast<void *>(&av::glicd::entry::wglGetExtensionsStringARB));
  CHECK(rig.opengl32().driver_proc_address("glDrawBuffers") != nullptr);
  CHECK(rig.opengl32().driver_proc_address("glBindVertexArray") != nullptr);
  CHECK(rig.opengl32().driver_proc_address("wglSwapIntervalEXT") ==
        reinterpret_cast<void *>(&av::glicd::entry::wglSwapIntervalEXT));
  CHECK(rig.opengl32().driver_proc_address("wglGetSwapIntervalEXT") ==
        reinterpret_cast<void *>(&av::glicd::entry::wglGetSwapIntervalEXT));
  CHECK(rig.opengl32().driver_proc_address("glNotACommand") == nullptr);
  CHECK(rig.root().get_proc_address(nullptr) == nullptr);
  CHECK(rig.opengl32().driver_proc_address("avGetShareName") ==
        reinterpret_cast<void *>(&av::glicd::entry::avGetShareName));
  CHECK(rig.opengl32().driver_proc_address("avRunHeldCalls") ==
        reinterpret_cast<void *>(&av::glicd::entry::avRunHeldCalls));
  const auto extensions = reinterpret_cast<ExtensionsFn>(rig.opengl32().driver_proc_address("wglGetExtensionsStringARB"));
  CHECK(std::strcmp(extensions(rig.dc()), "WGL_ARB_extensions_string WGL_ARB_pixel_format WGL_ARB_create_context "
                                          "WGL_ARB_create_context_profile WGL_ARB_create_context_robustness "
                                          "WGL_ARB_multisample WGL_ARB_framebuffer_sRGB WGL_EXT_swap_control") == 0);
  CHECK(rig.root().extensions_string(rig.memory_dc()) == extensions(rig.dc())); }

TEST(gl_icd_root, a_gl_call_with_nothing_current_does_nothing)
{ GlIcdRig rig;
  rig.load_with_format();
  void *const context = rig.current_context();
  CHECK(rig.opengl32().make_current(rig.dc(), nullptr));
  const std::uint32_t commands = rig.kmt().commands();
  const ClearFn clear = rig.opengl32().gl<ClearFn>(slot_clear);
  CHECK(clear == nullptr);
  CHECK(av::glicd::entry::get_error() == 0u);
  av::glicd::entry::get_integerv(gl_viewport, nullptr);
  CHECK_EQ(rig.root().record_legacy(), 0u);
  CHECK_EQ(rig.kmt().commands(), commands);
  CHECK(rig.opengl32().delete_context(context)); }

TEST(gl_icd_root, gl_calls_go_through_the_threads_table_to_its_context)
{ GlIcdRig rig;
  rig.load_with_format();
  CHECK(rig.current_context() != nullptr);
  const std::uint32_t clears = sent(rig, av::glproto::kCmd_glClear);
  rig.opengl32().gl<ClearFn>(slot_clear)(0x4000u);
  CHECK_EQ(sent(rig, av::glproto::kCmd_glClear), clears + 1u);
  const std::uint32_t commands = rig.kmt().commands();
  const GLubyte *const renderer = rig.opengl32().gl<GetStringFn>(slot_get_string)(gl_renderer);
  CHECK(std::strcmp(reinterpret_cast<const char *>(renderer), "host gpu") == 0);
  GLint extensions = -1;
  rig.opengl32().gl<GetIntegervFn>(slot_get_integerv)(gl_num_extensions, &extensions);
  CHECK_EQ(extensions, 0);
  CHECK_EQ(rig.kmt().commands(), commands);
  std::int32_t viewport[4] = {};
  const std::int32_t answer[4] = {0, 0, 64, 48};
  rig.host().gl_bytes(reinterpret_cast<const std::uint8_t *>(answer), sizeof answer);
  rig.opengl32().gl<GetIntegervFn>(slot_get_integerv)(gl_viewport, viewport);
  CHECK_EQ(viewport[2], 64);
  CHECK_EQ(viewport[3], 48);
  CHECK_EQ(sent(rig, av::glproto::kCmd_glGetIntegerv), 9u); }

TEST(gl_icd_root, the_line_names_whichever_profile_the_host_gave)
{ GlIcdRig core;
  core.load_with_format();
  CHECK(core.current_core_context() != nullptr);
  CHECK_EQ(core.log().holding("granted a core context, version 4.6"), 1u);

  GlIcdRig profileless;
  profileless.load_with_format();
  void *const old_context = profileless.opengl32().create_context(profileless.dc());
  CHECK(old_context != nullptr);
  profileless.host().gl_make_profile("host gpu", "1.4 Mesa", 1, 4, 0);
  profileless.host().gl_policy(0);
  profileless.host().gl_window(1u, 1u);
  CHECK(profileless.opengl32().make_current(profileless.dc(), old_context));
  CHECK_EQ(profileless.log().holding("granted a profileless context, version 1.4"), 1u);

  GlIcdRig silent;
  silent.load_with_format();
  void *const quiet = silent.opengl32().create_context(silent.dc());
  CHECK(quiet != nullptr);
  silent.host().gl_make_profile("host gpu", "", 0, 0, 0);
  silent.host().gl_policy(0);
  silent.host().gl_window(1u, 1u);
  CHECK(silent.opengl32().make_current(silent.dc(), quiet));
  CHECK_EQ(silent.log().holding("granted a silent context"), 1u); }

TEST(gl_icd_root, the_profile_the_host_granted_is_logged_once)
{ GlIcdRig rig;
  rig.load_with_format();
  CHECK(rig.current_context("host gpu", "4.6 (Compatibility Profile) Mesa") != nullptr);
  CHECK_EQ(rig.log().holding("compatibility"), 1u);
  CHECK_EQ(rig.log().holding("granted a compatibility context, version 4.6"), 1u);
  CHECK(rig.current_context("host gpu", "4.6 (Compatibility Profile) Mesa") != nullptr);
  CHECK_EQ(rig.log().holding("compatibility"), 1u); }

TEST(gl_icd_root, a_legacy_command_on_a_core_context_is_invalid_operation_without_a_host_call)
{ GlIcdRig rig;
  rig.load_with_format();
  CHECK(rig.current_core_context() != nullptr);
  const std::uint32_t commands = rig.kmt().commands();
  rig.opengl32().gl<NewListFn>(slot_new_list)(1u, 0x1300u );
  CHECK_EQ(rig.opengl32().gl<GetErrorFn>(slot_get_error)(), gl_invalid_operation);
  CHECK_EQ(rig.kmt().commands(), commands);
  rig.host().gl_varint(0x0505u);
  CHECK_EQ(rig.opengl32().gl<GetErrorFn>(slot_get_error)(), 0x0505u);
  CHECK_EQ(sent(rig, av::glproto::kCmd_glGetError), 1u); }

TEST(gl_icd_root, a_legacy_command_on_a_compatibility_context_reaches_the_host)
{ GlIcdRig rig;
  rig.load_with_format();
  CHECK(rig.current_context() != nullptr);
  rig.opengl32().gl<NewListFn>(slot_new_list)(1u, 0x1300u);
  CHECK_EQ(sent(rig, av::glproto::kCmd_glNewList), 1u);
  rig.host().gl_varint(0u);
  CHECK_EQ(rig.opengl32().gl<GetErrorFn>(slot_get_error)(), 0u); }

TEST(gl_icd_root, a_display_list_is_compiled_once_and_called_with_one_command)
{ GlIcdRig rig;
  rig.load_with_format();
  CHECK(rig.current_context() != nullptr);
  rig.host().gl_varint(7u);
  CHECK_EQ(rig.opengl32().gl<GenListsFn>(slot_gen_lists)(1), 7u);
  const std::uint32_t compiled = rig.kmt().commands();
  rig.opengl32().gl<NewListFn>(slot_new_list)(7u, 0x1300u );
  rig.opengl32().gl<BeginFn>(slot_begin)(0x0004u );
  rig.opengl32().gl<EndListFn>(slot_end_list)();
  CHECK_EQ(sent(rig, av::glproto::kCmd_glNewList), 1u);
  const std::uint32_t after_compile = rig.kmt().commands();
  CHECK_EQ(sent(rig, av::glproto::kCmd_glBegin), 1u);
  CHECK_EQ(sent(rig, av::glproto::kCmd_glEndList), 1u);

  const std::uint8_t names[] = {7u, 7u};
  for (int frame = 0; frame < 3; frame++)
      rig.opengl32().gl<CallListsFn>(slot_call_lists)(2, 0x1401u , names);
  CHECK_EQ(sent(rig, av::glproto::kCmd_glCallLists), 3u);
  CHECK_EQ(sent(rig, av::glproto::kCmd_glBegin), 1u);
  CHECK_EQ(rig.kmt().commands() - after_compile, 3u);
  CHECK(after_compile > compiled); }

TEST(gl_icd_root, a_lost_context_reports_its_loss_once_and_sends_nothing)
{ GlIcdRig rig;
  rig.load_with_format();
  CHECK(rig.current_context() != nullptr);
  rig.kmt().withhold_answer_at(1u);
  CHECK_EQ(rig.opengl32().gl<GetErrorFn>(slot_get_error)(), 0u);
  const std::uint32_t commands = rig.kmt().commands();
  CHECK_EQ(rig.opengl32().gl<GetErrorFn>(slot_get_error)(), 0x0505u);
  CHECK_EQ(rig.opengl32().gl<GetErrorFn>(slot_get_error)(), 0u);
  rig.opengl32().gl<ClearFn>(slot_clear)(0x4000u);
  CHECK(rig.opengl32().swap_buffers(rig.dc()));
  CHECK_EQ(rig.kmt().commands(), commands); }

TEST(gl_icd_root_set_swap_interval, unpaced_every_swap_reads_its_frame_at_swap_time)
{ GlIcdRig rig;
  rig.load_with_format();
  using IntervalFn = std::int32_t (APIENTRY *)(std::int32_t);
  const auto set = reinterpret_cast<IntervalFn>(rig.root().get_proc_address("wglSwapIntervalEXT"));
  CHECK(rig.current_context() != nullptr);
  CHECK_EQ(set(0), 1);
  rig.host().gl_bytes(nullptr, 0u);
  CHECK(rig.opengl32().swap_buffers(rig.dc()));
  CHECK_EQ(sent(rig, av::glproto::kCmd_glGetTextureSubImage), 1u);
  CHECK_EQ(rig.surface().blits(), 1u);
  rig.host().gl_bytes(nullptr, 0u);
  CHECK(rig.opengl32().swap_buffers(rig.dc()));
  CHECK_EQ(sent(rig, av::glproto::kCmd_glGetTextureSubImage), 2u);
  CHECK_EQ(rig.surface().blits(), 2u);
  CHECK_EQ(set(1), 1);
  rig.host().gl_bytes(nullptr, 0u);
  CHECK(rig.opengl32().swap_buffers(rig.dc()));
  CHECK_EQ(sent(rig, av::glproto::kCmd_glGetTextureSubImage), 3u);
  CHECK_EQ(rig.surface().blits(), 3u);
  CHECK(rig.root().invariants_hold()); }

TEST(gl_icd_root_swap_buffers, a_swap_the_screen_does_not_show_still_sends_what_the_frame_held)
{ GlIcdRig rig;
  rig.load_with_format();
  CHECK(rig.current_context() != nullptr);
  using IntervalFn = std::int32_t (APIENTRY *)(std::int32_t);
  const auto set = reinterpret_cast<IntervalFn>(rig.root().get_proc_address("wglSwapIntervalEXT"));
  CHECK_EQ(set(0), 1);
  rig.host().gl_bytes(nullptr, 0u);
  CHECK(rig.opengl32().swap_buffers(rig.dc()));
  const std::uint32_t before = rig.kmt().commands();
  rig.opengl32().gl<ClearFn>(slot_clear)(0x4000u);
  CHECK_EQ(rig.kmt().commands(), before);
  rig.host().gl_bytes(nullptr, 0u);
  CHECK(rig.opengl32().swap_buffers(rig.dc()));
  CHECK_EQ(rig.surface().blits(), 2u);
  CHECK_EQ(rig.kmt().command_id(before), av::glproto::kCmd_glClear); }

TEST(gl_icd_root_set_swap_interval, a_swap_waits_for_the_blanks_its_interval_asks_for)
{ GlIcdRig rig;
  rig.load_with_format();
  using IntervalFn = std::int32_t (APIENTRY *)(std::int32_t);
  using GetIntervalFn = std::int32_t (APIENTRY *)();
  const auto set = reinterpret_cast<IntervalFn>(rig.root().get_proc_address("wglSwapIntervalEXT"));
  const auto get = reinterpret_cast<GetIntervalFn>(rig.root().get_proc_address("wglGetSwapIntervalEXT"));
  CHECK(set != nullptr && get != nullptr);
  CHECK_EQ(set(1), 0);
  check_error(rig, Root::error_invalid_operation);
  CHECK_EQ(get(), 0);

  CHECK(rig.current_context() != nullptr);
  CHECK_EQ(get(), 1);
  const std::uint32_t blanks = rig.kmt().calls(MockKmt::Call::wait_for_vertical_blank);
  rig.host().gl_bytes(nullptr, 0u);
  CHECK(rig.opengl32().swap_buffers(rig.dc()));
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_vertical_blank), blanks + 1u);

  CHECK_EQ(set(2), 1);
  CHECK_EQ(get(), 2);
  rig.host().gl_bytes(nullptr, 0u);
  CHECK(rig.opengl32().swap_buffers(rig.dc()));
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_vertical_blank), blanks + 3u);
  CHECK_EQ(set(0), 1);
  rig.host().gl_bytes(nullptr, 0u);
  CHECK(rig.opengl32().swap_buffers(rig.dc()));
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_vertical_blank), blanks + 3u);

  CHECK_EQ(set(-1), 0);
  check_error(rig, Root::error_invalid_data);
  CHECK_EQ(set(99), 0);
  check_error(rig, Root::error_invalid_data);
  CHECK_EQ(get(), 0);

  CHECK(rig.root().set_swap_interval(1));
  CHECK_EQ(rig.root().swap_interval(), 1);

  const char *const wgl = rig.root().extensions_string(rig.dc());
  CHECK(wgl != nullptr);
  CHECK(std::strstr(wgl, "WGL_EXT_swap_control") != nullptr);
  CHECK(std::strstr(wgl, "WGL_EXT_swap_control_tear") == nullptr); }

TEST(gl_icd_root, a_swap_on_a_window_nobody_can_see_reads_nothing_back)
{ GlIcdRig rig;
  rig.load_with_format();
  CHECK(rig.current_context() != nullptr);
  const std::uint32_t read = sent(rig, av::glproto::kCmd_glGetTextureSubImage);
  const std::uint32_t blits = rig.surface().blits();
  const std::uint32_t blanks = rig.kmt().calls(MockKmt::Call::wait_for_vertical_blank);
  rig.surface().resize(rig.window(), 0u, 0u);
  CHECK(rig.opengl32().swap_buffers(rig.dc()));
  CHECK_EQ(sent(rig, av::glproto::kCmd_glGetTextureSubImage), read);
  CHECK_EQ(rig.surface().blits(), blits);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_vertical_blank), blanks);
  rig.surface().resize(rig.window(), GlIcdRig::window_width, GlIcdRig::window_height);
  rig.host().gl_bytes(nullptr, 0u);
  CHECK(rig.opengl32().swap_buffers(rig.dc()));
  CHECK_EQ(rig.surface().blits(), blits + 1u); }

TEST(gl_icd_root, a_single_buffered_format_presents_on_flush)
{ GlIcdRig rig;
  CHECK(rig.opengl32().load(Root::icd_version, 1u) == av::FakeOpengl32::Load::loaded);
  CHECK(rig.opengl32().set_pixel_format(rig.dc(), 2));
  void *const context = rig.opengl32().create_context(rig.dc());
  CHECK(context != nullptr);
  rig.host().gl_make("host gpu", "4.6.0 host");
  rig.host().gl_policy(0);
  rig.host().gl_window(1u, 1u);
  CHECK(rig.opengl32().make_current(rig.dc(), context));
  const std::uint32_t blits = rig.surface().blits();
  rig.host().gl_bytes(nullptr, 0u);
  rig.root().flush_current();
  CHECK_EQ(rig.surface().blits(), blits + 1u);
  CHECK(rig.opengl32().make_current(nullptr, nullptr));
  CHECK(rig.opengl32().delete_context(context));
  GlIcdRig doubled;
  doubled.load_with_format();
  CHECK(doubled.current_context() != nullptr);
  const std::uint32_t untouched = doubled.surface().blits();
  doubled.root().flush_current();
  CHECK_EQ(doubled.surface().blits(), untouched); }

TEST(gl_icd_root, a_flush_with_nothing_current_presents_nothing)
{ GlIcdRig rig;
  rig.load_with_format();
  const std::uint32_t blits = rig.surface().blits();
  rig.root().flush_current();
  CHECK_EQ(rig.surface().blits(), blits);

  CHECK(rig.current_context() != nullptr);
  rig.surface().refuse_next_blit();
  rig.host().gl_bytes(nullptr, 0u);
  CHECK(rig.opengl32().swap_buffers(rig.dc()));
  CHECK_EQ(rig.log().holding("refused a frame"), 1u);
  CHECK(rig.opengl32().set_pixel_format(rig.other_dc(), 1));
  CHECK(!rig.opengl32().swap_buffers(rig.other_dc()));
  CHECK(rig.root().invariants_hold()); }

TEST(gl_icd_root, the_swap_path_refuses_what_it_cannot_do_and_touches_nothing_else)
{ GlIcdRig rig;
  rig.load_with_format();
  CHECK(rig.current_context() != nullptr);
  using IntervalFn = std::int32_t (APIENTRY *)(std::int32_t);
  const auto set = reinterpret_cast<IntervalFn>(rig.root().get_proc_address("wglSwapIntervalEXT"));

  CHECK(rig.opengl32().set_pixel_format(rig.other_dc(), 2));
  CHECK(!rig.opengl32().swap_buffers(rig.other_dc()));
  CHECK(!rig.opengl32().swap_buffers(rig.memory_dc()));
  CHECK(!rig.opengl32().swap_buffers(reinterpret_cast<void *>(0xDEADu)));
  CHECK(!rig.opengl32().swap_buffers(nullptr));

  const std::uint32_t blits = rig.surface().blits();
  rig.surface().close_window(rig.window());
  CHECK(!rig.opengl32().swap_buffers(rig.dc()));
  CHECK_EQ(rig.surface().blits(), blits);
  CHECK(rig.root().invariants_hold());

  CHECK(rig.opengl32().make_current(nullptr, nullptr));
  CHECK_EQ(set(1), 0);
  check_error(rig, Root::error_invalid_operation);
  CHECK_EQ(rig.root().swap_interval(), 0);
  CHECK(rig.root().invariants_hold()); }

TEST(gl_icd_root, a_swap_puts_the_current_contexts_frame_on_its_window)
{ GlIcdRig rig;
  rig.load_with_format();
  CHECK(!rig.opengl32().swap_buffers(rig.dc()));
  CHECK(rig.current_context() != nullptr);
  CHECK(rig.opengl32().set_pixel_format(rig.other_dc(), 2));
  CHECK(!rig.opengl32().swap_buffers(rig.other_dc()));
  CHECK(!rig.opengl32().swap_buffers(rig.memory_dc()));
  rig.host().gl_bytes(nullptr, 0u);
  CHECK(rig.opengl32().swap_buffers(rig.dc()));
  CHECK_EQ(sent(rig, av::glproto::kCmd_glGetTextureSubImage), 1u);
  CHECK_EQ(rig.surface().blits(), 1u);
  CHECK(rig.surface().last_blit_dc() == rig.dc());
  CHECK_EQ(rig.surface().last_blit_width(), GlIcdRig::window_width);
  CHECK_EQ(rig.surface().last_blit_height(), GlIcdRig::window_height);
  rig.host().gl_bytes(nullptr, 0u);
  CHECK(rig.root().swap_layer_buffers(rig.dc(), Root::swap_main_plane | 2u));
  CHECK(!rig.root().swap_layer_buffers(rig.memory_dc(), Root::swap_main_plane));
  CHECK(!rig.root().swap_layer_buffers(rig.dc(), 2u));
  CHECK_EQ(sent(rig, av::glproto::kCmd_glGetTextureSubImage), 2u);
  CHECK_EQ(rig.surface().blits(), 2u);
  CHECK(rig.root().invariants_hold()); }

TEST(gl_icd_root, with_a_present_thread_a_swap_hands_its_frame_over_and_returns)
{ GlIcdRig rig(true);
  rig.load_with_format();
  CHECK(rig.current_context() != nullptr);
  rig.host().blob_import(0u, 0x0000600000000090ull);
  const std::uint32_t queued = rig.kmt().queued_answers();
  CHECK(rig.opengl32().swap_buffers(rig.dc()));
  CHECK_EQ(rig.kmt().queued_answers(), queued - 1u);
  CHECK_EQ(sent(rig, av::glproto::kCmd_glGetTextureSubImage), 1u);
  CHECK_EQ(rig.surface().blits(), 0u);
  CHECK_EQ(rig.presents().queued(), 1u);
  CHECK_EQ(rig.handed().raises(), 1u);
  CHECK(rig.present_thread().step());
  CHECK_EQ(rig.surface().blits(), 1u);
  CHECK(rig.surface().last_blit_dc() == rig.dc());
  CHECK_EQ(rig.presents().shown_frames(), 1u);
  rig.surface().resize(rig.window(), 32u, 16u);
  rig.host().gl_window(1u, 1u, 300u);
  CHECK(rig.opengl32().swap_buffers(rig.dc()));
  CHECK_EQ(rig.presents().dropped_frames(), 1u);
  CHECK_EQ(rig.root().current()->framebuffers().name_of(0u), 300u);
  CHECK(rig.root().invariants_hold()); }

TEST(gl_icd_root, a_lost_context_swaps_without_a_host_call)
{ GlIcdRig rig;
  rig.load_with_format();
  CHECK(rig.current_context() != nullptr);
  rig.kmt().withhold_answer_at(1u);
  CHECK_EQ(rig.opengl32().gl<GetErrorFn>(slot_get_error)(), 0u);
  const std::uint32_t commands = rig.kmt().commands();
  CHECK(rig.opengl32().swap_buffers(rig.dc()));
  CHECK_EQ(rig.kmt().commands(), commands);
  CHECK_EQ(rig.surface().blits(), 0u); }

TEST(gl_icd_root, a_create_with_attributes_makes_the_context_and_opengl32s_hglrc_names_it)
{ GlIcdRig rig;
  rig.load_with_format();
  const auto create = reinterpret_cast<AttribsFn>(rig.opengl32().driver_proc_address("wglCreateContextAttribsARB"));
  const std::int32_t attributes[] = {0x2091, 4, 0x2092, 5, 0x9126, 1, 0};
  rig.host().gl_make("host gpu", "4.6");
  rig.host().gl_policy(0);
  void *const context = create(rig.dc(), nullptr, attributes);
  CHECK(context != nullptr);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  av::AvContextRecord record;
  CHECK_STATUS(record.decode(rig.kmt().last_private(), rig.kmt().last_private_size()), Status::ok);
  CHECK_EQ(record.options().major(), 4u);
  CHECK_EQ(record.options().minor(), 5u);
  CHECK(record.options().profile() == av::GlContextOptions::Profile::core);
  rig.host().gl_window(1u, 1u);
  CHECK(rig.opengl32().make_current(rig.dc(), context));
  CHECK(rig.root().current() != nullptr);
  CHECK(rig.root().invariants_hold()); }

TEST(gl_icd_root, a_create_with_attributes_shares_with_the_context_opengl32_names)
{ GlIcdRig rig;
  rig.load_with_format();
  const auto create = reinterpret_cast<AttribsFn>(rig.opengl32().driver_proc_address("wglCreateContextAttribsARB"));
  void *const first = rig.opengl32().create_context(rig.dc());
  const std::int32_t attributes[] = {0};
  rig.host().gl_make("host gpu", "4.6");
  rig.host().gl_policy(0);
  rig.host().gl_make("host gpu", "4.6");
  rig.host().gl_policy(0);
  void *const second = create(rig.dc(), first, attributes);
  CHECK(second != nullptr);
  CHECK_EQ(rig.opengl32().dhglrc_lookups(), 1u);
  using Probe = av::glicd::GlIcdRootProbe;
  CHECK(!Probe::context(rig.root(), rig.opengl32().driver_context(first))->shadows().trusted());
  CHECK(!Probe::context(rig.root(), rig.opengl32().driver_context(second))->shadows().trusted());
  av::AvContextRecord record;
  CHECK_STATUS(record.decode(rig.kmt().last_private(), rig.kmt().last_private_size()), Status::ok);
  CHECK(record.options().share_context() != 0u);
  CHECK_EQ(record.options().major(), 1u);
  const std::int32_t versioned[] = {0x2091, 3, 0x2092, 3, 0};
  rig.host().gl_make("host gpu", "4.6");
  rig.host().gl_policy(0);
  CHECK(create(rig.dc(), first, versioned) != nullptr);
  CHECK_STATUS(record.decode(rig.kmt().last_private(), rig.kmt().last_private_size()), Status::ok);
  CHECK(record.options().share_context() != 0u);
  CHECK_EQ(record.options().major(), 3u);
  CHECK_EQ(record.options().minor(), 3u);
  rig.host().gl_make("host gpu", "4.6");
  rig.host().gl_policy(0);
  CHECK(create(rig.dc(), first, attributes) != nullptr);
  CHECK(create(rig.dc(), reinterpret_cast<void *>(0x5u), attributes) == nullptr);
  check_error(rig, Root::error_invalid_operation);
  void *const unmade = rig.opengl32().create_context(rig.dc());
  rig.kmt().fail_at(MockKmt::Call::create_context_virtual, 1u, MockKmt::invalid_parameter);
  CHECK(create(rig.dc(), unmade, attributes) == nullptr);
  check_error(rig, Root::error_invalid_operation);
  rig.kmt().withhold_answer_at(1u);
  CHECK(rig.opengl32().make_current(rig.dc(), first));
  CHECK_EQ(rig.opengl32().gl<GetErrorFn>(slot_get_error)(), 0x0505u);
  CHECK_EQ(rig.opengl32().gl<GetErrorFn>(slot_get_error)(), 0u);
  CHECK(create(rig.dc(), first, attributes) == nullptr);
  check_error(rig, Root::error_invalid_operation); }

TEST(gl_icd_root, a_create_with_attributes_answers_the_arb_errors)
{ GlIcdRig rig;
  rig.load_with_format();
  const auto create = reinterpret_cast<AttribsFn>(rig.opengl32().driver_proc_address("wglCreateContextAttribsARB"));
  const std::int32_t bad_version[] = {0x2091, 4, 0x2092, 7, 0};
  CHECK(create(rig.dc(), nullptr, bad_version) == nullptr);
  check_error(rig, Root::error_invalid_version_arb);
  const std::int32_t core[] = {0x2091, 4, 0x2092, 6, 0x9126, 1, 0};
  CHECK(create(rig.other_dc(), nullptr, core) == nullptr);
  check_error(rig, Root::error_invalid_pixel_format);
  rig.host().arena_import(0x77u, 2u);
  rig.kmt().withhold_answer_at(2u);
  CHECK(create(rig.dc(), nullptr, core) == nullptr);
  check_error(rig, Root::error_invalid_version_arb);
  const std::int32_t compatibility[] = {0x2091, 4, 0x2092, 6, 0x9126, 2, 0};
  rig.host().arena_import(0x77u, 2u);
  rig.kmt().withhold_answer_at(2u);
  CHECK(create(rig.dc(), nullptr, compatibility) == nullptr);
  check_error(rig, Root::error_invalid_profile_arb);
  rig.kmt().fail_at(MockKmt::Call::create_device, 1u, MockKmt::invalid_parameter);
  CHECK(create(rig.dc(), nullptr, core) == nullptr);
  check_error(rig, Root::error_not_supported);
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(gl_icd_root, a_create_with_attributes_opengl32_does_not_finish_leaves_nothing)
{ GlIcdRig rig;
  rig.load_with_format();
  const std::int32_t none[] = {0};
  rig.host().gl_make("host gpu", "4.6");
  rig.host().gl_policy(0);
  rig.opengl32().refuse_next_create();
  CHECK(rig.root().create_context_attribs(rig.dc(), nullptr, none) == nullptr);
  check_error(rig, Root::error_invalid_operation);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  rig.host().gl_make("host gpu", "4.6");
  rig.host().gl_policy(0);
  rig.opengl32().skip_driver_next_create();
  CHECK(rig.root().create_context_attribs(rig.dc(), nullptr, none) == nullptr);
  check_error(rig, Root::error_invalid_operation);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK(rig.root().invariants_hold());
  av::glicd::GlIcdRootProbe::plant_pending(rig.root(), 1u, 1u);
  rig.host().gl_make("host gpu", "4.6");
  rig.host().gl_policy(0);
  CHECK(rig.root().create_context_attribs(rig.dc(), nullptr, none) == nullptr);
  check_error(rig, Root::error_invalid_operation);
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(gl_icd_root, a_host_with_no_session_left_fails_the_legacy_context_s_first_make_current_with_not_enough_memory)
{ std::uint32_t refused = 0u;
  for (std::uint32_t after = 0u; after < 6u; after++) {
      GlIcdRig rig;
      rig.load_with_format();
      void *const context = rig.opengl32().create_context(rig.dc());
      CHECK(context != nullptr);
      rig.host().gl_make("host gpu", "4.6");
      rig.host().gl_policy(0);
      rig.kmt().time_out_faulted_with(static_cast<std::uint32_t>(av::FaultCode::no_executor), after);
      if (rig.opengl32().make_current(rig.dc(), context)) continue;
      refused++;
      check_error(rig, Root::error_not_enough_memory);
      CHECK_EQ(rig.kmt().open_objects(), 0u);
      CHECK(rig.root().invariants_hold());
  }
  CHECK(refused >= 1u); }

TEST(gl_icd_root, a_host_with_no_session_left_fails_wglCreateContextAttribsARB_with_not_enough_memory)
{ std::uint32_t refused = 0u;
  const std::int32_t core[] = {0x2091, 4, 0x2092, 6, 0x9126, 1, 0};
  for (std::uint32_t after = 0u; after < 6u; after++) {
      GlIcdRig rig;
      rig.load_with_format();
      rig.host().gl_make("host gpu", "4.6");
      rig.host().gl_policy(0);
      rig.kmt().time_out_faulted_with(static_cast<std::uint32_t>(av::FaultCode::no_executor), after);
      if (rig.root().create_context_attribs(rig.dc(), nullptr, core) != nullptr) continue;
      refused++;
      check_error(rig, Root::error_not_enough_memory);
      CHECK_EQ(rig.kmt().open_objects(), 0u);
  }
  CHECK(refused >= 1u); }

TEST(gl_icd_root, a_create_with_attributes_and_no_room_is_out_of_memory)
{ GlIcdRig rig;
  rig.load_with_format();
  const std::int32_t none[] = {0};
  rig.platform().fail_allocation_at(1u);
  CHECK(rig.root().create_context_attribs(rig.dc(), nullptr, none) == nullptr);
  check_error(rig, Root::error_not_enough_memory); }

TEST(gl_icd_root, a_share_needs_opengl32s_callbacks)
{ GlIcdRig rig;
  void *procs[2] = {};
  rig.root().set_callback_procs(2, procs);
  CHECK(rig.opengl32().set_pixel_format(rig.dc(), 1));
  const std::int32_t none[] = {0};
  CHECK(rig.root().create_context_attribs(rig.dc(), reinterpret_cast<void *>(0x10000u), none) == nullptr);
  check_error(rig, Root::error_invalid_operation); }

TEST(gl_icd_root, reports_busy_when_its_lock_is_held_and_everything_otherwise)
{ GlIcdRig rig;
  rig.load_with_format();
  CHECK(rig.current_context() != nullptr);
  char bytes[1024];
  rig.lock().held_elsewhere(true);
  av::StateWriter busy(bytes, sizeof bytes);
  rig.root().report(busy);
  CHECK(std::strcmp(busy.dump(), "avgl/0 busy=true") == 0);
  rig.lock().held_elsewhere(false);
  av::StateWriter full(bytes, sizeof bytes);
  rig.root().report(full);
  CHECK(std::strncmp(full.dump(), "avgl/0 busy=false callbacks=9 current_anywhere=1\nconnection/0 users=1 ", 69u) == 0);
  CHECK(std::strstr(full.dump(), "\nwindows/0 held=1\n") != nullptr);
  CHECK(std::strstr(full.dump(), "\ncontexts/0 live=1 retired=0\ncontexts/0/context/1 state=made owner=1 ") != nullptr); }

TEST(gl_icd_root, a_dropped_command_is_logged_and_sync_tokens_count_up)
{ GlIcdRig rig;
  rig.root().note_dropped("gl", "glBufferData", 300000u);
  CHECK_EQ(rig.log().holding("avgl: gl glBufferData not sent: its encoding overflowed at byte 300000"), 1u);
  CHECK_EQ(rig.root().mint_sync_token(), 1u);
  CHECK_EQ(rig.root().mint_sync_token(), 2u); }

namespace {

constexpr std::uint32_t minted_per_thread = 100000u;

struct MintingThread {
    GlIcdRoot    *root;
    std::uint64_t tokens[minted_per_thread];
};

MintingThread minting[2];
bool          minted[2u * minted_per_thread + 1u];

void *mint_tokens(void *argument)
{
    MintingThread &thread = *static_cast<MintingThread *>(argument);
    for (std::uint64_t &token : thread.tokens) token = thread.root->mint_sync_token();
    return nullptr;
}

}

TEST(gl_icd_root, sync_tokens_minted_on_threads_at_once_are_each_new_and_increasing)
{ GlIcdRig rig;
  pthread_t threads[2];
  for (std::uint32_t i = 0u; i < 2u; i++) {
      minting[i].root = &rig.root();
      CHECK_EQ(pthread_create(&threads[i], nullptr, mint_tokens, &minting[i]), 0);
  }
  for (const pthread_t thread : threads) CHECK_EQ(pthread_join(thread, nullptr), 0);
  std::memset(minted, 0, sizeof minted);
  std::uint32_t repeated = 0u, backwards = 0u;
  for (const MintingThread &thread : minting) {
      std::uint64_t previous = 0u;
      for (const std::uint64_t token : thread.tokens) {
          backwards += token <= previous ? 1u : 0u;
          previous = token;
          if (token == 0u || token > 2u * minted_per_thread || minted[token]) {
              repeated++;
              continue;
          }
          minted[token] = true;
      }
  }
  CHECK_EQ(repeated, 0u);
  CHECK_EQ(backwards, 0u);
  CHECK_EQ(rig.root().mint_sync_token(), 2u * minted_per_thread + 1u); }

TEST(gl_icd_binding, holds_the_root_that_bound_itself)
{ av::glicd::GlIcdBinding binding;
  CHECK(!binding.bound());
  GlIcdRig rig;
  binding.bind(rig.root());
  CHECK(binding.bound());
  CHECK(&binding.root() == &rig.root());
  binding.unbind();
  CHECK(!binding.bound()); }

TEST(gl_icd_root, binds_itself_for_its_life)
{ CHECK(!av::glicd::gl_icd_binding.bound());
  {
    GlIcdRig rig;
    CHECK(av::glicd::gl_icd_binding.bound());
    CHECK(&av::glicd::gl_icd_binding.root() == &rig.root());
  }
  CHECK(!av::glicd::gl_icd_binding.bound()); }

TEST(gl_icd_root, a_create_takes_the_root_lock_a_call_takes_none_and_a_flush_the_send_lock)
{ GlIcdRig rig;
  rig.load_with_format();
  void *const context = rig.opengl32().create_context(rig.dc());
  rig.trace().clear();
  rig.host().gl_make("host gpu", "4.6");
  rig.host().gl_policy(0);
  rig.host().gl_window(1u, 1u);
  CHECK(rig.opengl32().make_current(rig.dc(), context));
  CHECK(std::strcmp(rig.trace().text(), "HLlh") == 0);
  rig.trace().clear();
  rig.opengl32().gl<ClearFn>(slot_clear)(0x4000u);
  CHECK(std::strcmp(rig.trace().text(), "") == 0);
  CHECK(rig.root().current() != nullptr);
  if (rig.root().current() != nullptr) rig.root().current()->flush();
  CHECK(std::strcmp(rig.trace().text(), "Ss") == 0); }

TEST(gl_icd_root_invariants, fail_for_state_no_entry_makes)
{ GlIcdRig pending;
  av::glicd::GlIcdRootProbe::plant_pending(pending.root(), 5u, 0u);
  CHECK(!pending.root().invariants_hold());
  GlIcdRig slot;
  av::glicd::GlIcdRootProbe::empty_slot(slot.root());
  CHECK(!slot.root().invariants_hold());
  GlIcdRig table;
  av::glicd::GlIcdRootProbe::miscount_table(table.root());
  CHECK(!table.root().invariants_hold());
  GlIcdRig callbacks;
  av::glicd::GlIcdRootProbe::overcount_callbacks(callbacks.root());
  CHECK(!callbacks.root().invariants_hold()); }

TEST(gl_icd_root_invariants, fail_when_a_part_it_owns_is_broken)
{ using av::glicd::GlIcdRootProbe;
  GlIcdRig extensions;
  av::glicd::GlExtensionPolicyProbe::blank_wgl(GlIcdRootProbe::extensions(extensions.root()));
  CHECK(!extensions.root().invariants_hold());
  GlIcdRig formats;
  av::glicd::PixelFormatTableProbe::repeat_row(GlIcdRootProbe::formats(formats.root()));
  CHECK(!formats.root().invariants_hold());
  GlIcdRig windows;
  av::glicd::WindowTableProbe::plant_unformatted(GlIcdRootProbe::windows(windows.root()));
  CHECK(!windows.root().invariants_hold());
  GlIcdRig connection;
  CHECK_STATUS(av::icd::IcdConnectionProbe::find_adapter(GlIcdRootProbe::connection(connection.root())),
               Status::ok);
  CHECK(!connection.root().invariants_hold());
  av::icd::IcdConnectionProbe::lose_adapter(GlIcdRootProbe::connection(connection.root()));
  CHECK(connection.root().invariants_hold());
  GlIcdRig contexts;
  av::glicd::GlContextTableProbe::zero_next(GlIcdRootProbe::contexts(contexts.root()));
  CHECK(!contexts.root().invariants_hold()); }

TEST(gl_icd_root_invariants, fail_when_a_thread_names_a_context_another_thread_owns)
{ GlIcdRig rig;
  rig.load_with_format();
  CHECK(rig.current_context() != nullptr);
  const auto *const held = rig.root().current();
  rig.threads().switch_to(2u);
  CHECK(rig.root().invariants_hold());
  rig.threads().set(const_cast<void *>(static_cast<const void *>(held)));
  CHECK(!rig.root().invariants_hold());
  rig.threads().set(nullptr);
  CHECK(rig.root().invariants_hold());
  rig.threads().switch_to(1u); }

TEST(gl_icd_root, answers_pixel_format_attributes_and_choices_through_wgl)
{ GlIcdRig rig;
  rig.load_with_format();
  const auto attribiv = reinterpret_cast<AttribivFn>(rig.opengl32().driver_proc_address("wglGetPixelFormatAttribivARB"));
  const auto attribfv = reinterpret_cast<AttribfvFn>(rig.opengl32().driver_proc_address("wglGetPixelFormatAttribfvARB"));
  const auto choose = reinterpret_cast<ChooseFn>(rig.opengl32().driver_proc_address("wglChoosePixelFormatARB"));
  CHECK(attribiv == reinterpret_cast<AttribivFn>(&av::glicd::entry::wglGetPixelFormatAttribivARB));
  CHECK(attribfv == reinterpret_cast<AttribfvFn>(&av::glicd::entry::wglGetPixelFormatAttribfvARB));
  CHECK(choose == reinterpret_cast<ChooseFn>(&av::glicd::entry::wglChoosePixelFormatARB));
  const std::int32_t names[] = {0x2022, 0x2011};
  std::int32_t values[2] = {-1, -1};
  CHECK_EQ(attribiv(rig.dc(), 1, 0, 2u, names, values), 1);
  CHECK_EQ(values[0], 24);
  CHECK_EQ(values[1], 1);
  float floats[2] = {-1.0f, -1.0f};
  CHECK_EQ(attribfv(rig.dc(), 3, 0, 2u, names, floats), 1);
  CHECK(floats[0] == 32.0f);
  CHECK_EQ(attribiv(rig.dc(), 25, 0, 2u, names, values), 0);
  check_error(rig, Root::error_invalid_pixel_format);
  const std::int32_t unknown[] = {0x1234};
  CHECK_EQ(attribfv(rig.dc(), 1, 0, 1u, unknown, floats), 0);
  check_error(rig, Root::error_invalid_parameter);
  CHECK_EQ(attribiv(rig.memory_dc(), 1, 0, 2u, names, values), 0);
  check_error(rig, Root::error_invalid_pixel_format);
  CHECK_EQ(values[0], 24);
  const std::int32_t asked[] = {0x2022, 32, 0x2011, 1, 0};
  std::int32_t formats[4] = {-1, -1, -1, -1};
  std::uint32_t count = 99u;
  CHECK_EQ(choose(rig.dc(), asked, nullptr, 4u, formats, &count), 1);
  CHECK_EQ(count, 4u);
  CHECK_EQ(formats[0], 3);
  CHECK_EQ(choose(rig.memory_dc(), asked, nullptr, 4u, formats, &count), 1);
  CHECK_EQ(count, 0u);
  CHECK_EQ(formats[0], 3);
  const std::int32_t refused[] = {0x1234, 1, 0};
  count = 99u;
  CHECK_EQ(choose(rig.dc(), refused, nullptr, 4u, formats, &count), 0);
  check_error(rig, Root::error_invalid_parameter);
  CHECK_EQ(count, 99u);
  CHECK_EQ(choose(rig.memory_dc(), asked, nullptr, 4u, formats, nullptr), 0);
  check_error(rig, Root::error_invalid_parameter);
  CHECK(rig.root().invariants_hold()); }

TEST(gl_icd_root, a_window_the_context_cannot_draw_on_refuses_the_make_current)
{ GlIcdRig rig;
  rig.load_with_format();
  void *const window = rig.surface().window_of(rig.dc());
  rig.surface().resize(window, 9000u, 10u);
  void *const context = rig.opengl32().create_context(rig.dc());
  rig.host().gl_make("host gpu", "4.6");
  rig.host().gl_policy(0);
  CHECK(!rig.opengl32().make_current(rig.dc(), context));
  check_error(rig, Root::error_invalid_pixel_format);
  CHECK(rig.root().current() == nullptr);
  rig.surface().resize(window, 8u, 8u);
  int dcs[8] = {};
  int windows[8] = {};
  rig.host().gl_window(1u, 1u);
  CHECK(rig.opengl32().make_current(rig.dc(), context));
  for (int index = 0; index < 8; index++) {
    rig.surface().add_dc(&dcs[index], &windows[index], 4u, 4u);
    CHECK(rig.opengl32().set_pixel_format(&dcs[index], 1));
    rig.host().gl_window(1u, 1u);
    CHECK(rig.opengl32().make_current(&dcs[index], context));
  }
  CHECK(rig.root().invariants_hold());
  int more_dc = 0, more_window = 0;
  rig.surface().add_dc(&more_dc, &more_window, 4u, 4u);
  CHECK(rig.opengl32().set_pixel_format(&more_dc, 1));
  rig.platform().fail_allocation_at(1u);
  CHECK(!rig.opengl32().make_current(&more_dc, context));
  rig.platform().fail_allocation_at(0u);
  check_error(rig, Root::error_not_enough_memory);
  rig.surface().resize(window, 8u, 8u);
  void *const other = rig.opengl32().create_context(rig.dc());
  rig.host().gl_make("host gpu", "4.6");
  rig.host().gl_policy(0);
  const std::uint32_t made = rig.kmt().calls(MockKmt::Call::submit);
  rig.kmt().withhold_answer_at(rig.host().gl_make_waits + av::ScriptedHost::gl_policy_waits + 1u);
  CHECK(rig.opengl32().make_current(rig.dc(), other));
  CHECK(rig.kmt().calls(MockKmt::Call::submit) > made);
  CHECK(rig.root().current() != nullptr);
  CHECK(rig.root().current()->wire() == nullptr);
  CHECK(rig.opengl32().make_current(nullptr, nullptr));
  const std::uint32_t lost = rig.kmt().commands();
  CHECK(rig.opengl32().make_current(rig.dc(), other));
  CHECK_EQ(rig.kmt().commands(), lost);
  CHECK(rig.root().invariants_hold()); }

TEST(gl_icd_root, every_external_object_entry_is_answered_by_the_half_that_owns_it)
{ GlIcdRig rig;
  struct Named { const char *name; void *own; };
  const Named substituted[] = {
      {"glDeleteMemoryObjectsEXT", reinterpret_cast<void *>(&av::glicd::entry::glDeleteMemoryObjectsEXT)},
      {"glDeleteSemaphoresEXT", reinterpret_cast<void *>(&av::glicd::entry::glDeleteSemaphoresEXT)},
      {"glBufferStorageMemEXT", reinterpret_cast<void *>(&av::glicd::entry::glBufferStorageMemEXT)},
      {"glNamedBufferStorageMemEXT",
       reinterpret_cast<void *>(&av::glicd::entry::glNamedBufferStorageMemEXT)},
      {"glTexStorageMem1DEXT", reinterpret_cast<void *>(&av::glicd::entry::glTexStorageMem1DEXT)},
      {"glTexStorageMem2DEXT", reinterpret_cast<void *>(&av::glicd::entry::glTexStorageMem2DEXT)},
      {"glTexStorageMem2DMultisampleEXT",
       reinterpret_cast<void *>(&av::glicd::entry::glTexStorageMem2DMultisampleEXT)},
      {"glTexStorageMem3DEXT", reinterpret_cast<void *>(&av::glicd::entry::glTexStorageMem3DEXT)},
      {"glTexStorageMem3DMultisampleEXT",
       reinterpret_cast<void *>(&av::glicd::entry::glTexStorageMem3DMultisampleEXT)},
      {"glTextureStorageMem1DEXT", reinterpret_cast<void *>(&av::glicd::entry::glTextureStorageMem1DEXT)},
      {"glTextureStorageMem2DEXT", reinterpret_cast<void *>(&av::glicd::entry::glTextureStorageMem2DEXT)},
      {"glTextureStorageMem2DMultisampleEXT",
       reinterpret_cast<void *>(&av::glicd::entry::glTextureStorageMem2DMultisampleEXT)},
      {"glTextureStorageMem3DEXT", reinterpret_cast<void *>(&av::glicd::entry::glTextureStorageMem3DEXT)},
      {"glTextureStorageMem3DMultisampleEXT",
       reinterpret_cast<void *>(&av::glicd::entry::glTextureStorageMem3DMultisampleEXT)},
      {"glSignalSemaphoreEXT", reinterpret_cast<void *>(&av::glicd::entry::glSignalSemaphoreEXT)},
      {"glWaitSemaphoreEXT", reinterpret_cast<void *>(&av::glicd::entry::glWaitSemaphoreEXT)},
  };
  for (const Named &entry : substituted) {
      void *const found = rig.root().get_proc_address(entry.name);
      if (found != entry.own)
          av::TestRegistry::instance().record_failure(__FILE__, __LINE__, entry.name);
  }

  const char *const local[] = {"glCreateMemoryObjectsEXT", "glGenSemaphoresEXT",
                               "glImportMemoryWin32HandleEXT", "glImportSemaphoreWin32HandleEXT",
                               "glImportMemoryWin32NameEXT", "glImportSemaphoreWin32NameEXT",
                               "glIsMemoryObjectEXT", "glIsSemaphoreEXT",
                               "glMemoryObjectParameterivEXT", "glGetMemoryObjectParameterivEXT",
                               "glSemaphoreParameterui64vEXT", "glGetSemaphoreParameterui64vEXT"};
  for (const char *const name : local) {
      if (rig.root().get_proc_address(name) == nullptr)
          av::TestRegistry::instance().record_failure(__FILE__, __LINE__, name);
  }
  CHECK(rig.root().get_proc_address("glImportMemoryWin32NameEXT") ==
        reinterpret_cast<void *>(&av::glicd::generated::Wrap_glImportMemoryWin32NameEXT));
  CHECK(rig.root().get_proc_address("glImportSemaphoreWin32NameEXT") ==
        reinterpret_cast<void *>(&av::glicd::generated::Wrap_glImportSemaphoreWin32NameEXT)); }

TEST(gl_icd_root, a_swap_on_a_window_that_is_gone_shows_nothing)
{ GlIcdRig rig;
  rig.load_with_format();
  CHECK(rig.current_context() != nullptr);
  const std::uint32_t blits = rig.surface().blits();
  rig.surface().close_window(rig.window());
  CHECK(!rig.opengl32().swap_buffers(rig.dc()));
  CHECK_EQ(rig.surface().blits(), blits);
  CHECK(rig.root().invariants_hold()); }

TEST(gl_icd_root, held_calls_run_for_the_current_context_and_not_when_its_link_has_failed)
{ GlIcdRig rig;
  CHECK_EQ(rig.root().run_held_calls(), 0);
  rig.load_with_format();
  CHECK(rig.current_context() != nullptr);
  rig.host().nop();
  CHECK_EQ(rig.root().run_held_calls(), 1);
  rig.kmt().withhold_answer_at(1u);
  CHECK_EQ(rig.opengl32().gl<GetErrorFn>(slot_get_error)(), 0u);
  CHECK_EQ(rig.root().run_held_calls(), 0); }

TEST(gl_icd_root, a_create_that_shares_leaves_nothing_behind_wherever_an_allocation_fails)
{ std::uint32_t refused = 0u;
  std::uint32_t made = 0u;
  for (std::uint32_t nth = 1u; nth <= 150u; nth++) {
      GlIcdRig rig;
      rig.load_with_format();
      const auto create = reinterpret_cast<AttribsFn>(rig.opengl32().driver_proc_address("wglCreateContextAttribsARB"));
      void *const first = rig.opengl32().create_context(rig.dc());
      CHECK(first != nullptr);
      const std::int32_t attributes[] = {0};
      rig.host().gl_make("host gpu", "4.6");
      rig.host().gl_policy(0);
      rig.host().gl_make("host gpu", "4.6");
      rig.host().gl_policy(0);
      rig.platform().fail_allocation_at(nth);
      void *const second = create(rig.dc(), first, attributes);
      rig.platform().fail_allocation_at(0u);
      CHECK(rig.root().invariants_hold());
      if (second != nullptr) {
          made++;
          continue;
      }
      refused++;
      const std::uint32_t error = rig.threads().last_error();
      CHECK(error == Root::error_not_enough_memory || error == Root::error_invalid_operation);
      rig.threads().clear_last_error();
      CHECK_EQ(av::glicd::GlIcdRootProbe::contexts(rig.root()).live(), 1u);
  }
  CHECK(refused != 0u);
  CHECK(made != 0u); }

TEST(gl_icd_root_swap_buffers, a_swap_the_screen_did_not_show_still_fixes_its_frame_at_swap_time)
{ GlIcdRig rig;
  rig.load_with_format();
  CHECK(rig.current_context() != nullptr);
  using IntervalFn = std::int32_t (APIENTRY *)(std::int32_t);
  const auto set = reinterpret_cast<IntervalFn>(rig.root().get_proc_address("wglSwapIntervalEXT"));
  CHECK_EQ(set(0), 1);
  rig.kmt().reads_clock(nullptr, 1000u);
  rig.host().gl_bytes(nullptr, 0u);
  CHECK(rig.opengl32().swap_buffers(rig.dc()));
  rig.host().gl_bytes(nullptr, 0u);
  CHECK(rig.opengl32().swap_buffers(rig.dc()));
  CHECK_EQ(rig.surface().blits(), 2u);
  CHECK(rig.root().invariants_hold()); }
