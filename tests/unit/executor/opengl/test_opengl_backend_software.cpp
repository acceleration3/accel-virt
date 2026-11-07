#include "executor/monotonic_clock.hpp"
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
constexpr unsigned gl_device_uuid = 0x9597u;
constexpr unsigned gl_texture_2d = 0x0DE1u;
constexpr unsigned gl_rgba8 = 0x8058u;
constexpr unsigned gl_srgb8_alpha8 = 0x8C43u;
constexpr unsigned gl_rgb565 = 0x8D62u;
constexpr unsigned gl_rgba = 0x1908u;
constexpr unsigned gl_unsigned_byte = 0x1401u;
constexpr unsigned gl_framebuffer = 0x8D40u;
constexpr unsigned gl_color_attachment0 = 0x8CE0u;
constexpr unsigned gl_color_buffer_bit = 0x4000u;
constexpr unsigned gl_texture_internal_format = 0x1003u;
constexpr unsigned gl_texture_width = 0x1000u;
constexpr unsigned dxgi_r8g8b8a8_unorm = 28u;
constexpr std::uint32_t uuid_bytes = 16u;
constexpr std::uint64_t memory_bytes = 1u << 20u;
constexpr std::uint32_t side = 64u;
constexpr std::uint32_t row_bytes = side * 4u;
constexpr std::uint32_t iterations = 256u;
constexpr std::uint32_t threads = 8u;

using GetString = const unsigned char *(*)(unsigned);
using GetUnsignedBytev = void (*)(unsigned, unsigned char *);
using IsMemoryObject = unsigned char (*)(unsigned);
using StorageTwoD = void (*)(unsigned, int, unsigned, int, int);
using GetTextureImage = void (*)(unsigned, int, unsigned, unsigned, int, void *);
using GetTextureLevel = void (*)(unsigned, int, unsigned, int *);

class SoftwareDisplay {
public:
    SoftwareDisplay() : display_(), uuid_{}, usable_(false) { probe(); }

    bool                 usable() const { return usable_; }
    const av::GlDisplay *display() const { return &display_; }

private:
    void probe()
    {
        if (eglGetProcAddress("eglQueryDevicesEXT") == nullptr) return;
        auto *const query = reinterpret_cast<PFNEGLQUERYDEVICESEXTPROC>(eglGetProcAddress("eglQueryDevicesEXT"));
        auto *const platform =
            reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
        auto *const device_string =
            reinterpret_cast<PFNEGLQUERYDEVICESTRINGEXTPROC>(eglGetProcAddress("eglQueryDeviceStringEXT"));
        EGLDeviceEXT devices[8];
        EGLint       found = 0;
        if (query == nullptr || platform == nullptr || device_string == nullptr ||
            query(8, devices, &found) != EGL_TRUE)
            return;
        for (EGLint index = 0; index < found && !usable_; index++) {
            const char *const extensions = device_string(devices[index], EGL_EXTENSIONS);
            if (extensions == nullptr || std::strstr(extensions, "EGL_MESA_device_software") == nullptr) continue;
            EGLDisplay display = platform(EGL_PLATFORM_DEVICE_EXT, devices[index], nullptr);
            if (display == EGL_NO_DISPLAY || eglInitialize(display, nullptr, nullptr) != EGL_TRUE) continue;
            eglBindAPI(EGL_OPENGL_API);
            EGLContext context = eglCreateContext(display, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, nullptr);
            if (context != EGL_NO_CONTEXT && eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context)) {
                auto *const get_string = reinterpret_cast<GetString>(eglGetProcAddress("glGetString"));
                auto *const unsigned_bytev =
                    reinterpret_cast<GetUnsignedBytev>(eglGetProcAddress("glGetUnsignedBytevEXT"));
                const char *const renderer = get_string != nullptr
                                                 ? reinterpret_cast<const char *>(get_string(gl_renderer))
                                                 : nullptr;
                if (renderer != nullptr && std::strstr(renderer, "llvmpipe") != nullptr && unsigned_bytev != nullptr) {
                    unsigned_bytev(gl_device_uuid, uuid_);
                    usable_ = true;
                }
            }
            eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
            eglTerminate(display);
        }
        usable_ = usable_ && display_.open(uuid_);
    }

    av::GlDisplay display_;
    std::uint8_t  uuid_[uuid_bytes];
    bool          usable_;
};

SoftwareDisplay &software()
{
    static SoftwareDisplay display;
    return display;
}

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
    stub_calls++;
    plain_storage(texture, levels, format, width, height);
}

void use_plain_storage(OpenGlBackend &backend)
{
    plain_storage = reinterpret_cast<StorageTwoD>(eglGetProcAddress("glTextureStorage2D"));
    stub_calls = 0u;
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

std::atomic<std::uint32_t> faults(0u);
std::atomic<std::uint32_t> next_cycle(0u);

void expect(bool condition, const char *what, std::uint32_t cycle)
{
    if (condition) return;
    if (faults.fetch_add(1u) == 0u) std::fprintf(stderr, "cycle %u: %s\n", cycle, what);
}

void cycle(const av::GlDisplay *display, std::uint32_t number)
{
    OpenGlBackend first(executor_log.opengl());
    OpenGlBackend second(executor_log.opengl());
    expect(first.initialize(display, nullptr, nullptr), "the first context opens", number);
    expect(second.initialize(display, nullptr, &first), "the sharing context opens", number);
    const av::glproto::RealGl &gl = OpenGlBackendProbe::gl(second);
    use_plain_storage(second);
    unsigned back_buffer = 0u, framebuffer = 0u;
    gl.glCreateTextures(gl_texture_2d, 1, &back_buffer);
    gl.glTextureStorage2D(back_buffer, 1, gl_rgba8, static_cast<int>(side), static_cast<int>(side));
    gl.glCreateFramebuffers(1, &framebuffer);
    gl.glNamedFramebufferTexture(framebuffer, gl_color_attachment0, back_buffer, 0);
    gl.glBindFramebuffer(gl_framebuffer, framebuffer);
    gl.glClearColor(0.25f, 0.5f, 0.75f, 1.0f);
    gl.glClear(gl_color_buffer_bit);
    Memfd         memfd(memory_bytes);
    std::uint64_t name = 0u, detail = 0u;
    expect(import_memory(second, memfd.fd(), memory_bytes, av::SharedImageShape(), name, detail), "memory imports",
           number);
    const unsigned left = make_texture(second);
    const unsigned right = make_texture(second);
    expect(store_in_memory(second, left, side, name, 0u), "the first texture is placed", number);
    expect(store_in_memory(second, right, side, name, memory_bytes / 2u), "the second texture is placed", number);
    second.release_object(AV_SHARED_MEMORY, name, detail);
    second.end_batch();
    CHECK(second.drain(av::GenerousBudget().budget()));
    expect(memory_object_exists(static_cast<unsigned>(name)), "memory kept for two textures", number);
    expect(delete_texture(second, left), "the first texture goes", number);
    second.end_batch();
    CHECK(second.drain(av::GenerousBudget().budget()));
    expect(memory_object_exists(static_cast<unsigned>(name)), "memory kept for the last texture", number);
    expect(delete_texture(second, right), "the second texture goes", number);
    expect(Command(av::glproto::kCmd_glFlush).run(second), "flush", number);
    second.end_batch();
    CHECK(second.drain(av::GenerousBudget().budget()));
    expect(!memory_object_exists(static_cast<unsigned>(name)), "memory freed after the last texture", number);
    second.shut_down();
    first.shut_down();
}

void *work(void *)
{
    for (std::uint32_t number = next_cycle.fetch_add(1u); number < iterations; number = next_cycle.fetch_add(1u))
        cycle(software().display(), number);
    return nullptr;
}

}

TEST(opengl_backend_software, two_hundred_fifty_six_open_draw_retire_drain_and_close_cycles_over_eight_threads_leave_no_memory_object_under_a_texture)
{ if (skipped()) return;
  faults = 0u;
  next_cycle = 0u;
  pthread_t workers[threads];
  for (pthread_t &worker : workers) CHECK_EQ(pthread_create(&worker, nullptr, work, nullptr), 0);
  for (pthread_t &worker : workers) CHECK_EQ(pthread_join(worker, nullptr), 0);
  CHECK_EQ(faults.load(), 0u);
  CHECK(next_cycle.load() >= iterations); }
