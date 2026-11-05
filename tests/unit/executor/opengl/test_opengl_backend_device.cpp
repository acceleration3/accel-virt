#include "core/log/driver_log.hpp"
#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "executor/exported_memory.hpp"
#include "executor/host_memory_source.hpp"
#include "executor/executor_log.hpp"
#include "executor/source_memory.hpp"
#include "proto/submission/blob_role.hpp"
#include "proto/wire/guest_bytes.hpp"
#include "proto/resources/host_memory_request.hpp"
#include "executor/opengl/opengl_backend.hpp"
#include "executor/shim/gl_display.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "executor/sharing/shared_image_shape.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/generated/opengl/gl_dispatch_server.h"
#include "proto/generated/opengl/gl_protocol_ids.h"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <cstdio>
#include <cstring>
#include <EGL/egl.h>
#include <pthread.h>

extern "C" const char *__asan_default_options(void);
extern "C" const char *__asan_default_options(void)
{
    return "detect_leaks=0";
}

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
struct OpenGlBackendProbe {
    static const glproto::RealGl &gl(OpenGlBackend &backend)
    {
        CHECK(backend.context_.make_current());
        return backend.gl_;
    }
    static bool packs(OpenGlBackend &backend) { return backend.pack_.packing(); }
    static std::uint32_t live_syncs(OpenGlBackend &backend) { return backend.fences_.count(); }
    static glproto::RealGl &writable(OpenGlBackend &backend)
    {
        CHECK(backend.context_.make_current());
        return backend.gl_;
    }
    static void install_hooks(OpenGlBackend &backend) { backend.install_hooks(); }
    static void made_current(OpenGlBackend &backend) { backend.current_ = true; }
};

}

using av::OpenGlBackend;
using av::OpenGlBackendProbe;

namespace {
av::BlobRole arena_at(std::uint32_t slot)
{
    return av::BlobRole(av::BlobRole::Kind::arena_segment, std::uint64_t{slot} << av::GuestBytes::slot_shift);
}

std::uint64_t detail = 0u;

constexpr unsigned gl_major_version = 0x821B;
constexpr unsigned gl_minor_version = 0x821C;
constexpr unsigned gl_context_flags = 0x821E;
constexpr unsigned gl_context_profile_mask = 0x9126;
constexpr unsigned gl_reset_notification_strategy = 0x8256;
constexpr unsigned gl_lose_context_on_reset = 0x8252;
constexpr int      gl_context_flag_debug = 0x2;
constexpr int      gl_core_profile_bit = 0x1;
constexpr int      gl_compatibility_profile_bit = 0x2;
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

class Displays {
public:
    Displays() : sink_(), log_(sink_), host_(log_), count_(0u)
    {
        host_.open();
        for (std::uint32_t i = 0u; i < host_.gpu_count(); i++) {
            const av::GlDisplay *const display = host_.gl_display(i);
            CHECK_EQ(display != nullptr, (host_.sub_apis(i) & AV_EXECUTOR_SUB_API_OPENGL) != 0u);
            if (display == nullptr) continue;
            sources_[count_] = host_.memory_source(i);
            displays_[count_++] = display;
        }
    }
    ~Displays() { host_.close(); }
    av::DriverLog       &log() { return log_.opengl(); }
    std::uint32_t        count() const { return count_; }
    const av::GlDisplay *at(std::uint32_t index) const { return displays_[index]; }
    av::HostMemorySource *source(std::uint32_t index) const { return sources_[index]; }

private:
    av::RecordingLog     sink_;
    av::ExecutorLog      log_;
    av::LoaderHostApis   host_;
    const av::GlDisplay *displays_[16] = {};
    av::HostMemorySource *sources_[16] = {};
    std::uint32_t        count_;
};

class SourceAllocation {
public:
    static constexpr std::uint64_t key = 0x77u;
    static constexpr std::uint32_t domain = 1u;

    SourceAllocation(av::HostMemorySource *source, std::uint64_t bytes)
        : source_(source), answered_{-1, 0u, 0u}, memory_(nullptr)
    { const av::HostMemoryRequest readback(av::HostMemoryRequest::Kind::readback, 0u);
      if (source_ == nullptr || source_->allocate(domain, key, bytes, readback, answered_) != av::Status::ok) return;
      memory_ = source_->acquire(domain, key); }
    ~SourceAllocation()
    { if (memory_ != nullptr) source_->release(*memory_);
      if (answered_.fd >= 0) source_->free(domain, key);
      if (answered_.fd >= 0) close(answered_.fd); }
    SourceAllocation(const SourceAllocation &) = delete;
    SourceAllocation &operator=(const SourceAllocation &) = delete;

    bool              made() const { return memory_ != nullptr; }
    int               descriptor() const { return answered_.fd; }
    std::uint64_t     size() const { return answered_.size; }
    av::SourceMemory &memory() { return *memory_; }

private:
    av::HostMemorySource *source_;
    AvHostMemory          answered_;
    av::SourceMemory     *memory_;
};

constexpr unsigned gl_sync_gpu_commands_complete = 0x9117;
constexpr unsigned gl_sync_status = 0x9114;
constexpr int      gl_signaled = 0x9119;
constexpr unsigned gl_already_signaled = 0x911A;
constexpr unsigned gl_condition_satisfied = 0x911C;
constexpr unsigned gl_sync_flush_commands_bit = 0x1;
constexpr std::uint64_t second_ns = 1000000000u;

bool run_command(OpenGlBackend &backend, std::uint32_t id, const av::WireWriter &frame, const std::uint8_t *bytes,
                 std::uint8_t *reply, std::size_t reply_bytes, std::size_t &replied)
{
    std::uint8_t   block[4096];
    av::WireArena  arena(block, sizeof block);
    av::WireReader payload(bytes, frame.offset());
    av::WireWriter answer(reply, reply_bytes);
    const bool ran = backend.run(id, payload, answer, arena);
    replied = answer.offset();
    return ran;
}

bool fence_sync(OpenGlBackend &backend, std::uint64_t token)
{
    std::uint8_t   frame[32];
    av::WireWriter writer(frame, sizeof frame);
    writer.write_pod<std::uint64_t>(token);
    writer.write_varint<std::uint64_t>(gl_sync_gpu_commands_complete);
    writer.write_varint<std::uint64_t>(0u);
    std::uint8_t reply[16];
    std::size_t  replied = 0u;
    return run_command(backend, av::glproto::kCmd_glFenceSync, writer, frame, reply, sizeof reply, replied);
}

int sync_status(OpenGlBackend &backend, std::uint64_t token)
{
    std::uint8_t   frame[32];
    av::WireWriter writer(frame, sizeof frame);
    writer.write_pod<std::uint64_t>(token);
    writer.write_varint<std::uint64_t>(gl_sync_status);
    writer.write_varint<std::uint64_t>(1u);
    writer.write_pod<std::uint8_t>(0u);
    writer.write_pod<std::uint8_t>(1u);
    writer.write_varint<std::uint64_t>(sizeof(int));
    std::uint8_t reply[32];
    std::size_t  replied = 0u;
    if (!run_command(backend, av::glproto::kCmd_glGetSynciv, writer, frame, reply, sizeof reply, replied)) return -1;
    av::WireReader answer(reply, replied);
    std::uint64_t  bytes = 0u;
    int            value = -1;
    if (!answer.read_varint(bytes) || bytes != sizeof value || !answer.read(&value, sizeof value)) return -1;
    return value;
}

unsigned client_wait(OpenGlBackend &backend, std::uint64_t token)
{
    std::uint8_t   frame[32];
    av::WireWriter writer(frame, sizeof frame);
    writer.write_pod<std::uint64_t>(token);
    writer.write_varint<std::uint64_t>(gl_sync_flush_commands_bit);
    writer.write_varint<std::uint64_t>(second_ns);
    std::uint8_t reply[16];
    std::size_t  replied = 0u;
    if (!run_command(backend, av::glproto::kCmd_glClientWaitSync, writer, frame, reply, sizeof reply, replied))
        return 0u;
    av::WireReader answer(reply, replied);
    std::uint64_t  reached = 0u;
    return answer.read_varint(reached) ? static_cast<unsigned>(reached) : 0u;
}

bool delete_sync(OpenGlBackend &backend, std::uint64_t token)
{
    std::uint8_t   frame[16];
    av::WireWriter writer(frame, sizeof frame);
    writer.write_pod<std::uint64_t>(token);
    std::uint8_t reply[16];
    std::size_t  replied = 0u;
    return run_command(backend, av::glproto::kCmd_glDeleteSync, writer, frame, reply, sizeof reply, replied);
}

bool passed(unsigned reached) { return reached == gl_already_signaled || reached == gl_condition_satisfied; }

int integer(OpenGlBackend &backend, unsigned name)
{
    const av::glproto::RealGl &gl = OpenGlBackendProbe::gl(backend);
    int value = -1;
    gl.glGetIntegerv(name, &value);
    return value;
}

}

TEST(opengl_backend_device, a_context_reports_the_version_and_profile_it_was_made_with)
{ Displays displays;
  for (std::uint32_t index = 0u; index < displays.count(); index++) {
      const AvContextOptions core_46 = {4u, 6u, AV_CONTEXT_PROFILE_CORE, 0u, 0u};
      OpenGlBackend newest(displays.log());
      CHECK(newest.initialize(displays.at(index), &core_46, nullptr));
      CHECK_EQ(integer(newest, gl_major_version), 4);
      CHECK_EQ(integer(newest, gl_minor_version), 6);
      CHECK_EQ(integer(newest, gl_context_profile_mask) & gl_core_profile_bit, gl_core_profile_bit);
      newest.shut_down();
      const AvContextOptions core_33 = {3u, 3u, AV_CONTEXT_PROFILE_CORE, 0u, 0u};
      OpenGlBackend older(displays.log());
      CHECK(older.initialize(displays.at(index), &core_33, nullptr));
      const int version = integer(older, gl_major_version) * 10 + integer(older, gl_minor_version);
      CHECK(version >= 33);
      CHECK_EQ(integer(older, gl_context_profile_mask) & gl_core_profile_bit, gl_core_profile_bit);
      older.shut_down();
      const AvContextOptions refused[] = {{4u, 7u, AV_CONTEXT_PROFILE_CORE, 0u, 0u},
                                           {5u, 0u, AV_CONTEXT_PROFILE_CORE, 0u, 0u}};
      for (const AvContextOptions &options : refused) {
          OpenGlBackend none(displays.log());
          CHECK(!none.initialize(displays.at(index), &options, nullptr));
          CHECK_EQ(none.resolved(), 0u);
      }
  } }

TEST(opengl_backend_device, a_compatibility_context_is_made_where_the_host_has_one)
{ Displays displays;
  for (std::uint32_t index = 0u; index < displays.count(); index++) {
      const AvContextOptions compatibility_46 = {4u, 6u, AV_CONTEXT_PROFILE_COMPATIBILITY, 0u, 0u};
      OpenGlBackend backend(displays.log());
      if (!backend.initialize(displays.at(index), &compatibility_46, nullptr)) {
          std::fprintf(stderr, "display %u grants no 4.6 compatibility context\n", index);
          CHECK_EQ(backend.resolved(), 0u);
          continue;
      }
      CHECK_EQ(integer(backend, gl_major_version), 4);
      CHECK_EQ(integer(backend, gl_minor_version), 6);
      CHECK_EQ(integer(backend, gl_context_profile_mask) & gl_compatibility_profile_bit,
               gl_compatibility_profile_bit);
      CHECK_EQ(integer(backend, gl_context_profile_mask) & gl_core_profile_bit, 0);
      CHECK(av::OpenGlBackendProbe::gl(backend).glNewList != nullptr);
      CHECK(av::OpenGlBackendProbe::gl(backend).glCallLists != nullptr);
      CHECK(av::OpenGlBackendProbe::gl(backend).glBegin != nullptr);
      backend.shut_down();
  } }

TEST(opengl_backend_device, debug_and_robust_contexts_report_their_flags)
{ Displays displays;
  for (std::uint32_t index = 0u; index < displays.count(); index++) {
      const AvContextOptions debug = {4u, 3u, AV_CONTEXT_PROFILE_CORE, AV_CONTEXT_FLAG_DEBUG, 0u};
      OpenGlBackend debugging(displays.log());
      CHECK(debugging.initialize(displays.at(index), &debug, nullptr));
      CHECK_EQ(integer(debugging, gl_context_flags) & gl_context_flag_debug, gl_context_flag_debug);
      debugging.shut_down();
      const AvContextOptions robust = {4u, 3u, AV_CONTEXT_PROFILE_CORE,
                                        AV_CONTEXT_FLAG_ROBUST_ACCESS | AV_CONTEXT_FLAG_LOSE_CONTEXT_ON_RESET, 0u};
      OpenGlBackend robustly(displays.log());
      CHECK(robustly.initialize(displays.at(index), &robust, nullptr));
      CHECK_EQ(integer(robustly, gl_reset_notification_strategy), static_cast<int>(gl_lose_context_on_reset));
      robustly.shut_down();
      OpenGlBackend plain(displays.log());
      CHECK(plain.initialize(displays.at(index), nullptr, nullptr));
      CHECK_EQ(integer(plain, gl_context_flags) & gl_context_flag_debug, 0);
      plain.shut_down();
  } }

namespace {
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
    gl.glFlush();
    advance(holder, 1);
    await(holder, 2);
    CHECK(eglMakeCurrent(eglGetCurrentDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_TRUE);
    return nullptr;
}

}

TEST(opengl_backend_device, a_sharing_context_sees_a_texture_made_on_another_thread)
{ Displays displays;
  for (std::uint32_t index = 0u; index < displays.count(); index++) {
      OpenGlBackend first(displays.log());
      CHECK(first.initialize(displays.at(index), nullptr, nullptr));
      CHECK(eglMakeCurrent(eglGetCurrentDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_TRUE);
      Holder holder = {&first, 0u, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, 0};
      pthread_t thread;
      CHECK_EQ(pthread_create(&thread, nullptr, hold_current, &holder), 0);
      await(holder, 1);
      const AvContextOptions core = {4u, 3u, AV_CONTEXT_PROFILE_CORE, 0u, 0u};
      OpenGlBackend second(displays.log());
      CHECK(second.initialize(displays.at(index), &core, &first));
      const av::glproto::RealGl &gl = OpenGlBackendProbe::gl(second);
      CHECK(holder.texture != 0u);
      CHECK_EQ(gl.glIsTexture(holder.texture), 1u);
      OpenGlBackend alone(displays.log());
      CHECK(alone.initialize(displays.at(index), &core, nullptr));
      CHECK_EQ(OpenGlBackendProbe::gl(alone).glIsTexture(holder.texture), 0u);
      advance(holder, 2);
      CHECK_EQ(pthread_join(thread, nullptr), 0);
      alone.shut_down();
      second.shut_down();
      OpenGlBackend orphan(displays.log());
      CHECK(!orphan.initialize(displays.at(index), &core, &second));
      first.shut_down();
  }
  if (displays.count() < 2u) return;
  OpenGlBackend here(displays.log());
  CHECK(here.initialize(displays.at(0u), nullptr, nullptr));
  OpenGlBackend there(displays.log());
  CHECK(!there.initialize(displays.at(1u), nullptr, &here));
  here.shut_down(); }

TEST(opengl_backend_device, a_pixel_read_lands_in_the_adopted_region)
{ Displays displays;
  for (std::uint32_t index = 0u; index < displays.count(); index++) {
      OpenGlBackend backend(displays.log());
      CHECK(backend.initialize(displays.at(index), nullptr, nullptr));
      av::SourceMemory memory(1u, 1u, 0u, AV_MEMORY_WRITE_BACK, av::ExportedMemory(1u, -1, region, sizeof region));
      std::uint64_t handle = 0u;
      CHECK(backend.adopt(memory, arena_at(0u), handle));
      CHECK_EQ(handle, static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(region)));
      CHECK(!OpenGlBackendProbe::packs(backend));
      std::uint64_t second = 7u;
      CHECK(!backend.adopt(memory, arena_at(0u), second));
      CHECK_EQ(second, 0u);
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
      backend.unbind_arena(memory);
      backend.shut_down();
  } }

TEST(opengl_backend_device, a_pixel_read_into_a_source_region_is_written_by_the_gpu)
{ Displays displays;
  for (std::uint32_t index = 0u; index < displays.count(); index++) {
      SourceAllocation allocation(displays.source(index), sizeof region);
      if (!allocation.made()) {
          std::fprintf(stderr, "display %u has no memory source: nothing judged\n", index);
          continue;
      }
      std::uint8_t *const guest = allocation.memory().pointer();
      std::memset(guest, 0, sizeof region);
      OpenGlBackend backend(displays.log());
      CHECK(backend.initialize(displays.at(index), nullptr, nullptr));
      std::uint64_t handle = 0u;
      CHECK(backend.adopt(allocation.memory(), arena_at(0u), handle));
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
          gl.glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
          gl.glClear(gl_color_buffer_bit);
      }
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
      CHECK(OpenGlBackendProbe::packs(backend));
      CHECK_EQ(guest[page], 255u);
      CHECK_EQ(guest[page + 1u], 0u);
      CHECK_EQ(guest[page + 2u], 255u);
      CHECK_EQ(guest[page + pixels - 1u], 255u);
      CHECK_EQ(guest[0], 0u);
      CHECK_EQ(guest[page + pixels], 0u);
      backend.unbind_arena(allocation.memory());
      backend.shut_down();
  } }

TEST(opengl_backend_device, imports_a_dma_buf_as_a_memory_object_and_as_a_semaphore)
{ Displays          displays;
  for (std::uint32_t index = 0u; index < displays.count(); index++) {
      SourceAllocation descriptor(displays.source(index), page);
      if (!descriptor.made()) { std::fprintf(stderr, "display %u makes no dma-buf\n", index); continue; }
      OpenGlBackend backend(displays.log());
      CHECK(backend.initialize(displays.at(index), nullptr, nullptr));
      std::uint64_t memory = 9u;
      if (!backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, descriptor.descriptor(),
                                 page, av::SharedImageShape(), 0u, memory, detail)) {
          std::fprintf(stderr, "display %u imports no memory object from a descriptor\n", index);
          CHECK_EQ(memory, 0u);
          backend.shut_down();
          continue;
      }
      CHECK(memory != 0u);
      std::uint64_t again = 0u;
      CHECK(backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, descriptor.descriptor(),
                                  page, av::SharedImageShape(), 0u, again, detail));
      CHECK(again != 0u);
      CHECK(again != memory);

      backend.release_object(AV_SHARED_MEMORY, memory + again + 1u, detail);
      backend.release_object(AV_SHARED_MEMORY, 0x1'0000'0000ull | memory, detail);
      backend.release_object(AV_SHARED_MONITORED_FENCE, memory, detail);
      backend.release_object(AV_SHARED_MEMORY, memory, detail);
      backend.release_object(AV_SHARED_MEMORY, memory, detail);

      std::uint64_t fence = 0u;
      if (backend.import_object(AV_SHARED_MONITORED_FENCE, AV_SHARED_HANDLE_DMA_BUF, descriptor.descriptor(),
                                0u, av::SharedImageShape(), 0u, fence, detail)) {
          CHECK(fence != 0u);
          backend.release_object(AV_SHARED_NATIVE_FENCE, fence, detail);
      }
      std::uint64_t native = 0u;
      if (backend.import_object(AV_SHARED_NATIVE_FENCE, AV_SHARED_HANDLE_DMA_BUF, descriptor.descriptor(),
                                4096u, av::SharedImageShape(), 0u, native, detail)) {
          CHECK(native != 0u);
          backend.release_object(AV_SHARED_NATIVE_FENCE, native, detail);
      }
      backend.shut_down();
      std::uint64_t after = 9u;
      CHECK(!backend.import_object(AV_SHARED_NATIVE_FENCE, AV_SHARED_HANDLE_DMA_BUF,
                                   descriptor.descriptor(), 0u, av::SharedImageShape(), 0u, after, detail));
      CHECK_EQ(after, 0u);
  } }

TEST(opengl_backend_device, a_context_imports_more_than_any_fixed_table_held)
{ Displays        displays;
  constexpr std::uint32_t many = 80u;
  for (std::uint32_t index = 0u; index < displays.count(); index++) {
      SourceAllocation descriptor(displays.source(index), page);
      if (!descriptor.made()) continue;
      OpenGlBackend backend(displays.log());
      CHECK(backend.initialize(displays.at(index), nullptr, nullptr));
      std::uint64_t made[many] = {};
      if (!backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, descriptor.descriptor(),
                                 page, av::SharedImageShape(), 0u, made[0], detail)) {
          backend.shut_down();
          continue;
      }
      for (std::uint32_t i = 1u; i < many; i++)
          CHECK(backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, descriptor.descriptor(),
                                      page, av::SharedImageShape(), 0u, made[i], detail));
      for (std::uint32_t i = 1u; i < many; i++) CHECK(made[i] != made[i - 1u]);
      for (const std::uint64_t name : made) backend.release_object(AV_SHARED_MEMORY, name, detail);
      backend.shut_down();
  } }

TEST(opengl_backend_device, a_context_that_never_deletes_a_sync_has_every_one_answered)
{ Displays displays;
  constexpr std::uint64_t fences = 3000u;
  for (std::uint32_t index = 0u; index < displays.count(); index++) {
      OpenGlBackend backend(displays.log());
      CHECK(backend.initialize(displays.at(index), nullptr, nullptr));
      for (std::uint64_t token = 1u; token <= fences; token++) {
          CHECK(fence_sync(backend, token));
          if (token % 128u == 0u) CHECK(passed(client_wait(backend, token)));
          CHECK(OpenGlBackendProbe::live_syncs(backend) <= av::SyncTokens::retire_after);
      }
      CHECK_EQ(OpenGlBackendProbe::live_syncs(backend), av::SyncTokens::retire_after);
      CHECK(passed(client_wait(backend, fences)));
      std::uint64_t signaled = 0u;
      for (std::uint64_t token = 1u; token <= fences; token++)
          signaled += sync_status(backend, token) == gl_signaled ? 1u : 0u;
      CHECK_EQ(signaled, fences);
      CHECK_EQ(sync_status(backend, fences + 1u), 0);
      CHECK_EQ(sync_status(backend, 0u), 0);
      CHECK_EQ(client_wait(backend, 1u), gl_already_signaled);
      CHECK(delete_sync(backend, 1u));
      CHECK_EQ(sync_status(backend, 1u), 0);
      CHECK_EQ(sync_status(backend, 2u), gl_signaled);
      CHECK(delete_sync(backend, fences));
      CHECK_EQ(sync_status(backend, fences), 0);
      CHECK_EQ(sync_status(backend, fences - 1u), gl_signaled);
      CHECK_EQ(OpenGlBackendProbe::live_syncs(backend), av::SyncTokens::retire_after - 1u);
      backend.end_batch();
      backend.shut_down();
  } }

TEST(opengl_backend_device, contexts_fencing_in_turn_answer_only_their_own_recent_syncs)
{ Displays displays;
  constexpr std::uint64_t fences = 1000u;
  for (std::uint32_t index = 0u; index < displays.count(); index++) {
      OpenGlBackend contexts[2] = {OpenGlBackend(displays.log()), OpenGlBackend(displays.log())};
      for (OpenGlBackend &context : contexts) CHECK(context.initialize(displays.at(index), nullptr, nullptr));
      for (std::uint64_t token = 1u; token <= 2u * fences; token++) {
          OpenGlBackend &context = contexts[token % 2u];
          CHECK(fence_sync(context, token));
          context.end_batch();
      }
      for (std::uint32_t which = 0u; which < 2u; which++) {
          OpenGlBackend &context = contexts[which];
          const std::uint64_t newest = 2u * fences - (which == 0u ? 0u : 1u);
          CHECK(passed(client_wait(context, newest)));
          std::uint64_t signaled = 0u;
          for (std::uint64_t token = 2u - which; token <= 2u * fences; token += 2u)
              signaled += sync_status(context, token) == gl_signaled ? 1u : 0u;
          CHECK_EQ(signaled, fences);
          CHECK_EQ(sync_status(context, newest - 1u), 0);
          CHECK_EQ(sync_status(context, newest - 2u * av::SyncTokens::retire_after - 1u), 0);
          CHECK_EQ(sync_status(context, newest - 2u * av::SyncTokens::retire_after), gl_signaled);
          context.end_batch();
      }
      for (OpenGlBackend &context : contexts) context.shut_down();
  } }

namespace {

constexpr unsigned gl_timeout_expired = 0x911B, gl_wait_failed = 0x911D, gl_invalid_enum = 0x0500,
                   gl_context_lost = 0x0507;

unsigned      wait_answer = gl_already_signaled;
GLsync        waited_on = nullptr;
GLbitfield    waited_flags = 0u;
GLuint64      waited_ns = 0u;
std::uint32_t waits = 0u;

GLenum client_wait_sync(GLsync sync, GLbitfield flags, GLuint64 timeout)
{
    waited_on = sync;
    waited_flags = flags;
    waited_ns = timeout;
    waits++;
    return wait_answer;
}

constexpr std::uint32_t most_deleted = 16u;
GLsync        deleted[most_deleted] = {};
GLsync        last_deleted = nullptr;
std::uint32_t deletes = 0u;

void record_deleted_sync(GLsync sync)
{
    if (deletes < most_deleted) deleted[deletes] = sync;
    last_deleted = sync;
    deletes++;
}

char syncs[av::SyncTokens::retire_after + 8u];

GLsync sync_at(std::uint32_t index) { return reinterpret_cast<GLsync>(&syncs[index]); }

std::uint32_t errors_left = 0u;
std::uint32_t errors_asked = 0u;

GLenum pending_error()
{
    errors_asked++;
    if (errors_left == 0u) return 0u;
    errors_left--;
    return gl_invalid_enum;
}

GLenum lost_context_error()
{
    errors_asked++;
    return gl_context_lost;
}

av::glproto::RealGl &faked(OpenGlBackend &backend, unsigned answer)
{
    av::glproto::RealGl &gl = OpenGlBackendProbe::writable(backend);
    gl.glClientWaitSync = client_wait_sync;
    gl.glDeleteSync = record_deleted_sync;
    wait_answer = answer;
    waited_on = nullptr;
    waits = 0u;
    deletes = 0u;
    last_deleted = nullptr;
    return gl;
}

void fill_waiting(OpenGlBackend &backend)
{
    for (std::uint32_t i = 0u; i < av::SyncTokens::retire_after; i++)
        CHECK(backend.bind_fence(i + 1u, sync_at(i)));
    CHECK_EQ(waits, 0u);
}

bool delete_through_decoder(OpenGlBackend &backend, std::uint64_t token)
{
    std::uint8_t   frame[16];
    av::WireWriter writer(frame, sizeof frame);
    writer.write_pod<std::uint64_t>(token);
    std::uint8_t   block[256];
    av::WireArena  arena(block, sizeof block);
    std::uint8_t   reply[16];
    av::WireReader payload(frame, writer.offset());
    av::WireWriter answer(reply, sizeof reply);
    return av::glproto::dispatch_command(OpenGlBackendProbe::writable(backend), av::glproto::kCmd_glDeleteSync,
                                         payload, answer, arena);
}

}

TEST(opengl_backend_device, a_fence_is_found_only_by_the_token_it_was_bound_to)
{ Displays displays;
  for (std::uint32_t index = 0u; index < displays.count(); index++) {
      OpenGlBackend backend(displays.log());
      CHECK(backend.initialize(displays.at(index), nullptr, nullptr));
      faked(backend, gl_already_signaled);
      int fence = 0;
      CHECK(backend.fence_for(7u) == nullptr);
      CHECK(backend.bind_fence(7u, &fence));
      CHECK(backend.fence_for(7u) == &fence);
      CHECK(backend.fence_for(8u) == nullptr);
      CHECK(backend.forget_fence(7u) == &fence);
      CHECK(backend.fence_for(7u) == nullptr);
      CHECK(backend.forget_fence(7u) == nullptr);
      backend.shut_down();
  } }

TEST(opengl_backend_device, a_sync_no_token_can_keep_is_deleted_at_once)
{ Displays displays;
  for (std::uint32_t index = 0u; index < displays.count(); index++) {
      OpenGlBackend backend(displays.log());
      CHECK(backend.initialize(displays.at(index), nullptr, nullptr));
      faked(backend, gl_already_signaled);
      int first = 0, second = 0;
      CHECK(backend.bind_fence(7u, &first));
      CHECK(!backend.bind_fence(7u, &second));
      CHECK_EQ(deletes, 1u);
      CHECK(!backend.bind_fence(6u, &second));
      CHECK_EQ(deletes, 2u);
      CHECK(backend.fence_for(7u) == &first);
      CHECK(backend.fence_for(6u) == nullptr);
      CHECK(!backend.bind_fence(8u, nullptr));
      CHECK_EQ(deletes, 2u);
      CHECK(backend.forget_fence(7u) == &first);
      backend.shut_down();
  } }

TEST(opengl_backend_device, a_sync_the_table_cannot_grow_to_hold_is_deleted_and_raises_out_of_memory)
{ Displays displays;
  for (std::uint32_t index = 0u; index < displays.count(); index++) {
      av::MockPlatform memory;
      OpenGlBackend    backend(memory, displays.log());
      CHECK(backend.initialize(displays.at(index), nullptr, nullptr));
      faked(backend, gl_already_signaled);
      memory.fail_allocation_at(1u);
      CHECK(!backend.bind_fence(1u, sync_at(0u)));
      CHECK_EQ(deletes, 1u);
      CHECK(deleted[0] == sync_at(0u));
      CHECK(backend.fence_for(1u) == nullptr);
      CHECK_EQ(OpenGlBackendProbe::live_syncs(backend), 0u);
      memory.fail_allocation_at(0u);
      CHECK(backend.bind_fence(2u, sync_at(1u)));
      CHECK(backend.fence_for(2u) == sync_at(1u));
      backend.shut_down();
  } }

TEST(opengl_backend_device, a_full_table_retires_its_oldest_once_the_gpu_passed_it)
{ Displays displays;
  for (std::uint32_t index = 0u; index < displays.count(); index++) {
      OpenGlBackend backend(displays.log());
      CHECK(backend.initialize(displays.at(index), nullptr, nullptr));
      faked(backend, gl_already_signaled);
      fill_waiting(backend);
      const std::uint64_t next = av::SyncTokens::retire_after + 1u;
      CHECK(backend.bind_fence(next, sync_at(next - 1u)));
      CHECK_EQ(waits, 1u);
      CHECK(waited_on == sync_at(0u));
      CHECK_EQ(waited_flags, gl_sync_flush_commands_bit);
      CHECK_EQ(waited_ns, 0u);
      CHECK_EQ(deletes, 0u);
      CHECK(backend.fence_for(1u) == sync_at(0u));
      CHECK(backend.fence_for(next) == sync_at(next - 1u));
      wait_answer = gl_condition_satisfied;
      CHECK(backend.bind_fence(next + 1u, sync_at(next)));
      CHECK_EQ(waits, 2u);
      CHECK(waited_on == sync_at(1u));
      CHECK_EQ(deletes, 1u);
      CHECK(deleted[0] == sync_at(1u));
      CHECK(backend.fence_for(2u) == sync_at(0u));
      CHECK(backend.fence_for(1u) == sync_at(0u));
      CHECK_EQ(OpenGlBackendProbe::live_syncs(backend), av::SyncTokens::retire_after);
      backend.shut_down();
      CHECK_EQ(deletes, 1u + av::SyncTokens::retire_after + 1u);
      CHECK(last_deleted == sync_at(0u));
      CHECK(backend.fence_for(1u) == nullptr);
  } }

TEST(opengl_backend_device, a_full_table_whose_oldest_never_signals_grows_past_it_and_retires_it_when_it_does)
{ Displays displays;
  const unsigned unsignaled[] = {gl_timeout_expired, gl_wait_failed};
  for (std::uint32_t index = 0u; index < displays.count(); index++) {
      for (const unsigned answer : unsignaled) {
          OpenGlBackend backend(displays.log());
          CHECK(backend.initialize(displays.at(index), nullptr, nullptr));
          faked(backend, answer);
          fill_waiting(backend);
          const std::uint64_t next = av::SyncTokens::retire_after + 1u;
          CHECK(backend.bind_fence(next, sync_at(next - 1u)));
          CHECK_EQ(waits, 1u);
          CHECK_EQ(deletes, 0u);
          CHECK(backend.fence_for(next) == sync_at(next - 1u));
          CHECK(backend.fence_for(1u) == sync_at(0u));
          CHECK_EQ(OpenGlBackendProbe::live_syncs(backend), av::SyncTokens::retire_after + 1u);
          wait_answer = gl_already_signaled;
          CHECK(backend.bind_fence(next + 1u, sync_at(next)));
          CHECK_EQ(waits, 2u);
          CHECK(backend.fence_for(1u) == sync_at(0u));
          CHECK(backend.fence_for(next + 1u) == sync_at(next));
          CHECK_EQ(OpenGlBackendProbe::live_syncs(backend), av::SyncTokens::retire_after + 1u);
          backend.shut_down();
      }
  } }

TEST(opengl_backend_device, a_full_table_with_no_way_to_wait_grows_instead)
{ Displays displays;
  for (std::uint32_t index = 0u; index < displays.count(); index++) {
      OpenGlBackend backend(displays.log());
      CHECK(backend.initialize(displays.at(index), nullptr, nullptr));
      av::glproto::RealGl &gl = faked(backend, gl_already_signaled);
      fill_waiting(backend);
      gl.glClientWaitSync = nullptr;
      const std::uint64_t next = av::SyncTokens::retire_after + 1u;
      CHECK(backend.bind_fence(next, sync_at(next - 1u)));
      CHECK_EQ(deletes, 0u);
      CHECK(backend.fence_for(1u) == sync_at(0u));
      CHECK(backend.fence_for(next) == sync_at(next - 1u));
      CHECK_EQ(OpenGlBackendProbe::live_syncs(backend), av::SyncTokens::retire_after + 1u);
      backend.shut_down();
  } }

TEST(opengl_backend_device, deleting_a_retired_sync_through_the_decoder_spares_the_stand_in)
{ Displays displays;
  for (std::uint32_t index = 0u; index < displays.count(); index++) {
      OpenGlBackend backend(displays.log());
      CHECK(backend.initialize(displays.at(index), nullptr, nullptr));
      av::glproto::RealGl &gl = faked(backend, gl_already_signaled);
      fill_waiting(backend);
      OpenGlBackendProbe::install_hooks(backend);
      const std::uint64_t next = av::SyncTokens::retire_after + 1u;
      CHECK(backend.bind_fence(next, sync_at(next - 1u)));
      CHECK(backend.bind_fence(next + 1u, sync_at(next)));
      CHECK_EQ(deletes, 1u);
      CHECK(delete_through_decoder(backend, 1u));
      CHECK_EQ(deletes, 2u);
      CHECK(deleted[1] == nullptr);
      CHECK(backend.fence_for(1u) == nullptr);
      CHECK(backend.fence_for(2u) == sync_at(0u));
      CHECK(delete_through_decoder(backend, next));
      CHECK(deleted[2] == sync_at(next - 1u));
      CHECK(backend.fence_for(next) == nullptr);
      CHECK(delete_through_decoder(backend, next + 100u));
      CHECK(deleted[3] == nullptr);
      gl.glDeleteSync = nullptr;
      CHECK(!delete_through_decoder(backend, 2u));
      CHECK(backend.fence_for(2u) == sync_at(0u));
      gl.glDeleteSync = record_deleted_sync;
      backend.shut_down();
  } }

TEST(opengl_backend_device, a_lost_context_is_a_lost_device_and_an_error_is_not)
{ Displays displays;
  for (std::uint32_t index = 0u; index < displays.count(); index++) {
      { OpenGlBackend backend(displays.log());
        CHECK(backend.initialize(displays.at(index), nullptr, nullptr));
        faked(backend, gl_already_signaled).glGetError = pending_error;
        errors_left = 3u;
        OpenGlBackendProbe::made_current(backend);
        backend.end_batch();
        CHECK(!backend.lost());
        backend.shut_down(); }
      { OpenGlBackend backend(displays.log());
        CHECK(backend.initialize(displays.at(index), nullptr, nullptr));
        faked(backend, gl_already_signaled).glGetError = lost_context_error;
        errors_asked = 0u;
        OpenGlBackendProbe::made_current(backend);
        backend.end_batch();
        CHECK(backend.lost());
        CHECK_EQ(errors_asked, 8u);
        backend.shut_down(); }
  } }

TEST(opengl_backend_device, shutting_down_deletes_every_sync_it_holds)
{ Displays displays;
  for (std::uint32_t index = 0u; index < displays.count(); index++) {
      OpenGlBackend backend(displays.log());
      CHECK(backend.initialize(displays.at(index), nullptr, nullptr));
      faked(backend, gl_already_signaled);
      int first_sync = 0, second_sync = 0;
      CHECK(backend.bind_fence(3u, &first_sync));
      CHECK(backend.bind_fence(4u, &second_sync));
      backend.shut_down();
      CHECK_EQ(deletes, 2u);
      CHECK(backend.fence_for(3u) == nullptr);
      CHECK(backend.fence_for(4u) == nullptr);
  } }
