/* rvkgl.dll as opengl32 drives it (contracts/icd_interface.md), through
 * FakeOpengl32 over MockKmt and a scripted host: the load, pixel formats,
 * contexts made at their first make-current, every refusal and the error it
 * sets, GL calls through the proc table, and the WGL extension functions. */
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/gl_icd_rig.hpp"
#include "icd/opengl/gl_entry_points.hpp"
#include "icd/opengl/gl_icd_binding.hpp"
#include "icd/opengl/gl_icd_root.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/context/rvk_context_record.hpp"

#include "gl_protocol_ids.h"

#include <cstdint>
#include <cstring>

using av::GlIcdRig;
using av::MockKmt;
using av::Status;
using av::glicd::GlIcdRoot;
using Root = GlIcdRoot;

namespace av::glicd {

struct GlIcdRootProbe {
    static void fill_pending(GlIcdRoot &root)
    {
        for (uint32_t row = 0u; row < GlIcdRoot::max_pending; row++) {
            root.pending_threads_[row] = 100u + row;
            root.pending_handles_[row] = 1u;
        }
    }
    static void unpair_pending(GlIcdRoot &root) { root.pending_threads_[0] = 5u; }
    static void empty_slot(GlIcdRoot &root) { root.proc_table_.slots[4] = nullptr; }
    static void miscount_table(GlIcdRoot &root) { root.proc_table_.entries = 3; }
    static void overcount_callbacks(GlIcdRoot &root) { root.callback_count_ = 10u; }
    static GlExtensionPolicy &extensions(GlIcdRoot &root) { return root.extensions_; }
    static PixelFormatTable &formats(GlIcdRoot &root) { return root.formats_; }
    static WindowTable      &windows(GlIcdRoot &root) { return root.windows_; }
    static GlConnection     &connection(GlIcdRoot &root) { return root.connection_; }
    static GlContextTable   &contexts(GlIcdRoot &root) { return root.contexts_; }
};

struct GlExtensionPolicyProbe {
    static void overcount(GlExtensionPolicy &policy) { policy.gl_count_ = GlExtensionPolicy::max_rows + 1u; }
};

struct PixelFormatTableProbe {
    static void repeat_row(PixelFormatTable &table) { table.rows_[5] = table.rows_[1]; }
};

struct WindowTableProbe {
    static void overfill(WindowTable &table) { table.count_ = WindowTable::max_windows + 1u; }
};

struct GlConnectionProbe {
    static void set_users(GlConnection &connection, uint32_t users) { connection.users_ = users; }
};

struct GlContextTableProbe {
    static void zero_generation(GlContextTable &table) { table.generations_[3] = 0u; }
};

}  // namespace av::glicd

namespace {

using ClearFn = void (APIENTRY *)(GLbitfield);
/* The legacy recorder's own type: Win64 lets opengl32 call it as any GL 1.1
 * signature, which UBSan's function check would flag here. */
using LegacyFn = std::uintptr_t (APIENTRY *)();
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

}  // namespace

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
  CHECK(std::strncmp(none.dump(), "root/0 busy=false callbacks=0 ", 30u) == 0);
  rig.root().set_callback_procs(12, procs);
  av::StateWriter nine(bytes, sizeof bytes);
  rig.root().report(nine);
  CHECK(std::strncmp(nine.dump(), "root/0 busy=false callbacks=9 ", 30u) == 0);
  rig.root().set_callback_procs(2, procs);
  av::StateWriter two(bytes, sizeof bytes);
  rig.root().report(two);
  CHECK(std::strncmp(two.dump(), "root/0 busy=false callbacks=2 ", 30u) == 0);
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

TEST(gl_icd_root, a_257th_window_is_out_of_memory)
{ GlIcdRig rig;
  rig.load_with_format();
  static int dcs[256];
  static int windows[256];
  for (std::uint32_t index = 0u; index < 256u; index++) {
      rig.surface().add_dc(&dcs[index], &windows[index], 8u, 8u);
      const bool set = rig.opengl32().set_pixel_format(&dcs[index], 1);
      CHECK_EQ(set, index < 255u);
  }
  check_error(rig, Root::error_not_enough_memory);
  /* A closed window's row makes room for the next. */
  rig.surface().close_window(&windows[3]);
  CHECK(rig.opengl32().set_pixel_format(&dcs[255], 1));
  CHECK_EQ(av::glicd::GlIcdRootProbe::windows(rig.root()).held(), 256u);
  CHECK(av::glicd::GlIcdRootProbe::windows(rig.root()).format_of(&windows[3]) == 0);
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
  void *const context = rig.opengl32().create_context(rig.dc());
  CHECK(context != nullptr);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::enumerate_adapters), 0u);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  rig.host().gl_make("host gpu", "4.6.0");
  rig.host().gl_window(1u, 1u);
  CHECK(rig.opengl32().make_current(rig.dc(), context));
  CHECK(rig.kmt().open_objects() > 0u);
  CHECK_EQ(rig.opengl32().table_entries(), 336u);
  for (std::uint32_t slot = 0u; slot < 336u; slot++) CHECK(rig.opengl32().slot(slot) != nullptr);
  CHECK(rig.opengl32().slot(slot_get_error) == reinterpret_cast<void *>(&av::glicd::entry::get_error));
  CHECK(rig.opengl32().slot(slot_get_integerv) == reinterpret_cast<void *>(&av::glicd::entry::get_integerv));
  /* A second make-current of a made context asks the host nothing. */
  const std::uint32_t queued = rig.kmt().queued_answers();
  CHECK(rig.opengl32().make_current(rig.dc(), context));
  CHECK_EQ(rig.kmt().queued_answers(), queued);
  CHECK(rig.root().invariants_hold());
  CHECK(rig.opengl32().delete_context(context));
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.platform().live_allocations(), 0u); }

TEST(gl_icd_root, a_context_needs_layer_0_and_a_window_with_a_format)
{ GlIcdRig rig;
  rig.load_with_format();
  CHECK_EQ(rig.root().create_layer_context(rig.dc(), 1), 0u);
  check_error(rig, Root::error_invalid_parameter);
  CHECK_EQ(rig.root().create_context(rig.other_dc()), 0u);
  check_error(rig, Root::error_invalid_pixel_format);
  CHECK_EQ(rig.root().create_context(rig.memory_dc()), 0u);
  check_error(rig, Root::error_invalid_pixel_format);
  CHECK_EQ(rig.root().create_context(rig.dc()), 0x10001u);
  CHECK_EQ(rig.threads().last_error(), 0u); }

TEST(gl_icd_root, a_257th_context_is_out_of_memory)
{ GlIcdRig rig;
  rig.load_with_format();
  for (std::uint32_t index = 0u; index < 256u; index++) CHECK(rig.root().create_context(rig.dc()) != 0u);
  CHECK_EQ(rig.root().create_context(rig.dc()), 0u);
  check_error(rig, Root::error_not_enough_memory); }

TEST(gl_icd_root, set_context_refuses_a_handle_it_did_not_make_and_a_window_of_another_format)
{ GlIcdRig rig;
  rig.load_with_format();
  CHECK(rig.opengl32().set_pixel_format(rig.other_dc(), 2));
  const std::uint64_t handle = rig.root().create_context(rig.dc());
  CHECK(rig.root().set_context(rig.dc(), 0x10002u, nullptr) == nullptr);
  check_error(rig, Root::error_invalid_handle);
  CHECK(rig.root().set_context(rig.dc(), handle + 0x10000u, nullptr) == nullptr);
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
  check_error(rig, Root::error_not_enough_memory);
  rig.host().blob_import(0u, 0x77u);
  rig.kmt().withhold_answer_at(2u);
  CHECK(rig.root().set_context(rig.dc(), handle, nullptr) == nullptr);
  check_error(rig, Root::error_invalid_operation);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK(rig.root().current() == nullptr);
  rig.host().gl_make("host gpu", "4.6");
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

TEST(gl_icd_root, copy_share_and_layer_planes_are_declined)
{ GlIcdRig rig;
  CHECK(!rig.root().copy_context(1u, 2u, 0u));
  check_error(rig, Root::error_not_supported);
  CHECK(!rig.root().share_lists(1u, 2u));
  check_error(rig, Root::error_not_supported);
  CHECK(!rig.root().describe_layer_plane(rig.dc(), 1, 1, 32u, nullptr)); }

TEST(gl_icd_root, answers_gl_and_wgl_names_it_implements)
{ GlIcdRig rig;
  CHECK(rig.opengl32().get_proc_address("wglCreateContextAttribsARB") ==
        reinterpret_cast<void *>(&av::glicd::entry::wglCreateContextAttribsARB));
  CHECK(rig.opengl32().get_proc_address("wglGetExtensionsStringARB") ==
        reinterpret_cast<void *>(&av::glicd::entry::wglGetExtensionsStringARB));
  CHECK(rig.opengl32().get_proc_address("glDrawBuffers") != nullptr);
  CHECK(rig.opengl32().get_proc_address("glBindVertexArray") != nullptr);
  CHECK(rig.opengl32().get_proc_address("wglSwapIntervalEXT") == nullptr);
  CHECK(rig.opengl32().get_proc_address("glNotACommand") == nullptr);
  CHECK(rig.root().get_proc_address(nullptr) == nullptr);
  const auto extensions = reinterpret_cast<ExtensionsFn>(rig.opengl32().get_proc_address("wglGetExtensionsStringARB"));
  CHECK(std::strcmp(extensions(rig.dc()), "WGL_ARB_extensions_string WGL_ARB_pixel_format WGL_ARB_create_context "
                                          "WGL_ARB_create_context_profile WGL_ARB_create_context_robustness "
                                          "WGL_ARB_multisample WGL_ARB_framebuffer_sRGB") == 0);
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
  /* The renderer is the host's, answered without asking again. */
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
  CHECK_EQ(sent(rig, av::glproto::kCmd_glGetIntegerv), 4u); }

TEST(gl_icd_root, a_legacy_command_is_invalid_operation_without_a_host_call)
{ GlIcdRig rig;
  rig.load_with_format();
  CHECK(rig.current_context() != nullptr);
  const std::uint32_t commands = rig.kmt().commands();
  CHECK_EQ(rig.opengl32().gl<LegacyFn>(slot_new_list)(), 0u);
  CHECK_EQ(rig.opengl32().gl<GetErrorFn>(slot_get_error)(), gl_invalid_operation);
  CHECK_EQ(rig.kmt().commands(), commands);
  /* With nothing local, the host is asked. */
  rig.host().gl_varint(0x0505u);
  CHECK_EQ(rig.opengl32().gl<GetErrorFn>(slot_get_error)(), 0x0505u);
  CHECK_EQ(sent(rig, av::glproto::kCmd_glGetError), 1u); }

TEST(gl_icd_root, a_lost_context_answers_no_error_and_sends_nothing)
{ GlIcdRig rig;
  rig.load_with_format();
  CHECK(rig.current_context() != nullptr);
  rig.kmt().withhold_answer_at(1u);
  CHECK_EQ(rig.opengl32().gl<GetErrorFn>(slot_get_error)(), 0u);
  const std::uint32_t commands = rig.kmt().commands();
  CHECK_EQ(rig.opengl32().gl<GetErrorFn>(slot_get_error)(), 0u);
  rig.opengl32().gl<ClearFn>(slot_clear)(0x4000u);
  CHECK(rig.opengl32().swap_buffers(rig.dc()));
  CHECK_EQ(rig.kmt().commands(), commands); }

/* A swap puts the calling thread's context's frame on the DC's window: a DC of
 * a window with another format, a memory DC and a thread with nothing current
 * swap nothing, and only the main plane is a layer that swaps. */
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

/* A context the host lost swaps nothing and fails nothing. */
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
  const auto create = reinterpret_cast<AttribsFn>(rig.opengl32().get_proc_address("wglCreateContextAttribsARB"));
  const std::int32_t attributes[] = {0x2091, 4, 0x2092, 5, 0x9126, 1, 0};
  rig.host().gl_make("host gpu", "4.6");
  void *const context = create(rig.dc(), nullptr, attributes);
  CHECK(context != nullptr);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  av::RvkContextRecord record;
  CHECK_STATUS(record.decode(rig.kmt().last_private(), rig.kmt().last_private_size()), Status::ok);
  CHECK_EQ(record.options().major(), 4u);
  CHECK_EQ(record.options().minor(), 5u);
  CHECK(record.options().profile() == av::GlContextOptions::Profile::core);
  /* Already made: current once its window's framebuffer is made. */
  rig.host().gl_window(1u, 1u);
  CHECK(rig.opengl32().make_current(rig.dc(), context));
  CHECK(rig.root().current() != nullptr);
  CHECK(rig.root().invariants_hold()); }

TEST(gl_icd_root, a_create_with_attributes_shares_with_the_context_opengl32_names)
{ GlIcdRig rig;
  rig.load_with_format();
  const auto create = reinterpret_cast<AttribsFn>(rig.opengl32().get_proc_address("wglCreateContextAttribsARB"));
  void *const first = rig.opengl32().create_context(rig.dc());
  const std::int32_t attributes[] = {0};
  /* The share target is made first, then the context sharing it. */
  rig.host().gl_make("host gpu", "4.6");
  rig.host().gl_make("host gpu", "4.6");
  void *const second = create(rig.dc(), first, attributes);
  CHECK(second != nullptr);
  CHECK_EQ(rig.opengl32().dhglrc_lookups(), 1u);
  av::RvkContextRecord record;
  CHECK_STATUS(record.decode(rig.kmt().last_private(), rig.kmt().last_private_size()), Status::ok);
  CHECK(record.options().share_context() != 0u);
  CHECK_EQ(record.options().major(), 1u);
  /* A version asked for is kept beside the share. */
  const std::int32_t versioned[] = {0x2091, 3, 0x2092, 3, 0};
  rig.host().gl_make("host gpu", "4.6");
  CHECK(create(rig.dc(), first, versioned) != nullptr);
  CHECK_STATUS(record.decode(rig.kmt().last_private(), rig.kmt().last_private_size()), Status::ok);
  CHECK(record.options().share_context() != 0u);
  CHECK_EQ(record.options().major(), 3u);
  CHECK_EQ(record.options().minor(), 3u);
  /* A made share target is not made again; a lost or unknown one is refused. */
  rig.host().gl_make("host gpu", "4.6");
  CHECK(create(rig.dc(), first, attributes) != nullptr);
  CHECK(create(rig.dc(), reinterpret_cast<void *>(0x5u), attributes) == nullptr);
  check_error(rig, Root::error_invalid_operation);
  void *const unmade = rig.opengl32().create_context(rig.dc());
  rig.kmt().fail_at(MockKmt::Call::create_context_virtual, 1u, MockKmt::invalid_parameter);
  CHECK(create(rig.dc(), unmade, attributes) == nullptr);
  check_error(rig, Root::error_invalid_operation);
  rig.kmt().withhold_answer_at(1u);
  CHECK(rig.opengl32().make_current(rig.dc(), first));
  CHECK_EQ(rig.opengl32().gl<GetErrorFn>(slot_get_error)(), 0u);
  CHECK(create(rig.dc(), first, attributes) == nullptr);
  check_error(rig, Root::error_invalid_operation); }

TEST(gl_icd_root, a_create_with_attributes_answers_the_arb_errors)
{ GlIcdRig rig;
  rig.load_with_format();
  const auto create = reinterpret_cast<AttribsFn>(rig.opengl32().get_proc_address("wglCreateContextAttribsARB"));
  const std::int32_t bad_version[] = {0x2091, 4, 0x2092, 7, 0};
  CHECK(create(rig.dc(), nullptr, bad_version) == nullptr);
  check_error(rig, Root::error_invalid_version_arb);
  const std::int32_t core[] = {0x2091, 4, 0x2092, 6, 0x9126, 1, 0};
  CHECK(create(rig.other_dc(), nullptr, core) == nullptr);
  check_error(rig, Root::error_invalid_pixel_format);
  /* The host refuses what the ICD could not judge. */
  rig.host().blob_import(0u, 0x77u);
  rig.kmt().withhold_answer_at(2u);
  CHECK(create(rig.dc(), nullptr, core) == nullptr);
  check_error(rig, Root::error_invalid_version_arb);
  const std::int32_t compatibility[] = {0x2091, 4, 0x2092, 6, 0x9126, 2, 0};
  rig.host().blob_import(0u, 0x77u);
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
  rig.opengl32().refuse_next_create();
  CHECK(rig.root().create_context_attribs(rig.dc(), nullptr, none) == nullptr);
  check_error(rig, Root::error_invalid_operation);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  /* An HGLRC made without asking the driver names nothing of the ICD's. */
  rig.host().gl_make("host gpu", "4.6");
  rig.opengl32().skip_driver_next_create();
  CHECK(rig.root().create_context_attribs(rig.dc(), nullptr, none) == nullptr);
  check_error(rig, Root::error_invalid_operation);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK(rig.root().invariants_hold());
  av::glicd::GlIcdRootProbe::fill_pending(rig.root());
  rig.host().gl_make("host gpu", "4.6");
  CHECK(rig.root().create_context_attribs(rig.dc(), nullptr, none) == nullptr);
  check_error(rig, Root::error_invalid_operation);
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(gl_icd_root, a_create_with_attributes_and_no_room_is_out_of_memory)
{ GlIcdRig rig;
  rig.load_with_format();
  for (std::uint32_t index = 0u; index < 256u; index++) CHECK(rig.root().create_context(rig.dc()) != 0u);
  const std::int32_t none[] = {0};
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
  CHECK(std::strcmp(busy.dump(), "root/0 busy=true") == 0);
  rig.lock().held_elsewhere(false);
  av::StateWriter full(bytes, sizeof bytes);
  rig.root().report(full);
  CHECK(std::strncmp(full.dump(), "root/0 busy=false callbacks=9 current_anywhere=1\nconnection/0 users=1 ", 69u) == 0);
  CHECK(std::strstr(full.dump(), "\nwindows/0 held=1\n") != nullptr);
  CHECK(std::strstr(full.dump(), "\ncontexts/0 live=1\ncontexts/0/context/65537 state=made owner=1 ") != nullptr); }

TEST(gl_icd_root, a_dropped_command_is_logged_and_sync_tokens_count_up)
{ GlIcdRig rig;
  rig.root().note_dropped("gl", "glBufferData", 300000u);
  CHECK_EQ(rig.log().holding("rvkgl: gl glBufferData not sent: its encoding overflowed at byte 300000"), 1u);
  CHECK_EQ(rig.root().mint_sync_token(), 1u);
  CHECK_EQ(rig.root().mint_sync_token(), 2u); }

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

TEST(gl_icd_root, a_create_and_a_call_take_the_root_lock_then_the_send_lock)
{ GlIcdRig rig;
  rig.load_with_format();
  void *const context = rig.opengl32().create_context(rig.dc());
  rig.trace().clear();
  rig.host().gl_make("host gpu", "4.6");
  rig.host().gl_window(1u, 1u);
  CHECK(rig.opengl32().make_current(rig.dc(), context));
  const char *const made = rig.trace().text();
  CHECK_EQ(made[0], 'H');
  CHECK_EQ(made[1], 'S');
  CHECK_EQ(made[std::strlen(made) - 1u], 'h');
  rig.trace().clear();
  rig.opengl32().gl<ClearFn>(slot_clear)(0x4000u);
  CHECK(std::strcmp(rig.trace().text(), "Ss") == 0); }

TEST(gl_icd_root_invariants, fail_for_state_no_entry_makes)
{ GlIcdRig pending;
  av::glicd::GlIcdRootProbe::unpair_pending(pending.root());
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
  av::glicd::GlExtensionPolicyProbe::overcount(GlIcdRootProbe::extensions(extensions.root()));
  CHECK(!extensions.root().invariants_hold());
  GlIcdRig formats;
  av::glicd::PixelFormatTableProbe::repeat_row(GlIcdRootProbe::formats(formats.root()));
  CHECK(!formats.root().invariants_hold());
  GlIcdRig windows;
  av::glicd::WindowTableProbe::overfill(GlIcdRootProbe::windows(windows.root()));
  CHECK(!windows.root().invariants_hold());
  GlIcdRig connection;
  av::glicd::GlConnectionProbe::set_users(GlIcdRootProbe::connection(connection.root()), 1u);
  CHECK(!connection.root().invariants_hold());
  av::glicd::GlConnectionProbe::set_users(GlIcdRootProbe::connection(connection.root()), 0u);
  GlIcdRig contexts;
  av::glicd::GlContextTableProbe::zero_generation(GlIcdRootProbe::contexts(contexts.root()));
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

/* WGL_ARB_pixel_format through DrvGetProcAddress: the answers of
 * PixelFormatQuery, with its refusals reported as the ARB's errors, and a DC
 * with no window - a memory DC - offered no format. */
TEST(gl_icd_root, answers_pixel_format_attributes_and_choices_through_wgl)
{ GlIcdRig rig;
  rig.load_with_format();
  const auto attribiv = reinterpret_cast<AttribivFn>(rig.opengl32().get_proc_address("wglGetPixelFormatAttribivARB"));
  const auto attribfv = reinterpret_cast<AttribfvFn>(rig.opengl32().get_proc_address("wglGetPixelFormatAttribfvARB"));
  const auto choose = reinterpret_cast<ChooseFn>(rig.opengl32().get_proc_address("wglChoosePixelFormatARB"));
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
