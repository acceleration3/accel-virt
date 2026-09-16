/* One guest context's OpenGL made the way the application asked, on every GPU
 * this host serves OpenGL on (feature 050): the version and profile it reports,
 * its debug and robustness state, a context sharing another's objects while
 * that one is current on another thread, and the imported region a pixel read
 * lands in. A host with no OpenGL returns without judging. */
#include "core/support/status.hpp"
#include "device/abi/rvk_executor.h"
#include "device/executor/imported_blob.hpp"
#include "device/executor/opengl_backend.hpp"
#include "device/shim/gl_display.hpp"
#include "device/shim/loader_host_apis.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/opengl/gl_protocol_ids.h"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>
#include <EGL/egl.h>
#include <pthread.h>

extern "C" const char *__lsan_default_suppressions(void);
extern "C" const char *__lsan_default_suppressions(void)
{
    return "leak:libOpenCL\n"
           "leak:intelocl\n"
           "leak:libigdrcl\n"
           "leak:libvulkan\n"
           "leak:libEGL\n"
           "leak:libGLX\n"
           "leak:libnvidia\n"
           "leak:nvidia_icd\n"
           "leak:iris_dri\n"
           "leak:libdbus-1\n"
           "leak:libglapi\n"
           "leak:libz\n";
}

namespace av {

/* The host's own GL of a backend, called directly with its context current. */
struct OpenGlBackendProbe {
    static const glproto::RealGl &gl(OpenGlBackend &backend)
    {
        CHECK(backend.context_.make_current());
        return backend.gl_;
    }
};

}  // namespace av

using av::OpenGlBackend;
using av::OpenGlBackendProbe;

namespace {

constexpr unsigned gl_major_version = 0x821B;
constexpr unsigned gl_minor_version = 0x821C;
constexpr unsigned gl_context_flags = 0x821E;
constexpr unsigned gl_context_profile_mask = 0x9126;
constexpr unsigned gl_reset_notification_strategy = 0x8256;
constexpr unsigned gl_lose_context_on_reset = 0x8252;
constexpr int      gl_context_flag_debug = 0x2;
constexpr int      gl_core_profile_bit = 0x1;
constexpr unsigned gl_texture_2d = 0x0DE1;
constexpr unsigned gl_rgba8 = 0x8058;
constexpr unsigned gl_rgba = 0x1908;
constexpr unsigned gl_unsigned_byte = 0x1401;
constexpr unsigned gl_framebuffer = 0x8D40;
constexpr unsigned gl_color_attachment0 = 0x8CE0;
constexpr unsigned gl_framebuffer_complete = 0x8CD5;
constexpr unsigned gl_color_buffer_bit = 0x4000;
constexpr unsigned gl_texture_min_filter = 0x2801;
constexpr unsigned gl_nearest = 0x2600;

constexpr std::uint8_t blob_mode = 3u;
constexpr std::uint64_t page = 4096u;

alignas(4096) std::uint8_t region[2u * 4096u];

/* Every display this host keeps, in order; the count. */
class Displays {
public:
    Displays() : count_(0u)
    {
        host_.open();
        for (std::uint32_t i = 0u; i < host_.gpu_count(); i++) {
            const av::GlDisplay *const display = host_.gl_display(i);
            /* Where OpenGL runs its context imports a host pointer as its region. */
            CHECK_EQ(host_.import_kinds(i, RVK_EXECUTOR_SUB_API_OPENGL),
                     display != nullptr ? std::uint32_t{RVK_BLOB_HOST_POINTER} : 0u);
            if (display != nullptr) displays_[count_++] = display;
        }
    }
    ~Displays() { host_.close(); }
    std::uint32_t        count() const { return count_; }
    const av::GlDisplay *at(std::uint32_t index) const { return displays_[index]; }

private:
    av::LoaderHostApis   host_;
    const av::GlDisplay *displays_[16] = {};
    std::uint32_t        count_;
};

int integer(OpenGlBackend &backend, unsigned name)
{
    const av::glproto::RealGl &gl = OpenGlBackendProbe::gl(backend);
    int value = -1;
    gl.glGetIntegerv(name, &value);
    return value;
}

}  // namespace

TEST(opengl_backend_device, a_context_reports_the_version_and_profile_it_was_made_with)
{ Displays displays;
  for (std::uint32_t index = 0u; index < displays.count(); index++) {
      const RvkContextOptions core_46 = {4u, 6u, RVK_CONTEXT_PROFILE_CORE, 0u, 0u};
      OpenGlBackend newest;
      CHECK(newest.initialize(displays.at(index), &core_46, nullptr));
      CHECK_EQ(integer(newest, gl_major_version), 4);
      CHECK_EQ(integer(newest, gl_minor_version), 6);
      CHECK_EQ(integer(newest, gl_context_profile_mask) & gl_core_profile_bit, gl_core_profile_bit);
      newest.shut_down();
      const RvkContextOptions core_33 = {3u, 3u, RVK_CONTEXT_PROFILE_CORE, 0u, 0u};
      OpenGlBackend older;
      CHECK(older.initialize(displays.at(index), &core_33, nullptr));
      const int version = integer(older, gl_major_version) * 10 + integer(older, gl_minor_version);
      CHECK(version >= 33);
      CHECK_EQ(integer(older, gl_context_profile_mask) & gl_core_profile_bit, gl_core_profile_bit);
      older.shut_down();
      /* No OpenGL has these, and EGL is not asked for them. */
      const RvkContextOptions refused[] = {{4u, 7u, RVK_CONTEXT_PROFILE_CORE, 0u, 0u},
                                           {5u, 0u, RVK_CONTEXT_PROFILE_CORE, 0u, 0u}};
      for (const RvkContextOptions &options : refused) {
          OpenGlBackend none;
          CHECK(!none.initialize(displays.at(index), &options, nullptr));
          CHECK_EQ(none.resolved(), 0u);
      }
  } }

TEST(opengl_backend_device, debug_and_robust_contexts_report_their_flags)
{ Displays displays;
  for (std::uint32_t index = 0u; index < displays.count(); index++) {
      const RvkContextOptions debug = {4u, 3u, RVK_CONTEXT_PROFILE_CORE, RVK_CONTEXT_FLAG_DEBUG, 0u};
      OpenGlBackend debugging;
      CHECK(debugging.initialize(displays.at(index), &debug, nullptr));
      CHECK_EQ(integer(debugging, gl_context_flags) & gl_context_flag_debug, gl_context_flag_debug);
      debugging.shut_down();
      const RvkContextOptions robust = {4u, 3u, RVK_CONTEXT_PROFILE_CORE,
                                        RVK_CONTEXT_FLAG_ROBUST_ACCESS | RVK_CONTEXT_FLAG_LOSE_CONTEXT_ON_RESET, 0u};
      OpenGlBackend robustly;
      CHECK(robustly.initialize(displays.at(index), &robust, nullptr));
      CHECK_EQ(integer(robustly, gl_reset_notification_strategy), static_cast<int>(gl_lose_context_on_reset));
      robustly.shut_down();
      /* A plain one asked for neither. */
      OpenGlBackend plain;
      CHECK(plain.initialize(displays.at(index), nullptr, nullptr));
      CHECK_EQ(integer(plain, gl_context_flags) & gl_context_flag_debug, 0);
      plain.shut_down();
  } }

namespace {

/* The context that made the texture, held current on a thread of its own
 * while the sharing one is made and asks about it. */
struct Holder {
    OpenGlBackend  *backend;
    unsigned        texture;
    pthread_mutex_t lock;
    pthread_cond_t  changed;
    int             stage;
};

void advance(Holder &holder, int stage)
{
    pthread_mutex_lock(&holder.lock);
    holder.stage = stage;
    pthread_cond_broadcast(&holder.changed);
    pthread_mutex_unlock(&holder.lock);
}

void await(Holder &holder, int stage)
{
    pthread_mutex_lock(&holder.lock);
    while (holder.stage < stage) pthread_cond_wait(&holder.changed, &holder.lock);
    pthread_mutex_unlock(&holder.lock);
}

void *hold_current(void *context)
{
    Holder                    &holder = *static_cast<Holder *>(context);
    const av::glproto::RealGl &gl = OpenGlBackendProbe::gl(*holder.backend);
    gl.glGenTextures(1, &holder.texture);
    gl.glBindTexture(gl_texture_2d, holder.texture);
    advance(holder, 1);
    await(holder, 2);
    CHECK(eglMakeCurrent(eglGetCurrentDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_TRUE);
    return nullptr;
}

}  // namespace

TEST(opengl_backend_device, a_sharing_context_sees_a_texture_made_on_another_thread)
{ Displays displays;
  for (std::uint32_t index = 0u; index < displays.count(); index++) {
      OpenGlBackend first;
      CHECK(first.initialize(displays.at(index), nullptr, nullptr));
      /* Opening made it current here; a context is current on one thread. */
      CHECK(eglMakeCurrent(eglGetCurrentDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_TRUE);
      Holder holder = {&first, 0u, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, 0};
      pthread_t thread;
      CHECK_EQ(pthread_create(&thread, nullptr, hold_current, &holder), 0);
      await(holder, 1);
      const RvkContextOptions core = {4u, 3u, RVK_CONTEXT_PROFILE_CORE, 0u, 0u};
      OpenGlBackend second;
      CHECK(second.initialize(displays.at(index), &core, &first));
      const av::glproto::RealGl &gl = OpenGlBackendProbe::gl(second);
      CHECK(holder.texture != 0u);
      CHECK_EQ(gl.glIsTexture(holder.texture), 1u);
      /* One that shares nothing does not know it. */
      OpenGlBackend alone;
      CHECK(alone.initialize(displays.at(index), &core, nullptr));
      CHECK_EQ(OpenGlBackendProbe::gl(alone).glIsTexture(holder.texture), 0u);
      advance(holder, 2);
      CHECK_EQ(pthread_join(thread, nullptr), 0);
      alone.shut_down();
      second.shut_down();
      /* A share with no context is refused. */
      OpenGlBackend orphan;
      CHECK(!orphan.initialize(displays.at(index), &core, &second));
      first.shut_down();
  }
  /* A share on another display is refused, where the host has two. */
  if (displays.count() < 2u) return;
  OpenGlBackend here;
  CHECK(here.initialize(displays.at(0u), nullptr, nullptr));
  OpenGlBackend there;
  CHECK(!there.initialize(displays.at(1u), nullptr, &here));
  here.shut_down(); }

TEST(opengl_backend_device, a_pixel_read_lands_in_the_adopted_region)
{ Displays displays;
  for (std::uint32_t index = 0u; index < displays.count(); index++) {
      OpenGlBackend backend;
      CHECK(backend.initialize(displays.at(index), nullptr, nullptr));
      RvkBlobHandle handle = {};
      handle.kind = RVK_BLOB_HOST_POINTER;
      handle.pointer = region;
      handle.fd = -1;
      handle.size = sizeof region;
      av::ImportedBlob blob;
      CHECK_STATUS(blob.take(1u, handle), av::Status::ok);
      CHECK(backend.adopt(blob));
      av::ImportedBlob second;
      CHECK_STATUS(second.take(2u, handle), av::Status::ok);
      CHECK(!backend.adopt(second));
      {
          const av::glproto::RealGl &gl = OpenGlBackendProbe::gl(backend);
          unsigned texture = 0u;
          unsigned framebuffer = 0u;
          gl.glGenTextures(1, &texture);
          gl.glBindTexture(gl_texture_2d, texture);
          gl.glTexParameteri(gl_texture_2d, gl_texture_min_filter, static_cast<int>(gl_nearest));
          gl.glTexImage2D(gl_texture_2d, 0, static_cast<int>(gl_rgba8), 4, 4, 0, gl_rgba, gl_unsigned_byte, nullptr);
          gl.glGenFramebuffers(1, &framebuffer);
          gl.glBindFramebuffer(gl_framebuffer, framebuffer);
          gl.glFramebufferTexture2D(gl_framebuffer, gl_color_attachment0, gl_texture_2d, texture, 0);
          CHECK_EQ(gl.glCheckFramebufferStatus(gl_framebuffer), gl_framebuffer_complete);
          gl.glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
          gl.glClear(gl_color_buffer_bit);
      }
      std::memset(region, 0, sizeof region);
      /* glReadPixels(0, 0, 4, 4, RGBA, UNSIGNED_BYTE) into the region at one page in. */
      constexpr std::uint64_t pixels = 4u * 4u * 4u;
      std::uint8_t frame[64];
      av::WireWriter read(frame, sizeof frame);
      const std::uint64_t arguments[] = {0u, 0u, 4u, 4u, gl_rgba, gl_unsigned_byte};
      for (const std::uint64_t value : arguments) read.write_varint(value);
      read.write_pod<std::uint8_t>(blob_mode);
      read.write_varint<std::uint64_t>(pixels);
      read.write_varint<std::uint64_t>(page);
      std::uint8_t   block[4096];
      av::WireArena  arena(block, sizeof block);
      std::uint8_t   reply[64] = {};
      av::WireReader payload(frame, read.offset());
      av::WireWriter answer(reply, sizeof reply);
      CHECK(backend.run(av::glproto::kCmd_glReadPixels, payload, answer, arena));
      CHECK_EQ(region[page], 255u);
      CHECK_EQ(region[page + 1u], 0u);
      CHECK_EQ(region[page + 2u], 255u);
      CHECK_EQ(region[page + pixels - 1u], 255u);
      CHECK_EQ(region[0], 0u);
      CHECK_EQ(region[page + pixels], 0u);
      backend.abandon(blob);
      backend.shut_down();
  } }
