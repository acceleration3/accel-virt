#include "core/trace/state_writer.hpp"
#include "device/abi/av_executor.h"
#include "guest/probe/gl_probe.hpp"
#include "guest/suite/guest_report.hpp"
#include "harness/guest/directory_image_sink.hpp"
#include "harness/executor/executor_library_file.hpp"
#include "harness/executor/widest_host_gpu.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/fullstack/full_stack_rig.hpp"
#include "core/log/keyed_log.hpp"
#include "harness/icd/counted_wake_signal.hpp"
#include "harness/icd/fake_opengl32.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/opengl32_gl_entry.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/icd/scripted_thread.hpp"
#include "harness/icd/scripted_thread_slot.hpp"
#include "harness/icd/scripted_window_chains.hpp"
#include "harness/icd/scripted_window_surface.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opengl/gl_icd_binding.hpp"
#include "icd/opengl/gl_icd_root.hpp"
#include "icd/common/window_present_queue.hpp"
#include "icd/opengl/back_buffer_presenter.hpp"
#include "icd/opengl/batch_ready.hpp"

#include "gl_protocol_ids.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" const char *__lsan_default_suppressions(void);
extern "C" const char *__lsan_default_suppressions(void)
{
    return "leak:libOpenCL\n"
           "leak:intelocl\n"
           "leak:libigdrcl\n"
           "leak:libEGL\n"
           "leak:libGLX\n"
           "leak:libglapi\n"
           "leak:libvulkan\n"
           "leak:libnvidia\n"
           "leak:nvidia_icd\n"
           "leak:libvulkan_intel\n"
           "leak:iris_dri\n"
           "leak:libdbus-1\n"
           "leak:libz\n"
           "leak:libLLVM\n"
           "leak:libgallium\n"
           "leak:libav_executor\n";
}

using av::DdiKmt;
using av::ExecutorLibraryFile;
using av::FakeOpengl32;
using av::FullStackRig;
using av::LockTrace;
using av::MockPlatform;
using av::RecordingLog;
using av::RecordingThreadLock;
using av::ScriptedThreadSlot;
using av::ScriptedWindowSurface;
using av::WidestHostGpu;
using av::glicd::GlIcdRoot;

namespace {

constexpr GLenum gl_renderer = 0x1F01u, gl_version = 0x1F02u;
constexpr GLenum gl_texture_2d = 0x0DE1u, gl_rgba = 0x1908u, gl_unsigned_byte = 0x1401u;
constexpr GLenum gl_framebuffer = 0x8D40u, gl_color_attachment0 = 0x8CE0u, gl_framebuffer_complete = 0x8CD5u;
constexpr GLbitfield gl_color_buffer_bit = 0x4000u;
constexpr std::uint32_t side = 64u;
constexpr std::uint32_t window_side = 32u;
constexpr std::size_t   renderer_bytes = 256u;
constexpr std::size_t   pixel_blob_floor = 4096u;

constexpr std::uint32_t slot_new_list = 0u, slot_end_list = 1u, slot_call_list = 2u, slot_gen_lists = 5u,
                        slot_begin = 7u, slot_color_3f = 13u, slot_end = 43u, slot_vertex_2f = 128u,
                        slot_load_identity = 290u, slot_matrix_mode = 293u, slot_get_integerv = 263u;
constexpr std::uint32_t slot_tex_image_2d = 183u, slot_clear = 203u, slot_clear_color = 206u,
                        slot_finish = 216u, slot_read_pixels = 256u, slot_get_error = 261u,
                        slot_get_string = 275u, slot_bind_texture = 307u, slot_gen_textures = 328u;

class GlProcess {
public:
    explicit GlProcess(DdiKmt &kmt)
        : lock_(trace_, 'H'), send_lock_(trace_, 'S'), present_lock_(trace_, 'P'), lines_(memory_, log_, "avgl: "),
          slice_lock_(trace_, 'L'), opengl32_(threads_),
          ready_(kmt),
          presents_(memory_, present_thread_, handed_, shown_, present_lock_, ready_, surface_, chains_, lines_),
          root_(memory_, kmt, lock_, send_lock_, slice_lock_, lines_, surface_, threads_, opengl32_, presents_,
                av::glicd::gl_icd_binding)
    {
        presents_.refuse();
        surface_.add_dc(&dc_, &window_, window_side, window_side);
    }

    GlIcdRoot    &root() { return root_; }
    RecordingLog &log() { return log_; }
    FakeOpengl32 &opengl32() { return opengl32_; }
    ScriptedThreadSlot &threads() { return threads_; }
    void         *dc() { return &dc_; }

private:
    MockPlatform          memory_;
    LockTrace             trace_;
    RecordingThreadLock   lock_;
    RecordingThreadLock   send_lock_;
    RecordingThreadLock   present_lock_;
    RecordingLog          log_;
    av::KeyedLog          lines_;
    RecordingThreadLock   slice_lock_;
    ScriptedWindowSurface surface_;
    av::ScriptedWindowChains chains_;
    ScriptedThreadSlot    threads_;
    FakeOpengl32          opengl32_;
    av::ScriptedThread       present_thread_;
    av::CountedWakeSignal    handed_;
    av::CountedWakeSignal    shown_;
    av::glicd::BatchReady    ready_;
    av::glicd::GlPresentQueue presents_;
    GlIcdRoot             root_;
    int                   dc_ = 0;
    int                   window_ = 0;
};

bool host_renderer(const AvExecutorOps &ops, const std::uint8_t *uuid, char *out)
{
    const std::uint32_t    domain = ops.open_domain(ops.opaque, uuid, 0u, 0u, 0u);
    const AvContextOptions options = {0u, 0u, 0u, 0u, domain};
    AvExecutor *const      executor = ops.create(ops.opaque, AV_EXECUTOR_SUB_API_OPENGL, &options, nullptr);
    if (executor == nullptr) {
        if (domain != 0u) ops.close_domain(ops.opaque, domain);
        return false;
    }
    const std::uint8_t name[] = {0x81u, 0x3Eu};
    std::uint8_t       reply[renderer_bytes] = {};
    std::size_t        length = 0u;
    const std::uint32_t ran = ops.execute(ops.opaque, executor, av::glproto::kCmd_glGetString, name, sizeof name,
                                          reply, sizeof reply, &length);
    ops.destroy(ops.opaque, executor);
    ops.close_domain(ops.opaque, domain);
    if (ran != AV_EXECUTED || length < 2u || reply[0] != 1u || reply[1] >= 0x80u) return false;
    const std::size_t text = reply[1];
    if (2u + text > length) return false;
    std::memcpy(out, reply + 2u, text);
    out[text] = '\0';
    return true;
}

const WidestHostGpu &widest_gpu()
{
    static const WidestHostGpu gpu;
    return gpu;
}

const ExecutorLibraryFile &shared_library()
{
    static_cast<void>(widest_gpu());
    static const ExecutorLibraryFile library(AV_EXECUTOR_LIBRARY);
    return library;
}

bool version_of(const char *version, unsigned &major, unsigned &minor)
{
    return std::sscanf(version, "%u.%u", &major, &minor) == 2;
}

}

TEST(fullstack_opengl_icd, a_context_on_the_host_gpu_clears_and_reads_back_through_the_arena)
{ const WidestHostGpu &gpu = widest_gpu();
  if ((gpu.sub_apis() & AV_EXECUTOR_SUB_API_OPENGL) == 0u) return;
  const ExecutorLibraryFile &library = shared_library();
  CHECK(library.ops() != nullptr);
  if (library.ops() == nullptr) return;
  char expected_renderer[renderer_bytes] = {};
  CHECK(host_renderer(*library.ops(), gpu.uuid(), expected_renderer));
  FullStackRig rig(*library.ops(), gpu.uuid());
  rig.kmt().let_process_watch();
  CHECK_EQ(rig.started(), 0);
  { GlProcess process(rig.kmt());
    FakeOpengl32 &gl = process.opengl32();
    CHECK(gl.load(GlIcdRoot::icd_version, 1u) == FakeOpengl32::Load::loaded);
    CHECK(gl.set_pixel_format(process.dc(), 1));
    void *const context = gl.create_context(process.dc());
    CHECK(context != nullptr);
    CHECK(gl.make_current(process.dc(), context));
    CHECK_EQ(gl.table_entries(), FakeOpengl32::full_table);

    using GetString = const GLubyte *(APIENTRY *)(GLenum);
    const auto *const renderer = reinterpret_cast<const char *>(gl.gl<GetString>(slot_get_string)(gl_renderer));
    const auto *const version = reinterpret_cast<const char *>(gl.gl<GetString>(slot_get_string)(gl_version));
    CHECK(renderer != nullptr && version != nullptr);
    if (renderer == nullptr || version == nullptr) return;
    std::fprintf(stderr, "host GL: %s / %s\n", renderer, version);
    CHECK(std::strcmp(renderer, expected_renderer) == 0);
    unsigned major = 0u, minor = 0u;
    CHECK(version_of(version, major, minor));
    CHECK(major >= 3u && (major < 4u || (major == 4u && minor <= 6u)));

    GLuint texture = 0u, framebuffer = 0u;
    gl.gl<void (APIENTRY *)(GLsizei, GLuint *)>(slot_gen_textures)(1, &texture);
    gl.gl<void (APIENTRY *)(GLenum, GLuint)>(slot_bind_texture)(gl_texture_2d, texture);
    gl.gl<void (APIENTRY *)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *)>(
        slot_tex_image_2d)(gl_texture_2d, 0, static_cast<GLint>(gl_rgba), side, side, 0, gl_rgba,
                           gl_unsigned_byte, nullptr);
    using GenFramebuffers = void (APIENTRY *)(GLsizei, GLuint *);
    using BindFramebuffer = void (APIENTRY *)(GLenum, GLuint);
    using FramebufferTexture2D = void (APIENTRY *)(GLenum, GLenum, GLenum, GLuint, GLint);
    using CheckFramebufferStatus = GLenum (APIENTRY *)(GLenum);
    reinterpret_cast<GenFramebuffers>(gl.get_proc_address("glGenFramebuffers"))(1, &framebuffer);
    reinterpret_cast<BindFramebuffer>(gl.get_proc_address("glBindFramebuffer"))(gl_framebuffer, framebuffer);
    reinterpret_cast<FramebufferTexture2D>(gl.get_proc_address("glFramebufferTexture2D"))(
        gl_framebuffer, gl_color_attachment0, gl_texture_2d, texture, 0);
    CHECK(texture != 0u && framebuffer != 0u);
    CHECK_EQ(reinterpret_cast<CheckFramebufferStatus>(gl.get_proc_address("glCheckFramebufferStatus"))(
                 gl_framebuffer), gl_framebuffer_complete);
    gl.gl<void (APIENTRY *)(GLfloat, GLfloat, GLfloat, GLfloat)>(slot_clear_color)(0.2f, 0.4f, 0.6f, 1.0f);
    gl.gl<void (APIENTRY *)(GLbitfield)>(slot_clear)(gl_color_buffer_bit);
    gl.gl<void (APIENTRY *)()>(slot_finish)();
    static std::uint8_t pixels[side * side * 4u];
    std::memset(pixels, 0xEE, sizeof pixels);
    CHECK(sizeof pixels > pixel_blob_floor);
    gl.gl<void (APIENTRY *)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *)>(slot_read_pixels)(
        0, 0, side, side, gl_rgba, gl_unsigned_byte, pixels);
    CHECK_EQ(gl.gl<GLenum (APIENTRY *)()>(slot_get_error)(), 0u);
    std::uint32_t wrong = 0u;
    for (std::uint32_t pixel = 0u; pixel < side * side; pixel++) {
        const std::uint8_t *const at = pixels + pixel * 4u;
        if (at[0] != 51u || at[1] != 102u || at[2] != 153u || at[3] != 255u) wrong++;
    }
    CHECK_EQ(wrong, 0u);

    char dump[4096] = {};
    av::StateWriter writer(dump, sizeof dump);
    process.root().report(writer);
    CHECK(std::strstr(writer.dump(), " arena=true ") != nullptr);
    CHECK_EQ(process.log().holding("no blob arena"), 0u);
    CHECK(process.root().invariants_hold());

    CHECK(gl.make_current(nullptr, nullptr));
    CHECK(gl.delete_context(context));
    CHECK(process.root().invariants_hold());
    for (std::uint32_t index = 0u; index < process.log().lines(); index++)
        std::fprintf(stderr, "%s\n", process.log().line(index)); }
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.device().device().contexts(), 0u); }

TEST(fullstack_opengl_icd, without_an_arena_a_texture_larger_than_every_room_goes_up_and_comes_back)
{ const WidestHostGpu &gpu = widest_gpu();
  if ((gpu.sub_apis() & AV_EXECUTOR_SUB_API_OPENGL) == 0u) return;
  const ExecutorLibraryFile &library = shared_library();
  CHECK(library.ops() != nullptr);
  if (library.ops() == nullptr) return;
  FullStackRig rig(*library.ops(), gpu.uuid());
  rig.kmt().let_process_watch();
  rig.kmt().refuse_host_windows();
  CHECK_EQ(rig.started(), 0);
  { GlProcess process(rig.kmt());
    FakeOpengl32 &gl = process.opengl32();
    CHECK(gl.load(GlIcdRoot::icd_version, 1u) == FakeOpengl32::Load::loaded);
    CHECK(gl.set_pixel_format(process.dc(), 1));
    void *const context = gl.create_context(process.dc());
    CHECK(context != nullptr);
    CHECK(gl.make_current(process.dc(), context));
    constexpr GLsizei   wide = 1024, high = 512;
    constexpr std::size_t bytes = std::size_t{wide} * high * 4u;
    static std::uint8_t sent[bytes];
    static std::uint8_t back[bytes];
    for (std::size_t at = 0u; at < bytes; at++) sent[at] = static_cast<std::uint8_t>(at * 31u + at / 4093u);
    std::memset(back, 0xEE, sizeof back);
    GLuint texture = 0u, framebuffer = 0u;
    gl.gl<void (APIENTRY *)(GLsizei, GLuint *)>(slot_gen_textures)(1, &texture);
    gl.gl<void (APIENTRY *)(GLenum, GLuint)>(slot_bind_texture)(gl_texture_2d, texture);
    gl.gl<void (APIENTRY *)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *)>(
        slot_tex_image_2d)(gl_texture_2d, 0, static_cast<GLint>(gl_rgba), wide, high, 0, gl_rgba, gl_unsigned_byte,
                           sent);
    using GenFramebuffers = void (APIENTRY *)(GLsizei, GLuint *);
    using BindFramebuffer = void (APIENTRY *)(GLenum, GLuint);
    using FramebufferTexture2D = void (APIENTRY *)(GLenum, GLenum, GLenum, GLuint, GLint);
    reinterpret_cast<GenFramebuffers>(gl.get_proc_address("glGenFramebuffers"))(1, &framebuffer);
    reinterpret_cast<BindFramebuffer>(gl.get_proc_address("glBindFramebuffer"))(gl_framebuffer, framebuffer);
    reinterpret_cast<FramebufferTexture2D>(gl.get_proc_address("glFramebufferTexture2D"))(
        gl_framebuffer, gl_color_attachment0, gl_texture_2d, texture, 0);
    gl.gl<void (APIENTRY *)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *)>(slot_read_pixels)(
        0, 0, wide, high, gl_rgba, gl_unsigned_byte, back);
    CHECK_EQ(gl.gl<GLenum (APIENTRY *)()>(slot_get_error)(), 0u);
    std::size_t wrong = 0u;
    for (std::size_t at = 0u; at < bytes; at++) wrong += sent[at] != back[at] ? 1u : 0u;
    CHECK_EQ(wrong, 0u);

    char dump[4096] = {};
    av::StateWriter writer(dump, sizeof dump);
    process.root().report(writer);
    CHECK(std::strstr(writer.dump(), " arena=false ") != nullptr);
    CHECK(process.root().invariants_hold());
    CHECK(gl.make_current(nullptr, nullptr));
    CHECK(gl.delete_context(context));
    for (std::uint32_t index = 0u; index < process.log().lines(); index++)
        std::fprintf(stderr, "%s\n", process.log().line(index)); }
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.device().device().contexts(), 0u); }

TEST(fullstack_opengl_icd, a_legacy_context_draws_the_fixed_function_pipeline_through_a_display_list)
{ const WidestHostGpu &gpu = widest_gpu();
  if ((gpu.sub_apis() & AV_EXECUTOR_SUB_API_OPENGL) == 0u) return;
  const ExecutorLibraryFile &library = shared_library();
  CHECK(library.ops() != nullptr);
  if (library.ops() == nullptr) return;
  FullStackRig rig(*library.ops(), gpu.uuid());
  rig.kmt().let_process_watch();
  CHECK_EQ(rig.started(), 0);
  { GlProcess process(rig.kmt());
    FakeOpengl32 &gl = process.opengl32();
    CHECK(gl.load(GlIcdRoot::icd_version, 1u) == FakeOpengl32::Load::loaded);
    CHECK(gl.set_pixel_format(process.dc(), 1));
    void *const context = gl.create_context(process.dc());
    CHECK(context != nullptr);
    CHECK(gl.make_current(process.dc(), context));

    GLint mask = 0;
    gl.gl<void (APIENTRY *)(GLenum, GLint *)>(slot_get_integerv)(0x9126u , &mask);
    using GetString = const GLubyte *(APIENTRY *)(GLenum);
    const auto *const version = reinterpret_cast<const char *>(gl.gl<GetString>(slot_get_string)(gl_version));
    std::fprintf(stderr, "legacy context: mask 0x%x, %s\n", static_cast<unsigned>(mask),
                 version != nullptr ? version : "(none)");
    if (mask != 2) {
        std::fprintf(stderr, "host grants no compatibility profile; nothing to draw\n");
        CHECK(gl.make_current(nullptr, nullptr));
        CHECK(gl.delete_context(context));
        return;
    }

    GLuint texture = 0u, framebuffer = 0u;
    gl.gl<void (APIENTRY *)(GLsizei, GLuint *)>(slot_gen_textures)(1, &texture);
    gl.gl<void (APIENTRY *)(GLenum, GLuint)>(slot_bind_texture)(gl_texture_2d, texture);
    gl.gl<void (APIENTRY *)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *)>(
        slot_tex_image_2d)(gl_texture_2d, 0, static_cast<GLint>(gl_rgba), side, side, 0, gl_rgba,
                           gl_unsigned_byte, nullptr);
    using GenFramebuffers = void (APIENTRY *)(GLsizei, GLuint *);
    using BindFramebuffer = void (APIENTRY *)(GLenum, GLuint);
    using FramebufferTexture2D = void (APIENTRY *)(GLenum, GLenum, GLenum, GLuint, GLint);
    reinterpret_cast<GenFramebuffers>(gl.get_proc_address("glGenFramebuffers"))(1, &framebuffer);
    reinterpret_cast<BindFramebuffer>(gl.get_proc_address("glBindFramebuffer"))(gl_framebuffer, framebuffer);
    reinterpret_cast<FramebufferTexture2D>(gl.get_proc_address("glFramebufferTexture2D"))(
        gl_framebuffer, gl_color_attachment0, gl_texture_2d, texture, 0);
    gl.gl<void (APIENTRY *)(GLint, GLint, GLsizei, GLsizei)>(305u )(0, 0, side, side);

    gl.gl<void (APIENTRY *)(GLenum)>(slot_matrix_mode)(0x1701u );
    gl.gl<void (APIENTRY *)()>(slot_load_identity)();
    gl.gl<void (APIENTRY *)(GLenum)>(slot_matrix_mode)(0x1700u );
    gl.gl<void (APIENTRY *)()>(slot_load_identity)();
    gl.gl<void (APIENTRY *)(GLfloat, GLfloat, GLfloat, GLfloat)>(slot_clear_color)(0.0f, 0.0f, 0.0f, 1.0f);
    gl.gl<void (APIENTRY *)(GLbitfield)>(slot_clear)(gl_color_buffer_bit);

    const GLuint list = gl.gl<GLuint (APIENTRY *)(GLsizei)>(slot_gen_lists)(1);
    CHECK(list != 0u);
    gl.gl<void (APIENTRY *)(GLuint, GLenum)>(slot_new_list)(list, 0x1300u );
    gl.gl<void (APIENTRY *)(GLenum)>(slot_begin)(0x0004u );
    gl.gl<void (APIENTRY *)(GLfloat, GLfloat, GLfloat)>(slot_color_3f)(1.0f, 0.0f, 0.0f);
    gl.gl<void (APIENTRY *)(GLfloat, GLfloat)>(slot_vertex_2f)(-1.0f, -1.0f);
    gl.gl<void (APIENTRY *)(GLfloat, GLfloat)>(slot_vertex_2f)(3.0f, -1.0f);
    gl.gl<void (APIENTRY *)(GLfloat, GLfloat)>(slot_vertex_2f)(-1.0f, 3.0f);
    gl.gl<void (APIENTRY *)()>(slot_end)();
    gl.gl<void (APIENTRY *)()>(slot_end_list)();
    gl.gl<void (APIENTRY *)(GLuint)>(slot_call_list)(list);
    gl.gl<void (APIENTRY *)()>(slot_finish)();
    CHECK_EQ(gl.gl<GLenum (APIENTRY *)()>(slot_get_error)(), 0u);

    static std::uint8_t pixels[side * side * 4u];
    std::memset(pixels, 0xEE, sizeof pixels);
    gl.gl<void (APIENTRY *)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *)>(slot_read_pixels)(
        0, 0, side, side, gl_rgba, gl_unsigned_byte, pixels);
    CHECK_EQ(gl.gl<GLenum (APIENTRY *)()>(slot_get_error)(), 0u);
    std::uint32_t wrong = 0u;
    for (std::uint32_t pixel = 0u; pixel < side * side; pixel++) {
        const std::uint8_t *const at = pixels + pixel * 4u;
        if (at[0] != 255u || at[1] != 0u || at[2] != 0u || at[3] != 255u) wrong++;
    }
    CHECK_EQ(wrong, 0u);
    CHECK(process.root().invariants_hold());
    CHECK(gl.make_current(nullptr, nullptr));
    CHECK(gl.delete_context(context)); }
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.device().device().contexts(), 0u); }

TEST(fullstack_opengl_icd, the_gl_probe_passes_every_step_on_the_host_gpu)
{ const WidestHostGpu &gpu = widest_gpu();
  if ((gpu.sub_apis() & AV_EXECUTOR_SUB_API_OPENGL) == 0u) return;
  const ExecutorLibraryFile &library = shared_library();
  CHECK(library.ops() != nullptr);
  if (library.ops() == nullptr) return;
  FullStackRig rig(*library.ops(), gpu.uuid());
  rig.kmt().let_process_watch();
  CHECK_EQ(rig.started(), 0);
  { GlProcess process(rig.kmt());
    CHECK(process.opengl32().load(GlIcdRoot::icd_version, 1u) == FakeOpengl32::Load::loaded);
    av::Opengl32GlEntry gl(process.opengl32(), process.threads(), process.dc(), window_side, window_side);
    av::MockPlatform report_memory;
    av::GuestReport     report(report_memory, "avglprobe");
    av::DirectoryImageSink images(AV_IMAGE_DIR);
    av::GlProbe         probe(rig.kmt(), report, gl, images);
    probe.run();
    report.finish();
    std::fprintf(stderr, "%s", report.text());
    CHECK_EQ(report.failed(), 0u);
    CHECK_EQ(report.passed(), 12u);
    CHECK(std::strstr(report.text(), "ok opengl.draw 1024\n") != nullptr);
    CHECK(std::strstr(report.text(), "ok opengl.share 4\n") != nullptr);
    CHECK(std::strstr(report.text(), "ok opengl.busy 170\n") != nullptr);
    CHECK(std::strstr(report.text(), "ok opengl.lost 33363\n") != nullptr);
    CHECK(std::strstr(report.text(), "ok opengl.recreated 1024\n") != nullptr);
    CHECK(process.root().invariants_hold());
    for (std::uint32_t index = 0u; index < process.log().lines(); index++)
        std::fprintf(stderr, "%s\n", process.log().line(index)); }
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.device().device().contexts(), 0u); }
