/* One guest context's OpenGL on the host's own.
 *
 * This one can be judged without a GPU, and is: what is asserted is the
 * bookkeeping the generated table's hooks depend on - which fence a token
 * names, which bytes an offset resolves to, which ids this backend carries -
 * rather than whether a driver drew anything. A host with no EGL fails to open
 * a context and every command is refused, which is also asserted. */
#include "device/executor/imported_blob.hpp"
#include "device/executor/opengl_backend.hpp"
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
    return "leak:libEGL\n"
           "leak:libGLX\n"
           "leak:libnvidia\n"
           "leak:iris_dri\n"
           "leak:libdbus-1\n"
           "leak:libglapi\n"
           "leak:libz\n";
}

namespace {

std::uint8_t region[4096];
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
  backend.take_region(region, sizeof region);
  CHECK(backend.guest_bytes(0u, sizeof region) == region);
  CHECK(backend.guest_bytes(16u, 32u) == region + 16u);
  CHECK(backend.guest_bytes(0u, sizeof region + 1u) == nullptr);
  CHECK(backend.guest_bytes(~std::uint64_t{0}, 1u) == nullptr); }

/* A GPU nobody has serves no OpenGL, which is the answer the capset already
 * gave and this confirms rather than guesses at. */
TEST(opengl_backend, a_gpu_nobody_has_opens_no_context)
{ OpenGlBackend backend;
  CHECK(!backend.initialize(no_such_gpu));
  CHECK(!backend.initialize(nullptr));
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

/* The region a command reads from is a mapping the device made, not an object
 * this backend imported - so there is nothing here to adopt. */
TEST(opengl_backend, takes_no_blob_and_says_so)
{ OpenGlBackend backend;
  av::ImportedBlob blob;
  CHECK(!backend.adopt(blob));
  backend.abandon(blob);
  CHECK_EQ(blob.object(), 0u); }
