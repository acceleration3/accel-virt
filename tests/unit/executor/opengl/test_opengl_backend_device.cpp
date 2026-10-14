/* One guest context's OpenGL made the way the application asked, on every GPU
 * this host serves OpenGL on (feature 050): the version and profile it reports,
 * its debug and robustness state, a context sharing another's objects while
 * that one is current on another thread, and the imported region a pixel read
 * lands in. A host with no OpenGL returns without judging. */
#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "executor/exported_memory.hpp"
#include "executor/host_memory_source.hpp"
#include "executor/imported_blob.hpp"
#include "executor/source_memory.hpp"
#include "proto/resources/host_memory_request.hpp"
#include "executor/opengl/opengl_backend.hpp"
#include "executor/shim/gl_display.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "executor/sharing/shared_image_shape.hpp"
#include "harness/framework/test_registry.hpp"
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
    /* Whether a pixel read has had a pack buffer made over the region. */
    static bool packs(OpenGlBackend &backend) { return backend.pack_.packing(); }
};

}  // namespace av

using av::OpenGlBackend;
using av::OpenGlBackendProbe;

namespace {

/* What the last import answered beside its handle, handed back to its release. */
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

/* Every display this host keeps, in order; the count. */
class Displays {
public:
    Displays() : count_(0u)
    {
        host_.open();
        for (std::uint32_t i = 0u; i < host_.gpu_count(); i++) {
            const av::GlDisplay *const display = host_.gl_display(i);
            /* The host keeps a display exactly where it says OpenGL runs. */
            CHECK_EQ(display != nullptr, (host_.sub_apis(i) & AV_EXECUTOR_SUB_API_OPENGL) != 0u);
            if (display == nullptr) continue;
            sources_[count_] = host_.memory_source(i);
            displays_[count_++] = display;
        }
    }
    ~Displays() { host_.close(); }
    std::uint32_t        count() const { return count_; }
    const av::GlDisplay *at(std::uint32_t index) const { return displays_[index]; }
    /* The memory source of the GPU display `index` is on, or null. */
    av::HostMemorySource *source(std::uint32_t index) const { return sources_[index]; }

private:
    av::LoaderHostApis   host_;
    const av::GlDisplay *displays_[16] = {};
    av::HostMemorySource *sources_[16] = {};
    std::uint32_t        count_;
};

/* One allocation of a GPU's memory source, of `request`, as the device asks
 * for one: the descriptor it was answered with - what the device maps into
 * the window, and what a shared object's import names - and the memory a
 * context imports, held for as long as this is. */
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
      const AvContextOptions core_46 = {4u, 6u, AV_CONTEXT_PROFILE_CORE, 0u, 0u};
      OpenGlBackend newest;
      CHECK(newest.initialize(displays.at(index), &core_46, nullptr));
      CHECK_EQ(integer(newest, gl_major_version), 4);
      CHECK_EQ(integer(newest, gl_minor_version), 6);
      CHECK_EQ(integer(newest, gl_context_profile_mask) & gl_core_profile_bit, gl_core_profile_bit);
      newest.shut_down();
      const AvContextOptions core_33 = {3u, 3u, AV_CONTEXT_PROFILE_CORE, 0u, 0u};
      OpenGlBackend older;
      CHECK(older.initialize(displays.at(index), &core_33, nullptr));
      const int version = integer(older, gl_major_version) * 10 + integer(older, gl_minor_version);
      CHECK(version >= 33);
      CHECK_EQ(integer(older, gl_context_profile_mask) & gl_core_profile_bit, gl_core_profile_bit);
      older.shut_down();
      /* No OpenGL has these, and EGL is not asked for them. */
      const AvContextOptions refused[] = {{4u, 7u, AV_CONTEXT_PROFILE_CORE, 0u, 0u},
                                           {5u, 0u, AV_CONTEXT_PROFILE_CORE, 0u, 0u}};
      for (const AvContextOptions &options : refused) {
          OpenGlBackend none;
          CHECK(!none.initialize(displays.at(index), &options, nullptr));
          CHECK_EQ(none.resolved(), 0u);
      }
  } }

/* Feature 051: the compatibility profile a legacy application needs, asked for
 * on every host GPU this machine has. A driver that has none refuses the
 * create, which is the answer the guest ICD ladders down from - so it is
 * reported, not failed. */
TEST(opengl_backend_device, a_compatibility_context_is_made_where_the_host_has_one)
{ Displays displays;
  for (std::uint32_t index = 0u; index < displays.count(); index++) {
      const AvContextOptions compatibility_46 = {4u, 6u, AV_CONTEXT_PROFILE_COMPATIBILITY, 0u, 0u};
      OpenGlBackend backend;
      if (!backend.initialize(displays.at(index), &compatibility_46, nullptr)) {
          std::fprintf(stderr, "display %u grants no 4.6 compatibility context\n", index);
          CHECK_EQ(backend.resolved(), 0u);
          continue;
      }
      CHECK_EQ(integer(backend, gl_major_version), 4);
      CHECK_EQ(integer(backend, gl_minor_version), 6);
      /* The mask is the only answer: one of this machine's two drivers names
       * its compatibility context exactly as it names its core one. */
      CHECK_EQ(integer(backend, gl_context_profile_mask) & gl_compatibility_profile_bit,
               gl_compatibility_profile_bit);
      CHECK_EQ(integer(backend, gl_context_profile_mask) & gl_core_profile_bit, 0);
      /* And the fixed-function entry points it exists for are there. */
      CHECK(av::OpenGlBackendProbe::gl(backend).glNewList != nullptr);
      CHECK(av::OpenGlBackendProbe::gl(backend).glCallLists != nullptr);
      CHECK(av::OpenGlBackendProbe::gl(backend).glBegin != nullptr);
      backend.shut_down();
  } }

TEST(opengl_backend_device, debug_and_robust_contexts_report_their_flags)
{ Displays displays;
  for (std::uint32_t index = 0u; index < displays.count(); index++) {
      const AvContextOptions debug = {4u, 3u, AV_CONTEXT_PROFILE_CORE, AV_CONTEXT_FLAG_DEBUG, 0u};
      OpenGlBackend debugging;
      CHECK(debugging.initialize(displays.at(index), &debug, nullptr));
      CHECK_EQ(integer(debugging, gl_context_flags) & gl_context_flag_debug, gl_context_flag_debug);
      debugging.shut_down();
      const AvContextOptions robust = {4u, 3u, AV_CONTEXT_PROFILE_CORE,
                                        AV_CONTEXT_FLAG_ROBUST_ACCESS | AV_CONTEXT_FLAG_LOSE_CONTEXT_ON_RESET, 0u};
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
    /* A shared object another context may name only after the context that
     * made it has flushed - GL 4.6 section 5.3.2. Without this the texture is
     * there on some drivers and not on others. */
    gl.glFlush();
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
      const AvContextOptions core = {4u, 3u, AV_CONTEXT_PROFILE_CORE, 0u, 0u};
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
      /* Memory with a mapping and no descriptor: what the backend reads
       * into through the CPU. */
      av::SourceMemory memory(1u, 1u, 0u, AV_MEMORY_WRITE_BACK, av::ExportedMemory(1u, -1, region, sizeof region));
      av::ImportedBlob blob;
      CHECK_STATUS(blob.take(1u, memory), av::Status::ok);
      CHECK(backend.adopt(blob));
      CHECK(!OpenGlBackendProbe::packs(backend));
      av::ImportedBlob second;
      CHECK_STATUS(second.take(2u, memory), av::Status::ok);
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

/* A region of the GPU's own memory source - host GPU memory exported as a
 * dma-buf, as every region is - is what a pixel read packs into: the GPU
 * writes the memory itself (GlPackBuffer), and the bytes land where the
 * guest's window shows them, read here through the source's mapping. A GPU
 * whose compositing context would not open judges nothing. */
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
      OpenGlBackend backend;
      CHECK(backend.initialize(displays.at(index), nullptr, nullptr));
      av::ImportedBlob blob;
      CHECK_STATUS(blob.take(1u, allocation.memory()), av::Status::ok);
      CHECK(backend.adopt(blob));
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
      backend.abandon(blob);
      backend.shut_down();
      static_cast<void>(blob.give_back());
  } }

/* Feature 053: a descriptor of this process's own making imported as a memory
 * object of a real GL context, and as a semaphore, and deleted again. A host
 * whose GL has neither fd extension imports nothing and says so, which is the
 * answer the device turns into status 4. */
TEST(opengl_backend_device, imports_a_dma_buf_as_a_memory_object_and_as_a_semaphore)
{ Displays          displays;
  for (std::uint32_t index = 0u; index < displays.count(); index++) {
      SourceAllocation descriptor(displays.source(index), page);
      if (!descriptor.made()) { std::fprintf(stderr, "display %u makes no dma-buf\n", index); continue; }
      OpenGlBackend backend;
      CHECK(backend.initialize(displays.at(index), nullptr, nullptr));
      std::uint64_t memory = 9u;
      if (!backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, descriptor.descriptor(),
                                 page, av::SharedImageShape(), memory, detail)) {
          std::fprintf(stderr, "display %u imports no memory object from a descriptor\n", index);
          CHECK_EQ(memory, 0u);
          backend.shut_down();
          continue;
      }
      CHECK(memory != 0u);
      /* The descriptor stays the caller's: GL took a duplicate, so it can be
       * imported again, which is what a second context opening one object
       * does. */
      std::uint64_t again = 0u;
      CHECK(backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, descriptor.descriptor(),
                                  page, av::SharedImageShape(), again, detail));
      CHECK(again != 0u);
      CHECK(again != memory);

      /* A name this context did not import, and one no GL name could be, are
       * released by nobody. */
      backend.release_object(AV_SHARED_MEMORY, memory + again + 1u, detail);
      backend.release_object(AV_SHARED_MEMORY, 0x1'0000'0000ull | memory, detail);
      backend.release_object(AV_SHARED_MONITORED_FENCE, memory, detail);
      backend.release_object(AV_SHARED_MEMORY, memory, detail);
      backend.release_object(AV_SHARED_MEMORY, memory, detail);

      /* A semaphore is the other name space, and one number can be both. Both
       * guest fence kinds are that one host semaphore, so both are imported:
       * a driver told the wrong one would make a second object nobody would
       * ever release. */
      std::uint64_t fence = 0u;
      if (backend.import_object(AV_SHARED_MONITORED_FENCE, AV_SHARED_HANDLE_DMA_BUF, descriptor.descriptor(),
                                0u, av::SharedImageShape(), fence, detail)) {
          CHECK(fence != 0u);
          /* Either guest fence kind is the one host semaphore. */
          backend.release_object(AV_SHARED_NATIVE_FENCE, fence, detail);
      }
      std::uint64_t native = 0u;
      if (backend.import_object(AV_SHARED_NATIVE_FENCE, AV_SHARED_HANDLE_DMA_BUF, descriptor.descriptor(),
                                4096u, av::SharedImageShape(), native, detail)) {
          CHECK(native != 0u);
          backend.release_object(AV_SHARED_NATIVE_FENCE, native, detail);
      }
      /* What HostShares never released goes with the context. */
      backend.shut_down();
      /* And with the context gone the driver has nothing to make a semaphore
       * on: the import is refused rather than answering a name that names
       * nothing, which is the one refusal on this path the wire's answer
       * depends on and no argument can force. */
      std::uint64_t after = 9u;
      CHECK(!backend.import_object(AV_SHARED_NATIVE_FENCE, AV_SHARED_HANDLE_DMA_BUF,
                                   descriptor.descriptor(), 0u, av::SharedImageShape(), after, detail));
      CHECK_EQ(after, 0u);
  } }

/* A context imports as many shared objects as the guest opens: the table
 * this replaced stopped at 64, and the next import was refused. Each is
 * released by the name it was answered. */
TEST(opengl_backend_device, a_context_imports_more_than_any_fixed_table_held)
{ Displays        displays;
  constexpr std::uint32_t many = 80u;
  for (std::uint32_t index = 0u; index < displays.count(); index++) {
      SourceAllocation descriptor(displays.source(index), page);
      if (!descriptor.made()) continue;
      OpenGlBackend backend;
      CHECK(backend.initialize(displays.at(index), nullptr, nullptr));
      std::uint64_t made[many] = {};
      if (!backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, descriptor.descriptor(),
                                 page, av::SharedImageShape(), made[0], detail)) {
          backend.shut_down();
          continue;
      }
      for (std::uint32_t i = 1u; i < many; i++)
          CHECK(backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, descriptor.descriptor(),
                                      page, av::SharedImageShape(), made[i], detail));
      for (std::uint32_t i = 1u; i < many; i++) CHECK(made[i] != made[i - 1u]);
      for (const std::uint64_t name : made) backend.release_object(AV_SHARED_MEMORY, name, detail);
      backend.shut_down();
  } }
