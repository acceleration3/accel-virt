#include "executor/monotonic_clock.hpp"
#include "harness/executor/software_gl.hpp"
#include "executor/opengl/gl_memory_group.hpp"
#include "executor/teardown_budget.hpp"
#include "harness/executor/generous_budget.hpp"
#include "core/log/driver_log.hpp"
#include "device/abi/av_executor.h"
#include "executor/executor_log.hpp"
#include "executor/opengl/opengl_backend.hpp"
#include "executor/sharing/shared_image_shape.hpp"
#include "executor/shim/gl_display.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "proto/generated/opengl/gl_protocol_ids.h"
#include "proto/submission/gl_error_word.hpp"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <linux/udmabuf.h>

extern "C" const char *__asan_default_options(void);
extern "C" const char *__asan_default_options(void)
{
    return "detect_leaks=0";
}

extern "C" const char *__lsan_default_suppressions(void);
extern "C" const char *__lsan_default_suppressions(void)
{
    return "leak:libEGL\n"
           "leak:libGLX\n"
           "leak:libglapi\n"
           "leak:libz\n"
           "leak:libLLVM\n"
           "leak:libdbus-1\n";
}

namespace av {
struct OpenGlBackendProbe {
    static const glproto::RealGl &gl(OpenGlBackend &backend)
    {
        CHECK(backend.context_.make_current());
        return backend.gl_;
    }
    static glproto::RealGl &writable(OpenGlBackend &backend)
    {
        CHECK(backend.context_.make_current());
        return backend.gl_;
    }
    static GlMemoryObjects &memories(OpenGlBackend &backend) { return backend.memories_; }
    static GlDmaBufImages  &images(OpenGlBackend &backend) { return backend.images_; }
    static uint8_t          errors(OpenGlBackend &backend, uint32_t kind) { return backend.errors_.count(kind); }
};
}

using av::OpenGlBackend;
using av::OpenGlBackendProbe;

namespace {

av::SoftwareGl &software()
{
    static av::SoftwareGl display;
    return display;
}

struct SoftwareOnly {
    SoftwareOnly()
    {
        setenv("LIBGL_ALWAYS_SOFTWARE", "1", 1);
        setenv("MESA_LOADER_DRIVER_OVERRIDE", "swrast", 1);
        setenv("GALLIUM_DRIVER", "llvmpipe", 1);
        setenv("__EGL_VENDOR_LIBRARY_FILENAMES", "/usr/share/glvnd/egl_vendor.d/50_mesa.json", 1);
        setenv("VK_DRIVER_FILES", "/nonexistent", 1);
        setenv("VK_ICD_FILENAMES", "/nonexistent", 1);
        setenv("OCL_ICD_VENDORS", "/nonexistent", 1);
    }
};

const SoftwareOnly software_only;

constexpr unsigned gl_renderer = 0x1F01u;
constexpr unsigned gl_texture_2d = 0x0DE1u;
constexpr unsigned gl_rgba8 = 0x8058u;
constexpr unsigned gl_srgb8_alpha8 = 0x8C43u;
constexpr unsigned gl_rgb565 = 0x8D62u;
constexpr unsigned gl_rgba = 0x1908u;
constexpr unsigned gl_unsigned_byte = 0x1401u;
constexpr unsigned gl_color_buffer_bit = 0x4000u;
constexpr unsigned gl_texture_internal_format = 0x1003u;
constexpr unsigned gl_texture_width = 0x1000u;
constexpr unsigned dxgi_r8g8b8a8_unorm = 28u;
constexpr std::uint64_t memory_bytes = 1u << 20u;
constexpr std::uint32_t side = 64u;
constexpr std::uint32_t row_bytes = side * 4u;
constexpr std::uint32_t iterations = 256u;
constexpr std::uint32_t threads = 8u;

using GetString = const unsigned char *(*)(unsigned);
using IsMemoryObject = unsigned char (*)(unsigned);
using StorageTwoD = void (*)(unsigned, int, unsigned, int, int);
using GetTextureImage = void (*)(unsigned, int, unsigned, unsigned, int, void *);
using GetTextureLevel = void (*)(unsigned, int, unsigned, int *);

av::RecordingLog sink;
av::ExecutorLog  executor_log(sink);

class Command {
public:
    explicit Command(std::uint32_t id) : id_(id), bytes_{}, writer_(bytes_, sizeof bytes_) {}

    Command &varint(std::uint64_t value)
    {
        writer_.write_varint(value);
        return *this;
    }
    Command &pod8(std::uint8_t value)
    {
        writer_.write_pod<std::uint8_t>(value);
        return *this;
    }
    Command &pod32(std::uint32_t value)
    {
        writer_.write_pod<std::uint32_t>(value);
        return *this;
    }

    bool run(OpenGlBackend &backend) const
    {
        std::uint8_t reply[256];
        std::size_t  replied = 0u;
        return run(backend, reply, sizeof reply, replied);
    }

    bool run(OpenGlBackend &backend, std::uint8_t *reply, std::size_t room, std::size_t &replied) const
    {
        std::uint8_t   block[4096];
        av::WireArena  arena(block, sizeof block);
        av::WireReader payload(bytes_, writer_.offset());
        av::WireWriter answer(reply, room);
        const bool     ran = backend.run(id_, payload, answer, arena);
        replied = answer.offset();
        return ran;
    }

private:
    std::uint32_t  id_;
    std::uint8_t   bytes_[256];
    av::WireWriter writer_;
};

bool store_in_memory(OpenGlBackend &backend, unsigned texture, std::uint32_t width, std::uint32_t height,
                     std::uint64_t memory, std::uint64_t offset, unsigned format)
{
    return Command(av::glproto::kCmd_glTextureStorageMem2DEXT)
        .varint(texture)
        .varint(1u)
        .varint(format)
        .varint(width)
        .varint(height)
        .varint(memory)
        .varint(offset)
        .run(backend);
}

bool store_in_memory(OpenGlBackend &backend, unsigned texture, std::uint32_t edge, std::uint64_t memory,
                     std::uint64_t offset, unsigned format = gl_rgba8)
{
    return store_in_memory(backend, texture, edge, edge, memory, offset, format);
}

bool delete_texture(OpenGlBackend &backend, unsigned texture)
{
    return Command(av::glproto::kCmd_glDeleteTextures).varint(1u).pod8(1u).varint(4u).pod32(texture).run(backend);
}

unsigned make_texture(OpenGlBackend &backend)
{
    std::uint8_t reply[32];
    std::size_t  replied = 0u;
    if (!Command(av::glproto::kCmd_glCreateTextures)
             .varint(gl_texture_2d)
             .varint(1u)
             .pod8(1u)
             .varint(sizeof(unsigned))
             .run(backend, reply, sizeof reply, replied))
        return 0u;
    av::WireReader answer(reply, replied);
    std::uint64_t  bytes = 0u;
    unsigned       texture = 0u;
    return answer.read_varint(bytes) && bytes == sizeof texture && answer.read(&texture, sizeof texture) ? texture : 0u;
}

StorageTwoD plain_storage = nullptr;
unsigned    stub_calls = 0u;

void storage_from_memory_as_plain(GLuint texture, GLsizei levels, GLenum format, GLsizei width, GLsizei height,
                                  GLuint, GLuint64)
{
    __atomic_fetch_add(&stub_calls, 1u, __ATOMIC_RELAXED);
    __atomic_load_n(&plain_storage, __ATOMIC_RELAXED)(texture, levels, format, width, height);
}

void use_plain_storage(OpenGlBackend &backend)
{
    __atomic_store_n(&plain_storage, reinterpret_cast<StorageTwoD>(eglGetProcAddress("glTextureStorage2D")),
                     __ATOMIC_RELAXED);
    __atomic_store_n(&stub_calls, 0u, __ATOMIC_RELAXED);
    OpenGlBackendProbe::writable(backend).glTextureStorageMem2DEXT = storage_from_memory_as_plain;
}

bool memory_object_exists(unsigned name)
{
    auto *const is_object = reinterpret_cast<IsMemoryObject>(eglGetProcAddress("glIsMemoryObjectEXT"));
    return is_object != nullptr && is_object(name) != 0;
}

class Memfd {
public:
    explicit Memfd(std::uint64_t bytes) : fd_(memfd_create("av-software", MFD_CLOEXEC))
    {
        if (fd_ >= 0 && ftruncate(fd_, static_cast<off_t>(bytes)) != 0) {
            close(fd_);
            fd_ = -1;
        }
    }
    ~Memfd()
    {
        if (fd_ >= 0) close(fd_);
    }
    Memfd(const Memfd &) = delete;
    Memfd &operator=(const Memfd &) = delete;

    int fd() const { return fd_; }

private:
    int fd_;
};

bool import_memory(OpenGlBackend &backend, int fd, std::uint64_t claimed, const av::SharedImageShape &shape,
                   std::uint64_t &name, std::uint64_t &detail)
{
    return backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, fd, claimed, shape, 0u, name, detail);
}

bool skipped()
{
    if (software().usable()) return false;
    std::fprintf(stderr, "no llvmpipe EGL device: software GL tests skipped\n");
    return true;
}

}

TEST(opengl_backend_software, the_display_under_test_is_llvmpipe_and_nothing_else)
{ if (skipped()) return;
  OpenGlBackend backend(executor_log.opengl());
  CHECK(backend.initialize(software().display(), nullptr, nullptr));
  auto *const get_string = reinterpret_cast<GetString>(eglGetProcAddress("glGetString"));
  OpenGlBackendProbe::gl(backend);
  const char *const renderer = reinterpret_cast<const char *>(get_string(gl_renderer));
  CHECK(renderer != nullptr && std::strstr(renderer, "llvmpipe") != nullptr);
  backend.shut_down(); }

TEST(opengl_backend_software, drain_on_a_backend_that_never_opened_returns_and_has_nothing_in_flight)
{ OpenGlBackend backend(executor_log.opengl());
  CHECK(backend.drain(av::GenerousBudget().budget()));
  std::uint64_t marked = 7u, reached = 7u;
  backend.gpu_progress(marked, reached);
  CHECK_EQ(marked, 0u);
  CHECK_EQ(reached, 0u); }

TEST(opengl_backend_software, drain_finishes_the_work_and_brings_the_reached_mark_up_to_the_marked_one)
{ if (skipped()) return;
  OpenGlBackend backend(executor_log.opengl());
  CHECK(backend.initialize(software().display(), nullptr, nullptr));
  const av::glproto::RealGl &gl = OpenGlBackendProbe::gl(backend);
  CHECK(Command(av::glproto::kCmd_glFlush).run(backend));
  gl.glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
  gl.glClear(gl_color_buffer_bit);
  backend.end_batch();
  std::uint64_t marked = 0u, reached = 0u;
  backend.gpu_progress(marked, reached);
  CHECK_EQ(marked, 1u);
  CHECK(backend.drain(av::GenerousBudget().budget()));
  backend.gpu_progress(marked, reached);
  CHECK_EQ(marked, 1u);
  CHECK_EQ(reached, 1u);
  backend.shut_down(); }

TEST(opengl_backend_software, no_fence_is_made_while_an_occlusion_query_is_open_and_the_owed_one_follows_its_end)
{ if (skipped()) return;
  OpenGlBackend backend(executor_log.opengl());
  CHECK(backend.initialize(software().display(), nullptr, nullptr));
  const av::glproto::RealGl &gl = OpenGlBackendProbe::gl(backend);
  constexpr std::uint64_t samples_passed = 0x8914u;
  GLuint query = 0u;
  gl.glGenQueries(1, &query);
  CHECK(Command(av::glproto::kCmd_glBeginQuery).varint(samples_passed).varint(query).run(backend));
  backend.end_batch();
  std::uint64_t marked = 0u, reached = 0u;
  backend.gpu_progress(marked, reached);
  CHECK_EQ(marked, 0u);
  backend.end_batch();
  backend.gpu_progress(marked, reached);
  CHECK_EQ(marked, 0u);
  CHECK(Command(av::glproto::kCmd_glEndQuery).varint(samples_passed).run(backend));
  backend.gpu_progress(marked, reached);
  CHECK_EQ(marked, 1u);
  backend.end_batch();
  backend.gpu_progress(marked, reached);
  CHECK_EQ(marked, 2u);
  backend.shut_down(); }

TEST(opengl_backend_software, no_fence_is_made_while_advanced_blending_is_enabled_until_something_needs_the_gpu)
{ if (skipped()) return;
  OpenGlBackend backend(executor_log.opengl());
  CHECK(backend.initialize(software().display(), nullptr, nullptr));
  CHECK(Command(av::glproto::kCmd_glBlendEquation).varint(0x9294u).run(backend));
  CHECK(Command(av::glproto::kCmd_glEnable).varint(0x0BE2u).run(backend));
  std::uint64_t marked = 0u, reached = 0u;
  backend.end_batch();
  backend.end_batch();
  backend.gpu_progress(marked, reached);
  CHECK_EQ(marked, 0u);
  CHECK(Command(av::glproto::kCmd_glFlush).run(backend));
  backend.gpu_progress(marked, reached);
  CHECK_EQ(marked, 1u);
  backend.end_batch();
  CHECK(Command(av::glproto::kCmd_glDisable).varint(0x0BE2u).run(backend));
  backend.gpu_progress(marked, reached);
  CHECK_EQ(marked, 2u);
  backend.end_batch();
  backend.gpu_progress(marked, reached);
  CHECK_EQ(marked, 3u);
  backend.shut_down(); }

namespace {

GLenum (*real_wait)(GLsync, GLbitfield, GLuint64) = nullptr;
unsigned waits = 0u;
GLuint64 waited_nanoseconds = 0u;
GLenum   wait_answer = 0u;

GLenum counted_wait(GLsync sync, GLbitfield flags, GLuint64 nanoseconds)
{
    waits++;
    waited_nanoseconds = nanoseconds;
    const GLenum real = real_wait(sync, flags, nanoseconds);
    return wait_answer != 0u ? wait_answer : real;
}

}

TEST(opengl_backend_software, drain_waits_on_a_fence_for_no_longer_than_its_budget_before_it_settles)
{ if (skipped()) return;
  OpenGlBackend backend(executor_log.opengl());
  CHECK(backend.initialize(software().display(), nullptr, nullptr));
  real_wait = OpenGlBackendProbe::writable(backend).glClientWaitSync;
  waits = 0u;
  wait_answer = 0u;
  OpenGlBackendProbe::writable(backend).glClientWaitSync = counted_wait;
  av::MonotonicClock clock;
  av::TeardownBudget budget(clock, 700u * 1000u * 1000u);
  CHECK(backend.drain(budget));
  CHECK_EQ(waits, 1u);
  CHECK(waited_nanoseconds <= 700u * 1000u * 1000u);
  CHECK(waited_nanoseconds > 0u);
  OpenGlBackendProbe::writable(backend).glClientWaitSync = real_wait;
  backend.shut_down(); }

TEST(opengl_backend_software, a_drain_whose_fence_does_not_signal_in_the_budget_is_reported_unfinished)
{ if (skipped()) return;
  OpenGlBackend backend(executor_log.opengl());
  CHECK(backend.initialize(software().display(), nullptr, nullptr));
  real_wait = OpenGlBackendProbe::writable(backend).glClientWaitSync;
  waits = 0u;
  wait_answer = 0x911Bu;
  OpenGlBackendProbe::writable(backend).glClientWaitSync = counted_wait;
  CHECK(!backend.drain(av::GenerousBudget().budget()));
  wait_answer = 0u;
  CHECK(backend.drain(av::GenerousBudget().budget()));
  OpenGlBackendProbe::writable(backend).glClientWaitSync = real_wait;
  backend.shut_down(); }

TEST(opengl_backend_software, drain_reclaims_a_retired_memory_object_once_the_work_that_named_it_is_done)
{ if (skipped()) return;
  OpenGlBackend backend(executor_log.opengl());
  CHECK(backend.initialize(software().display(), nullptr, nullptr));
  Memfd         memfd(memory_bytes);
  std::uint64_t name = 0u, detail = 0u;
  CHECK(import_memory(backend, memfd.fd(), memory_bytes, av::SharedImageShape(), name, detail));
  CHECK(name != 0u);
  CHECK(memory_object_exists(static_cast<unsigned>(name)));
  backend.release_object(AV_SHARED_MEMORY, name, detail);
  CHECK(Command(av::glproto::kCmd_glFlush).run(backend));
  backend.end_batch();
  CHECK(backend.drain(av::GenerousBudget().budget()));
  CHECK(!memory_object_exists(static_cast<unsigned>(name)));
  CHECK_EQ(OpenGlBackendProbe::memories(backend).count(), 0u);
  backend.shut_down(); }

TEST(opengl_backend_software, a_texture_placed_on_imported_memory_keeps_the_memory_object_after_the_memory_is_released)
{ if (skipped()) return;
  OpenGlBackend backend(executor_log.opengl());
  CHECK(backend.initialize(software().display(), nullptr, nullptr));
  use_plain_storage(backend);
  Memfd         memfd(memory_bytes);
  std::uint64_t name = 0u, detail = 0u;
  CHECK(import_memory(backend, memfd.fd(), memory_bytes, av::SharedImageShape(), name, detail));
  const unsigned first = make_texture(backend);
  const unsigned second = make_texture(backend);
  CHECK(store_in_memory(backend, first, side, name, 0u));
  CHECK(store_in_memory(backend, second, side, name, memory_bytes / 2u));
  CHECK_EQ(stub_calls, 2u);
  CHECK_EQ(OpenGlBackendProbe::memories(backend).textured(), 2u);
  backend.release_object(AV_SHARED_MEMORY, name, detail);
  backend.end_batch();
  CHECK(backend.drain(av::GenerousBudget().budget()));
  CHECK(memory_object_exists(static_cast<unsigned>(name)));
  CHECK(delete_texture(backend, first));
  backend.end_batch();
  CHECK(backend.drain(av::GenerousBudget().budget()));
  CHECK(memory_object_exists(static_cast<unsigned>(name)));
  CHECK(delete_texture(backend, second));
  CHECK(Command(av::glproto::kCmd_glFlush).run(backend));
  backend.end_batch();
  CHECK(backend.drain(av::GenerousBudget().budget()));
  CHECK(!memory_object_exists(static_cast<unsigned>(name)));
  CHECK_EQ(OpenGlBackendProbe::memories(backend).textured(), 0u);
  backend.shut_down(); }

TEST(opengl_backend_software, a_texture_that_does_not_fit_the_real_size_of_the_memory_is_an_invalid_value)
{ if (skipped()) return;
  OpenGlBackend backend(executor_log.opengl());
  CHECK(backend.initialize(software().display(), nullptr, nullptr));
  use_plain_storage(backend);
  Memfd         memfd(side * side * 4u);
  std::uint64_t name = 0u, detail = 0u;
  CHECK(import_memory(backend, memfd.fd(), memory_bytes, av::SharedImageShape(), name, detail));
  const unsigned texture = make_texture(backend);
  CHECK(store_in_memory(backend, texture, side * 2u, name, 0u));
  CHECK_EQ(stub_calls, 0u);
  CHECK_EQ(OpenGlBackendProbe::errors(backend, 1u), 1u);
  CHECK(store_in_memory(backend, texture, side, name, 0u));
  CHECK_EQ(stub_calls, 1u);
  CHECK_EQ(OpenGlBackendProbe::memories(backend).textured(), 1u);
  backend.shut_down(); }

TEST(opengl_backend_software, an_error_recorded_by_the_decoder_lands_in_the_published_error_word)
{ OpenGlBackend backend(executor_log.opengl());
  CHECK_EQ(OpenGlBackendProbe::errors(backend, 1u), 0u);
  backend.record_error(0x0501u);
  backend.record_error(0x0501u);
  CHECK_EQ(OpenGlBackendProbe::errors(backend, 1u), 2u);
  CHECK_EQ(OpenGlBackendProbe::errors(backend, 2u), 0u); }

TEST(opengl_backend_software, a_negative_count_reaches_the_gl_error_state_and_the_context_keeps_decoding)
{ if (skipped()) return;
  OpenGlBackend backend(executor_log.opengl());
  CHECK(backend.initialize(software().display(), nullptr, nullptr));
  CHECK(Command(av::glproto::kCmd_glGenTextures).varint(static_cast<std::uint32_t>(-1)).pod8(1u).varint(4u).run(backend));
  CHECK(!backend.lost());
  backend.end_batch();
  CHECK_EQ(OpenGlBackendProbe::errors(backend, 1u), 1u);
  CHECK(Command(av::glproto::kCmd_glGenTextures).varint(1u).pod8(1u).varint(4u).run(backend));
  CHECK(Command(av::glproto::kCmd_glFlush).run(backend));
  backend.shut_down(); }

namespace {

class Udmabuf {
public:
    Udmabuf(std::uint64_t bytes) : memfd_(-1), fd_(-1), bytes_(bytes), mapped_(nullptr)
    {
        const int device = open("/dev/udmabuf", O_RDWR | O_CLOEXEC);
        if (device < 0) return;
        memfd_ = memfd_create("av-udmabuf", MFD_ALLOW_SEALING | MFD_CLOEXEC);
        if (memfd_ >= 0 && ftruncate(memfd_, static_cast<off_t>(bytes)) == 0 &&
            fcntl(memfd_, F_ADD_SEALS, F_SEAL_SHRINK) == 0) {
            udmabuf_create create = {};
            create.memfd = static_cast<std::uint32_t>(memfd_);
            create.size = bytes;
            fd_ = ioctl(device, UDMABUF_CREATE, &create);
        }
        close(device);
        if (fd_ >= 0) {
            void *const map = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
            mapped_ = map == MAP_FAILED ? nullptr : static_cast<std::uint32_t *>(map);
        }
    }
    ~Udmabuf()
    {
        if (mapped_ != nullptr) munmap(mapped_, bytes_);
        if (fd_ >= 0) close(fd_);
        if (memfd_ >= 0) close(memfd_);
    }
    Udmabuf(const Udmabuf &) = delete;
    Udmabuf &operator=(const Udmabuf &) = delete;

    bool           made() const { return mapped_ != nullptr; }
    int            fd() const { return fd_; }
    std::uint32_t *pixels() const { return mapped_; }

    void let_go()
    {
        if (mapped_ != nullptr) munmap(mapped_, bytes_);
        if (fd_ >= 0) close(fd_);
        if (memfd_ >= 0) close(memfd_);
        mapped_ = nullptr;
        fd_ = -1;
        memfd_ = -1;
    }

private:
    int            memfd_;
    int            fd_;
    std::uint64_t  bytes_;
    std::uint32_t *mapped_;
};

}

TEST(opengl_backend_software, a_linear_dma_buf_is_placed_under_a_texture_as_an_egl_image_with_its_offset_pitch_and_modifier)
{ if (skipped()) return;
  Udmabuf buffer(memory_bytes);
  if (!buffer.made()) {
      std::fprintf(stderr, "no udmabuf device: the EGL image path is not run\n");
      return;
  }
  for (std::uint32_t texel = 0u; texel < side * side; texel++) buffer.pixels()[texel] = 0xFF000000u | (texel * 0x010305u);
  OpenGlBackend backend(executor_log.opengl());
  CHECK(backend.initialize(software().display(), nullptr, nullptr));
  use_plain_storage(backend);
  std::uint64_t name = 0u, detail = 0u;
  const av::SharedImageShape shape(side, side, dxgi_r8g8b8a8_unorm, row_bytes);
  CHECK(import_memory(backend, buffer.fd(), memory_bytes, shape, name, detail));
  CHECK_EQ(OpenGlBackendProbe::images(backend).count(), 1u);
  const unsigned texture = make_texture(backend);
  CHECK(store_in_memory(backend, texture, side, name, 0u));
  CHECK_EQ(stub_calls, 0u);
  CHECK_EQ(OpenGlBackendProbe::memories(backend).textured(), 1u);
  auto *const level = reinterpret_cast<GetTextureLevel>(eglGetProcAddress("glGetTextureLevelParameteriv"));
  int width = 0, format = 0;
  level(texture, 0, gl_texture_width, &width);
  level(texture, 0, gl_texture_internal_format, &format);
  CHECK_EQ(width, side);
  CHECK_EQ(format, gl_rgba8);
  std::uint32_t read[side * side] = {};
  auto *const get_image = reinterpret_cast<GetTextureImage>(eglGetProcAddress("glGetTextureImage"));
  get_image(texture, 0, gl_rgba, gl_unsigned_byte, sizeof read, read);
  CHECK_EQ(std::memcmp(read, buffer.pixels(), sizeof read), 0);
  const unsigned other = make_texture(backend);
  CHECK(store_in_memory(backend, other, side, name, 0u));
  CHECK_EQ(stub_calls, 0u);
  CHECK(store_in_memory(backend, texture, side, name, 0u));
  CHECK_EQ(stub_calls, 1u);
  CHECK_EQ(OpenGlBackendProbe::memories(backend).textured(), 2u);
  CHECK(delete_texture(backend, texture));
  CHECK(delete_texture(backend, other));
  backend.release_object(AV_SHARED_MEMORY, name, detail);
  CHECK_EQ(OpenGlBackendProbe::images(backend).count(), 1u);
  backend.end_batch();
  CHECK(backend.drain(av::GenerousBudget().budget()));
  CHECK_EQ(OpenGlBackendProbe::images(backend).count(), 0u);
  backend.shut_down(); }

TEST(opengl_backend_software, an_importer_keeps_the_pixels_of_a_dma_buf_after_the_exporter_has_let_every_descriptor_go)
{ if (skipped()) return;
  Udmabuf buffer(memory_bytes);
  if (!buffer.made()) return;
  std::uint32_t expected[side * side];
  for (std::uint32_t texel = 0u; texel < side * side; texel++) {
      expected[texel] = 0xFF000000u | (texel * 0x020401u);
      buffer.pixels()[texel] = expected[texel];
  }
  OpenGlBackend backend(executor_log.opengl());
  CHECK(backend.initialize(software().display(), nullptr, nullptr));
  std::uint64_t name = 0u, detail = 0u;
  CHECK(import_memory(backend, buffer.fd(), memory_bytes, av::SharedImageShape(side, side, dxgi_r8g8b8a8_unorm, row_bytes),
                      name, detail));
  const unsigned texture = make_texture(backend);
  CHECK(store_in_memory(backend, texture, side, name, 0u));
  buffer.let_go();
  std::uint32_t read[side * side] = {};
  auto *const get_image = reinterpret_cast<GetTextureImage>(eglGetProcAddress("glGetTextureImage"));
  get_image(texture, 0, gl_rgba, gl_unsigned_byte, sizeof read, read);
  CHECK_EQ(std::memcmp(read, expected, sizeof read), 0);
  backend.shut_down(); }

TEST(opengl_backend_software, a_memory_object_imported_by_one_session_is_stored_into_by_a_session_sharing_its_context)
{ if (skipped()) return;
  av::GlMemoryGroup group;
  OpenGlBackend     maker(executor_log.opengl());
  OpenGlBackend     user(executor_log.opengl());
  CHECK(maker.initialize(software().display(), nullptr, nullptr, &group));
  CHECK(user.initialize(software().display(), nullptr, &maker, &group));
  use_plain_storage(user);
  Memfd         memfd(memory_bytes);
  std::uint64_t name = 0u, detail = 0u;
  CHECK(import_memory(maker, memfd.fd(), memory_bytes, av::SharedImageShape(), name, detail));
  const unsigned texture = make_texture(user);
  CHECK(texture != 0u);
  CHECK(store_in_memory(user, texture, side, name, 0u));
  CHECK_EQ(stub_calls, 1u);
  CHECK_EQ(OpenGlBackendProbe::memories(maker).textured(), 1u);
  CHECK(OpenGlBackendProbe::memories(user).holds(static_cast<std::uint32_t>(name)));
  CHECK(delete_texture(user, texture));
  CHECK_EQ(OpenGlBackendProbe::memories(maker).textured(), 0u);
  user.shut_down();
  CHECK(OpenGlBackendProbe::memories(maker).holds(static_cast<std::uint32_t>(name)));
  maker.shut_down();
  CHECK_EQ(group.members(), 0u); }

TEST(opengl_backend_software, a_session_that_joined_no_group_cannot_store_into_another_session_s_memory)
{ if (skipped()) return;
  OpenGlBackend maker(executor_log.opengl());
  OpenGlBackend user(executor_log.opengl());
  CHECK(maker.initialize(software().display(), nullptr, nullptr));
  CHECK(user.initialize(software().display(), nullptr, &maker));
  use_plain_storage(user);
  Memfd         memfd(memory_bytes);
  std::uint64_t name = 0u, detail = 0u;
  CHECK(import_memory(maker, memfd.fd(), memory_bytes, av::SharedImageShape(), name, detail));
  const unsigned texture = make_texture(user);
  CHECK(store_in_memory(user, texture, side, name, 0u));
  CHECK_EQ(stub_calls, 0u);
  user.shut_down();
  maker.shut_down(); }

TEST(opengl_backend_software, a_row_pitch_shorter_than_a_row_makes_no_image_and_the_memory_is_still_imported)
{ if (skipped()) return;
  Udmabuf buffer(memory_bytes);
  if (!buffer.made()) return;
  OpenGlBackend backend(executor_log.opengl());
  CHECK(backend.initialize(software().display(), nullptr, nullptr));
  std::uint64_t name = 0u, detail = 0u;
  CHECK(import_memory(backend, buffer.fd(), memory_bytes,
                      av::SharedImageShape(side, side, dxgi_r8g8b8a8_unorm, row_bytes - 4u), name, detail));
  CHECK_EQ(OpenGlBackendProbe::images(backend).count(), 0u);
  CHECK(OpenGlBackendProbe::memories(backend).holds(static_cast<std::uint32_t>(name)));
  backend.shut_down(); }

TEST(opengl_backend_software, a_dma_buf_whose_shape_the_image_does_not_cover_goes_through_the_memory_object)
{ if (skipped()) return;
  Udmabuf buffer(memory_bytes);
  if (!buffer.made()) return;
  OpenGlBackend backend(executor_log.opengl());
  CHECK(backend.initialize(software().display(), nullptr, nullptr));
  use_plain_storage(backend);
  std::uint64_t name = 0u, detail = 0u;
  const av::SharedImageShape shape(side, side, dxgi_r8g8b8a8_unorm, row_bytes);
  CHECK(import_memory(backend, buffer.fd(), memory_bytes, shape, name, detail));
  CHECK_EQ(OpenGlBackendProbe::images(backend).count(), 1u);
  const unsigned texture = make_texture(backend);
  CHECK(store_in_memory(backend, texture, side / 2u, name, 0u));
  CHECK_EQ(stub_calls, 1u);
  const unsigned wide = make_texture(backend);
  CHECK(store_in_memory(backend, wide, side, side / 2u, name, 0u, gl_rgba8));
  CHECK_EQ(stub_calls, 2u);
  const unsigned tall = make_texture(backend);
  CHECK(store_in_memory(backend, tall, side / 2u, side, name, 0u, gl_rgba8));
  CHECK_EQ(stub_calls, 3u);
  const unsigned shifted = make_texture(backend);
  CHECK(store_in_memory(backend, shifted, side, name, 4096u));
  CHECK_EQ(stub_calls, 4u);
  const unsigned srgb = make_texture(backend);
  CHECK(store_in_memory(backend, srgb, side, name, 0u, gl_srgb8_alpha8));
  CHECK_EQ(stub_calls, 4u);
  const unsigned packed = make_texture(backend);
  CHECK(store_in_memory(backend, packed, side, name, 0u, gl_rgb565));
  CHECK_EQ(stub_calls, 5u);
  backend.shut_down(); }

namespace {

constexpr std::uint32_t pairs = threads / 2u;
constexpr std::uint32_t cycles_per_pair = iterations / pairs;
constexpr std::uint32_t memories_per_cycle = 6u;
constexpr std::uint32_t looks_per_import = 400u;

std::atomic<std::uint32_t> faults(0u);

void expect(bool condition, const char *what, std::uint32_t cycle)
{
    if (condition) return;
    if (faults.fetch_add(1u) == 0u) std::fprintf(stderr, "cycle %u: %s\n", cycle, what);
}

class Pair {
public:
    Pair() : first_(nullptr)
    {
        pthread_barrier_init(&gate_, nullptr, 2);
        for (auto &seat : published_)
            for (auto &name : seat) name.store(0u);
    }
    ~Pair() { pthread_barrier_destroy(&gate_); }
    Pair(const Pair &) = delete;
    Pair &operator=(const Pair &) = delete;

    void wait() { pthread_barrier_wait(&gate_); }

    OpenGlBackend                 *first_;
    std::atomic<std::uint64_t>     published_[2][memories_per_cycle];

private:
    pthread_barrier_t gate_;
};

struct Member {
    Pair         *pair;
    std::uint32_t seat;
    std::uint32_t number;
};

void run_cycle(Pair &pair, std::uint32_t seat, std::uint32_t number)
{
    const av::GlDisplay *const display = software().display();
    OpenGlBackend backend(executor_log.opengl());
    if (seat == 0u) {
        pair.first_ = &backend;
        expect(backend.initialize(display, nullptr, nullptr), "the first context opens", number);
    }
    pair.wait();
    if (seat == 1u) expect(backend.initialize(display, nullptr, pair.first_), "the sharing context opens", number);
    pair.wait();
    use_plain_storage(backend);
    unsigned textures[memories_per_cycle * (memories_per_cycle + 1u)] = {};
    std::uint32_t made = 0u;
    std::uint64_t mine[memories_per_cycle] = {};
    std::uint64_t details[memories_per_cycle] = {};
    const std::uint32_t other = 1u - seat;
    for (std::uint32_t at = 0u; at < memories_per_cycle; at++) {
        Memfd memfd(memory_bytes);
        expect(import_memory(backend, memfd.fd(), memory_bytes, av::SharedImageShape(), mine[at], details[at]),
               "memory imports", number);
        pair.published_[seat][at].store(mine[at]);
        for (std::uint32_t look = 0u; look < looks_per_import; look++)
            for (std::uint32_t seen = 0u; seen <= at; seen++) {
                const std::uint64_t theirs = pair.published_[other][seen].load();
                if (theirs != 0u) static_cast<void>(OpenGlBackendProbe::memories(backend).holds(static_cast<std::uint32_t>(theirs)));
            }
        const unsigned own = make_texture(backend);
        textures[made++] = own;
        expect(store_in_memory(backend, own, side, mine[at], 0u), "a texture is placed in an own memory", number);
        for (std::uint32_t seen = 0u; seen <= at; seen++) {
            const std::uint64_t theirs = pair.published_[other][seen].load();
            if (theirs == 0u) continue;
            const unsigned across = make_texture(backend);
            textures[made++] = across;
            static_cast<void>(store_in_memory(backend, across, side, theirs, memory_bytes / 2u));
        }
    }
    pair.wait();
    for (std::uint32_t at = 0u; at < memories_per_cycle; at++) {
        backend.release_object(AV_SHARED_MEMORY, mine[at], details[at]);
        backend.end_batch();
        static_cast<void>(backend.drain(av::GenerousBudget().budget()));
    }
    pair.wait();
    for (std::uint32_t at = 0u; at < made; at++) static_cast<void>(delete_texture(backend, textures[at]));
    expect(Command(av::glproto::kCmd_glFlush).run(backend), "flush", number);
    backend.end_batch();
    static_cast<void>(backend.drain(av::GenerousBudget().budget()));
    pair.wait();
    for (std::uint32_t round = 0u; round < 2u; round++) {
        backend.end_batch();
        expect(backend.drain(av::GenerousBudget().budget()), "the final drain finishes", number);
    }
    expect(OpenGlBackendProbe::memories(backend).count() == 0u, "every memory of the pair is gone", number);
    expect(OpenGlBackendProbe::memories(backend).invariants_hold(), "the tables are sound", number);
    for (std::uint32_t at = 0u; at < memories_per_cycle; at++)
        expect(!memory_object_exists(static_cast<unsigned>(mine[at])), "a retired memory object is deleted", number);
    pair.wait();
    if (seat == 1u) backend.shut_down();
    pair.wait();
    if (seat == 0u) backend.shut_down();
    pair.wait();
}

void *work(void *argument)
{
    const Member &member = *static_cast<Member *>(argument);
    for (std::uint32_t number = 0u; number < cycles_per_pair; number++)
        run_cycle(*member.pair, member.seat, member.number * cycles_per_pair + number);
    return nullptr;
}

}

TEST(opengl_backend_software, open_import_place_retire_drain_and_close_cycles_over_four_pairs_of_threads_sharing_a_context_leave_no_memory_object_under_a_texture)
{ if (skipped()) return;
  faults = 0u;
  static Pair rigs[pairs];
  Member members[threads];
  pthread_t workers[threads];
  for (std::uint32_t at = 0u; at < threads; at++) {
      members[at] = {&rigs[at / 2u], at % 2u, at / 2u};
      CHECK_EQ(pthread_create(&workers[at], nullptr, work, &members[at]), 0);
  }
  for (pthread_t &worker : workers) CHECK_EQ(pthread_join(worker, nullptr), 0);
  CHECK_EQ(faults.load(), 0u); }

namespace {

GLsync (*real_fence)(GLenum, GLbitfield) = nullptr;
void (*real_flush)(void) = nullptr;
unsigned fences_made = 0u;
unsigned flushes_after_fence = 0u;
bool     fenced_since_flush = false;

GLsync counted_fence(GLenum condition, GLbitfield flags)
{
    fences_made++;
    fenced_since_flush = true;
    return real_fence(condition, flags);
}

void counted_flush()
{
    if (fenced_since_flush) flushes_after_fence++;
    fenced_since_flush = false;
    real_flush();
}

void count_fences(OpenGlBackend &backend)
{
    av::glproto::RealGl &gl = OpenGlBackendProbe::writable(backend);
    real_wait = gl.glClientWaitSync;
    real_fence = gl.glFenceSync;
    real_flush = gl.glFlush;
    fences_made = 0u;
    flushes_after_fence = 0u;
    fenced_since_flush = false;
    waits = 0u;
    wait_answer = 0u;
    gl.glClientWaitSync = counted_wait;
    gl.glFenceSync = counted_fence;
    gl.glFlush = counted_flush;
}

}

TEST(opengl_backend_software, a_drain_that_timed_out_is_retried_on_the_same_fence_and_a_new_one_is_made_only_after_new_work)
{ if (skipped()) return;
  OpenGlBackend backend(executor_log.opengl());
  CHECK(backend.initialize(software().display(), nullptr, nullptr));
  count_fences(backend);
  av::MonotonicClock clock;
  wait_answer = 0x911Bu;
  av::TeardownBudget first(clock, 0u);
  CHECK(!backend.drain(first));
  CHECK_EQ(fences_made, 1u);
  CHECK_EQ(flushes_after_fence, 1u);
  av::TeardownBudget again(clock, 0u);
  CHECK(!backend.drain(again));
  CHECK_EQ(fences_made, 1u);
  wait_answer = 0u;
  av::TeardownBudget last(clock, 0u);
  CHECK(backend.drain(last));
  CHECK_EQ(fences_made, 1u);
  CHECK(Command(av::glproto::kCmd_glFlush).run(backend));
  wait_answer = 0x911Bu;
  av::TeardownBudget later(clock, 0u);
  CHECK(!backend.drain(later));
  CHECK_EQ(fences_made, 2u);
  CHECK(Command(av::glproto::kCmd_glFlush).run(backend));
  wait_answer = 0u;
  av::TeardownBudget fresh(clock, 0u);
  CHECK(backend.drain(fresh));
  CHECK_EQ(fences_made, 3u);
  backend.shut_down(); }

namespace {

struct Foreign {
    OpenGlBackend    *backend;
    std::atomic<bool> drained;
    std::atomic<bool> done;
};

void *drain_and_close(void *context)
{
    Foreign &foreign = *static_cast<Foreign *>(context);
    foreign.drained.store(foreign.backend->drain(av::GenerousBudget().budget()));
    foreign.backend->shut_down();
    foreign.done.store(true);
    return nullptr;
}

}

TEST(opengl_backend_software, a_backend_whose_context_cannot_be_made_current_is_not_drained_and_still_leaves_its_group)
{ if (skipped()) return;
  av::GlMemoryGroup group;
  OpenGlBackend backend(executor_log.opengl());
  CHECK(backend.initialize(software().display(), nullptr, nullptr, &group));
  Memfd         memfd(memory_bytes);
  std::uint64_t name = 0u, detail = 0u;
  CHECK(import_memory(backend, memfd.fd(), memory_bytes, av::SharedImageShape(), name, detail));
  CHECK_EQ(group.members(), 1u);
  CHECK_EQ(OpenGlBackendProbe::memories(backend).count(), 1u);
  OpenGlBackendProbe::gl(backend);
  Foreign foreign = {&backend, true, false};
  pthread_t thread;
  CHECK_EQ(pthread_create(&thread, nullptr, drain_and_close, &foreign), 0);
  CHECK_EQ(pthread_join(thread, nullptr), 0);
  CHECK(foreign.done.load());
  CHECK(!foreign.drained.load());
  CHECK_EQ(group.members(), 0u);
  CHECK_EQ(OpenGlBackendProbe::memories(backend).count(), 0u);
  eglMakeCurrent(eglGetCurrentDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT); }

TEST(opengl_backend_software, a_context_left_current_by_a_failed_drain_cannot_be_drained_from_another_thread)
{ if (skipped()) return;
  OpenGlBackend backend(executor_log.opengl());
  CHECK(backend.initialize(software().display(), nullptr, nullptr));
  real_wait = OpenGlBackendProbe::writable(backend).glClientWaitSync;
  wait_answer = 0x911Bu;
  OpenGlBackendProbe::writable(backend).glClientWaitSync = counted_wait;
  CHECK(!backend.drain(av::GenerousBudget().budget()));
  CHECK(eglGetCurrentContext() != EGL_NO_CONTEXT);
  wait_answer = 0u;
  Foreign foreign = {&backend, true, false};
  pthread_t thread;
  CHECK_EQ(pthread_create(&thread, nullptr, drain_and_close, &foreign), 0);
  CHECK_EQ(pthread_join(thread, nullptr), 0);
  CHECK(!foreign.drained.load());
  eglMakeCurrent(eglGetCurrentDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT); }

TEST(opengl_backend_software, a_context_let_go_of_after_a_failed_drain_is_drained_and_closed_by_another_thread)
{ if (skipped()) return;
  OpenGlBackend backend(executor_log.opengl());
  CHECK(backend.initialize(software().display(), nullptr, nullptr));
  real_wait = OpenGlBackendProbe::writable(backend).glClientWaitSync;
  wait_answer = 0x911Bu;
  OpenGlBackendProbe::writable(backend).glClientWaitSync = counted_wait;
  CHECK(!backend.drain(av::GenerousBudget().budget()));
  CHECK(eglGetCurrentContext() != EGL_NO_CONTEXT);
  backend.let_go_of_thread();
  CHECK(eglGetCurrentContext() == EGL_NO_CONTEXT);
  wait_answer = 0u;
  Foreign foreign = {&backend, false, false};
  pthread_t thread;
  CHECK_EQ(pthread_create(&thread, nullptr, drain_and_close, &foreign), 0);
  CHECK_EQ(pthread_join(thread, nullptr), 0);
  CHECK(foreign.done.load());
  CHECK(foreign.drained.load()); }

TEST(opengl_backend_software, letting_go_leaves_another_context_current_on_the_thread_alone)
{ if (skipped()) return;
  OpenGlBackend first(executor_log.opengl());
  OpenGlBackend second(executor_log.opengl());
  CHECK(first.initialize(software().display(), nullptr, nullptr));
  CHECK(second.initialize(software().display(), nullptr, nullptr));
  const EGLContext current = eglGetCurrentContext();
  CHECK(current != EGL_NO_CONTEXT);
  first.let_go_of_thread();
  CHECK(eglGetCurrentContext() == current);
  second.let_go_of_thread();
  CHECK(eglGetCurrentContext() == EGL_NO_CONTEXT);
  first.shut_down();
  second.shut_down(); }

namespace {

constexpr std::uint32_t gl_kill_steps = 12u;
constexpr std::uint32_t gl_kill_points = 20u;

struct GlKillScript {
    explicit GlKillScript(OpenGlBackend &backend) : backend_(backend), first_(memory_bytes), second_(memory_bytes) {}

    bool step(std::uint32_t index)
    {
        switch (index) {
        case 0u: return import_memory(backend_, first_.fd(), memory_bytes, av::SharedImageShape(), name_[0], detail_[0]);
        case 1u: texture_[0] = make_texture(backend_); return texture_[0] != 0u;
        case 2u: return store_in_memory(backend_, texture_[0], side, name_[0], 0u);
        case 3u: return Command(av::glproto::kCmd_glFlush).run(backend_);
        case 4u: backend_.end_batch(); return true;
        case 5u: return import_memory(backend_, second_.fd(), memory_bytes, av::SharedImageShape(), name_[1], detail_[1]);
        case 6u: texture_[1] = make_texture(backend_); return texture_[1] != 0u;
        case 7u: return store_in_memory(backend_, texture_[1], side, name_[1], 0u);
        case 8u: return delete_texture(backend_, texture_[0]);
        case 9u: backend_.release_object(AV_SHARED_MEMORY, name_[0], detail_[0]); return true;
        case 10u: return Command(av::glproto::kCmd_glFlush).run(backend_);
        default: backend_.end_batch(); return true;
        }
    }

private:
    OpenGlBackend &backend_;
    Memfd          first_, second_;
    std::uint64_t  name_[2] = {};
    std::uint64_t  detail_[2] = {};
    unsigned       texture_[2] = {};
};

}

TEST(opengl_backend_software, a_backend_killed_after_any_of_its_commands_drains_and_shuts_down_with_nothing_left_tracked)
{ if (skipped()) return;
  std::uint32_t seed = 0x2545F491u;
  for (std::uint32_t point = 0u; point < gl_kill_points; point++) {
      seed = seed * 1664525u + 1013904223u;
      const std::uint32_t killed_after = (seed >> 16u) % (gl_kill_steps + 1u);
      OpenGlBackend backend(executor_log.opengl());
      CHECK(backend.initialize(software().display(), nullptr, nullptr));
      {
          GlKillScript script(backend);
          for (std::uint32_t index = 0u; index < killed_after; index++) CHECK(script.step(index));
          CHECK(backend.drain(av::GenerousBudget().budget()));
          std::uint64_t marked = 0u, reached = 0u;
          backend.gpu_progress(marked, reached);
          CHECK_EQ(reached, marked);
          backend.shut_down();
      }
      CHECK_EQ(OpenGlBackendProbe::memories(backend).count(), 0u);
      CHECK_EQ(OpenGlBackendProbe::images(backend).count(), 0u);
      backend.shut_down();
  } }
