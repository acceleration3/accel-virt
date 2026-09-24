/* One guest context's OpenGL on the host's own.
 *
 * This one can be judged without a GPU, and is: what is asserted is the
 * bookkeeping the generated table's hooks depend on - which fence a token
 * names, which bytes an offset resolves to, which ids this backend carries -
 * rather than whether a driver drew anything. A host with no EGL fails to open
 * a context and every command is refused, which is also asserted. */
#include "device/executor/imported_blob.hpp"
#include "core/support/status.hpp"
#include "device/abi/rvk_executor.h"
#include "device/executor/opengl_backend.hpp"
#include "device/shim/gl_display.hpp"
#include "device/shim/loader_host_apis.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/opengl/gl_protocol_ids.h"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>

namespace av {

/* The backend's table and what it asks of the context it opened, reached with
 * no context: a table of this test's own entry points stands in for the
 * driver's. */
struct OpenGlBackendProbe {
    static glproto::RealGl &gl(OpenGlBackend &backend) { return backend.gl_; }
    static bool granted_fixed_function(OpenGlBackend &backend) { return backend.granted_fixed_function(); }
};

}  // namespace av

using av::OpenGlBackend;
using av::OpenGlBackendProbe;
using av::WireArena;
using av::WireReader;
using av::WireWriter;

/* Opening a context asks the host's real EGL which devices it has, and the
 * loaders behind that allocate things they never give back. Named rather than
 * leak detection being turned off. */
extern "C" const char *__lsan_default_suppressions(void);
extern "C" const char *__lsan_default_suppressions(void)
{
    /* The host's enumeration asks OpenCL as well as EGL which GPU is which. */
    return "leak:libOpenCL\n"
           "leak:intelocl\n"
           "leak:libigdrcl\n"
           "leak:libvulkan\n"
           "leak:libEGL\n"
           "leak:libGLX\n"
           "leak:libnvidia\n"
           "leak:iris_dri\n"
           "leak:libdbus-1\n"
           "leak:libglapi\n"
           "leak:libz\n";
}

namespace {

alignas(4096) std::uint8_t region[2u * 4096u];
alignas(4096) std::uint8_t second_region[4096];

RvkBlobHandle host_pointer(std::uint8_t *base, std::uint64_t bytes)
{
    RvkBlobHandle handle = {};
    handle.kind = RVK_BLOB_HOST_POINTER;
    handle.pointer = base;
    handle.fd = -1;
    handle.size = bytes;
    return handle;
}
int          fence = 0;
/* What the stand-in driver answers: the profile mask, and how many errors
 * glGetError has left to report (all of them GL_INVALID_ENUM). */
constexpr GLenum  gl_context_profile_mask = 0x9126u, gl_invalid_enum = 0x0500u;
constexpr GLint   core_profile_bit = 0x1, compatibility_profile_bit = 0x2;
constexpr std::uint32_t endless = 1000u;
GLint             answered_mask = 0;
std::uint32_t     errors_left = 0u;
std::uint32_t     errors_asked = 0u;

void profile_integers(GLenum pname, GLint *value)
{
    if (pname == gl_context_profile_mask) *value = answered_mask;
}

GLenum pending_error()
{
    errors_asked++;
    if (errors_left == 0u) return 0u;
    errors_left--;
    return gl_invalid_enum;
}

/* A backend whose table answers the mask and the errors given. */
bool fixed_function_for(GLint mask, std::uint32_t errors)
{
    OpenGlBackend backend;
    av::glproto::RealGl &gl = OpenGlBackendProbe::gl(backend);
    gl.glGetIntegerv = profile_integers;
    gl.glGetError = pending_error;
    answered_mask = mask;
    errors_left = errors;
    errors_asked = 0u;
    return OpenGlBackendProbe::granted_fixed_function(backend);
}

constexpr std::uint8_t no_such_gpu[16] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                          0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

}  // namespace

TEST(opengl_backend, knows_the_ids_the_profile_carries_and_no_others)
{ OpenGlBackend backend;
  CHECK(backend.knows(av::glproto::kCmd_glClear));
  /* Inside OpenGL's range and not carried. */
  CHECK(!backend.knows(0x3FFFFu));
  /* The capset calls OpenGL bit 2 and its commands carry API 3, so an id
   * built from the capset bit is one the device would never send. */
  CHECK(!backend.knows(0x2FFFFu));
  CHECK(!backend.knows(0u)); }

/* A token the host never bound names nothing, which is what the decoder asks
 * before it hands a fence to the driver. */
TEST(opengl_backend, a_fence_is_found_only_by_the_token_it_was_bound_to)
{ OpenGlBackend backend;
  CHECK(backend.fence_for(7u) == nullptr);
  CHECK(backend.bind_fence(7u, &fence));
  CHECK(backend.fence_for(7u) == &fence);
  CHECK(backend.fence_for(8u) == nullptr);
  backend.forget_fence(7u);
  CHECK(backend.fence_for(7u) == nullptr); }

/* The resolver the thousand-odd bulk-data sites call. Every offset and length
 * it is asked for came off the wire. */
TEST(opengl_backend, guest_bytes_answers_inside_the_region_and_nowhere_else)
{ OpenGlBackend backend;
  CHECK(backend.guest_bytes(0u, 1u) == nullptr);
  av::ImportedBlob blob;
  CHECK_STATUS(blob.take(1u, host_pointer(region, sizeof region)), av::Status::ok);
  CHECK(backend.adopt(blob));
  CHECK(backend.guest_bytes(0u, sizeof region) == region);
  CHECK(backend.guest_bytes(16u, 32u) == region + 16u);
  CHECK(backend.guest_bytes(0u, sizeof region + 1u) == nullptr);
  CHECK(backend.guest_bytes(~std::uint64_t{0}, 1u) == nullptr); }

/* A GPU the host kept no display for serves no OpenGL, which is the answer the
 * capset already gave and this confirms rather than guesses at; nor does a
 * display nobody opened, and no GPU nobody has gets a display. */
TEST(opengl_backend, a_gpu_with_no_display_opens_no_context)
{ OpenGlBackend backend;
  CHECK(!backend.initialize(nullptr, nullptr, nullptr));
  av::GlDisplay display;
  CHECK(!display.open(no_such_gpu));
  CHECK(!display.open());
  CHECK(!display.open(nullptr));
  CHECK(!backend.initialize(&display, nullptr, nullptr));
  CHECK_EQ(backend.resolved(), 0u); }

/* Only a core context lacks the fixed-function arrays a draw's check asks
 * after. A context from before profiles has no mask, and asking for one is an
 * error that is cleared before the guest can read it back; a context lost
 * answers an error forever, and is asked only as often as GL has flags. */
TEST(opengl_backend, only_a_core_context_is_told_it_has_no_fixed_function_arrays)
{ CHECK(!fixed_function_for(core_profile_bit, 0u));
  CHECK_EQ(errors_asked, 1u);
  CHECK(fixed_function_for(compatibility_profile_bit, 0u));
  CHECK(fixed_function_for(0, 1u));
  CHECK_EQ(errors_left, 0u);
  CHECK_EQ(errors_asked, 2u);
  CHECK(!fixed_function_for(core_profile_bit | compatibility_profile_bit, endless));
  CHECK_EQ(errors_asked, 8u);
  OpenGlBackend unloaded;
  CHECK(OpenGlBackendProbe::granted_fixed_function(unloaded));
  OpenGlBackendProbe::gl(unloaded).glGetIntegerv = profile_integers;
  CHECK(OpenGlBackendProbe::granted_fixed_function(unloaded)); }

/* Without a context every command is refused rather than run against a table
 * of null entry points. */
TEST(opengl_backend, without_a_context_nothing_runs)
{ OpenGlBackend backend;
  std::uint8_t block[4096];
  WireArena arena(block, sizeof block);
  std::uint8_t frame[16] = {};
  std::uint8_t reply[16] = {};
  WireReader payload(frame, sizeof frame);
  WireWriter answer(reply, sizeof reply);
  CHECK(!backend.run(av::glproto::kCmd_glClear, payload, answer, arena));
  CHECK_EQ(backend.ran(), 0u);
  CHECK(!backend.lost());
  backend.shut_down();
  CHECK_EQ(backend.resolved(), 0u); }

/* The region a command reads from is the guest chunk the context imported
 * (feature 050): one host pointer at a time, named by a handle that is not 0,
 * and given back when the import is. A dma-buf is not a region. */
TEST(opengl_backend, adopts_one_host_pointer_as_its_region)
{ OpenGlBackend backend;
  av::ImportedBlob first;
  CHECK_STATUS(first.take(1u, host_pointer(region, sizeof region)), av::Status::ok);
  CHECK(backend.adopt(first));
  CHECK(first.wire_handle() != 0u);
  CHECK(first.object() != 0u);
  CHECK(backend.guest_bytes(4096u, 4096u) == region + 4096u);
  CHECK(backend.guest_bytes(4096u, 4097u) == nullptr);
  av::ImportedBlob second;
  CHECK_STATUS(second.take(2u, host_pointer(second_region, sizeof second_region)), av::Status::ok);
  CHECK(!backend.adopt(second));
  CHECK_EQ(second.wire_handle(), 0u);
  /* Giving back an import that is not the region leaves the region. */
  backend.abandon(second);
  CHECK(backend.guest_bytes(0u, 1u) == region);
  backend.abandon(first);
  CHECK_EQ(first.wire_handle(), 0u);
  CHECK_EQ(first.object(), 0u);
  CHECK(backend.guest_bytes(0u, 1u) == nullptr);
  CHECK(backend.adopt(second));
  CHECK(backend.guest_bytes(0u, 4096u) == second_region);
  backend.shut_down();
  CHECK(backend.guest_bytes(0u, 1u) == nullptr);
  av::ImportedBlob shared;
  RvkBlobHandle dma = {};
  dma.kind = RVK_BLOB_DMA_BUF;
  dma.fd = 7;
  dma.size = 4096u;
  CHECK_STATUS(shared.take(3u, dma), av::Status::ok);
  CHECK(!backend.adopt(shared));
  CHECK(backend.guest_bytes(0u, 1u) == nullptr); }

/* A real context on a real GPU, found from this host's own enumeration rather
 * than a UUID written into the test. A host with no GPU that runs OpenGL has
 * nothing below this line to judge, which is said rather than faked. */
TEST(opengl_backend, a_context_on_a_real_gpu_runs_a_command_and_refuses_a_broken_one)
{ av::LoaderHostApis host;
  host.open();
  const av::GlDisplay *display = nullptr;
  for (std::uint32_t i = 0u; i < host.gpu_count() && display == nullptr; i++) {
      display = host.gl_display(i);
      /* The host keeps a display exactly where it says OpenGL runs. */
      CHECK_EQ(display != nullptr, (host.sub_apis(i) & RVK_EXECUTOR_SUB_API_OPENGL) != 0u);
  }
  CHECK(host.gl_display(host.gpu_count()) == nullptr);
  if (display == nullptr) return;

  OpenGlBackend backend;
  CHECK(backend.initialize(display, nullptr, nullptr));
  CHECK(backend.resolved() > 0u);
  /* A second context on the same display, which outlives the first. */
  OpenGlBackend other;
  CHECK(other.initialize(display, nullptr, nullptr));

  std::uint8_t block[4096];
  WireArena arena(block, sizeof block);
  /* OpenGL's wire carries scalars as varints, so GL_COLOR_BUFFER_BIT is three
   * bytes here rather than four - and a frame written as a raw u32 would have
   * decoded, from its first byte alone, as glClear(0). */
  std::uint8_t frame[16];
  WireWriter clear(frame, sizeof frame);
  clear.write_varint<std::uint32_t>(0x4000u);
  CHECK_EQ(clear.offset(), 3u);
  std::uint8_t reply[16] = {};
  WireReader payload(frame, clear.offset());
  WireWriter answer(reply, sizeof reply);
  CHECK(backend.run(av::glproto::kCmd_glClear, payload, answer, arena));
  CHECK_EQ(backend.ran(), 1u);

  /* A varint cut before its last byte is refused by the decoder even with a
   * context current, and does not count as having run. */
  arena.reset();
  WireReader short_payload(frame, 1u);
  WireWriter short_answer(reply, sizeof reply);
  CHECK(!backend.run(av::glproto::kCmd_glClear, short_payload, short_answer, arena));
  CHECK_EQ(backend.ran(), 1u);

  backend.shut_down();
  CHECK_EQ(backend.resolved(), 0u);
  /* And after shutting down the context is gone, so nothing runs. */
  arena.reset();
  WireReader again(frame, clear.offset());
  WireWriter again_answer(reply, sizeof reply);
  CHECK(!backend.run(av::glproto::kCmd_glClear, again, again_answer, arena));
  /* The other context's display is still open, and so it still runs. */
  arena.reset();
  WireReader still(frame, clear.offset());
  WireWriter still_answer(reply, sizeof reply);
  CHECK(other.run(av::glproto::kCmd_glClear, still, still_answer, arena));
  CHECK(display->open());
  other.shut_down();
  host.close();
  CHECK(!display->open()); }

/* Feature 053. Nothing in OpenGL hands out a descriptor, so the export is a
 * refusal on every host; the import needs a driver, and a backend that never
 * opened a context has none, which is what this can judge without a GPU. */
TEST(opengl_backend, a_backend_with_no_context_exports_imports_and_releases_nothing)
{ OpenGlBackend backend;
  std::uint32_t handle_kind = 9u;
  int           fd = 9;
  CHECK(!backend.export_object(RVK_SHARED_MEMORY, 1u, handle_kind, fd));
  CHECK_EQ(handle_kind, 0u);
  CHECK_EQ(fd, -1);
  std::uint64_t made = 9u;
  CHECK(!backend.import_object(RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, 0, 4096u, made));
  CHECK_EQ(made, 0u);
  backend.release_object(RVK_SHARED_MEMORY, 1u);
  backend.shut_down(); }

/* What is refused before any driver is asked, which is every question the
 * wire's answer depends on: a descriptor of the wrong kind, no descriptor at
 * all, a kind this protocol does not have, and memory of no size. */
TEST(opengl_backend, an_import_of_the_wrong_shape_never_reaches_a_driver)
{ OpenGlBackend backend;
  std::uint64_t made = 9u;
  CHECK(!backend.import_object(RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_OPAQUE_FD, 0, 4096u, made));
  CHECK_EQ(made, 0u);
  CHECK(!backend.import_object(RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, -1, 4096u, made));
  CHECK(!backend.import_object(RVK_SHARED_NATIVE_FENCE + 1u, RVK_SHARED_HANDLE_DMA_BUF, 0, 4096u, made));
  CHECK(!backend.import_object(0u, RVK_SHARED_HANDLE_DMA_BUF, 0, 4096u, made));
  CHECK(!backend.import_object(RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, 0, 0u, made));
  CHECK_EQ(made, 0u); }
