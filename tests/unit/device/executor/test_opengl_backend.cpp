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

using av::OpenGlBackend;
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
