/* One guest context's OpenCL on the host's own. As with Vulkan, the driver's
 * own answer is not asserted - these pass on a machine with no OpenCL at all,
 * where every command still decodes and the platform count is zero. */
#include "device/executor/imported_blob.hpp"
#include "device/executor/opencl_backend.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/opencl/cl_protocol_ids.h"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>

using av::OpenClBackend;
using av::WireArena;
using av::WireReader;
using av::WireWriter;

/* This test calls the real OpenCL ICD loader, which allocates a table of every
 * platform it finds and never gives it back. Named rather than leak detection
 * being turned off, so a leak of this repository's own still fails. */
extern "C" const char *__lsan_default_suppressions(void);
extern "C" const char *__lsan_default_suppressions(void)
{
    return "leak:libOpenCL\n"
           "leak:intelocl\n"
           "leak:libigdrcl\n"
           "leak:libnvidia\n"
           "leak:libdbus-1\n";
}

namespace {

constexpr std::size_t block_bytes = 65536u;

}  // namespace

TEST(opencl_backend, knows_the_ids_the_profile_carries_and_no_others)
{ OpenClBackend backend;
  CHECK(backend.knows(av::clproto::kCmd_clGetPlatformIDs));
  /* Inside OpenCL's range and not carried. */
  CHECK(!backend.knows(0x2FFFFu));
  /* Another API's range entirely, which the device should never deliver. */
  CHECK(!backend.knows(av::clproto::kCmd_clGetPlatformIDs ^ 0x30000u));
  CHECK(!backend.knows(0u)); }

TEST(opencl_backend, a_well_formed_command_decodes_and_answers)
{ OpenClBackend backend;
  std::uint8_t frame[64];
  WireWriter writer(frame, sizeof frame);
  /* clGetPlatformIDs(num_entries, platforms, num_platforms): ask for the
   * count only, which is the call every OpenCL program starts with. */
  writer.write_pod<std::uint32_t>(0u);
  writer.write_pod<std::uint8_t>(0u);
  writer.write_pod<std::uint8_t>(1u);

  std::uint8_t block[block_bytes];
  WireArena arena(block, sizeof block);
  std::uint8_t reply[64] = {};
  WireReader payload(frame, writer.offset());
  WireWriter answer(reply, sizeof reply);
  CHECK(backend.run(av::clproto::kCmd_clGetPlatformIDs, payload, answer, arena));
  CHECK_EQ(backend.ran(), 1u);
  CHECK(!backend.lost());
  backend.shut_down();
  CHECK_EQ(backend.ran(), 0u); }

TEST(opencl_backend, a_truncated_command_is_refused_at_every_length)
{ OpenClBackend backend;
  std::uint8_t frame[64];
  WireWriter writer(frame, sizeof frame);
  writer.write_pod<std::uint32_t>(0u);
  writer.write_pod<std::uint8_t>(0u);
  writer.write_pod<std::uint8_t>(1u);

  /* What the decoder actually reads, rather than what this test happened to
   * write: a frame longer than the command needs is a frame with trailing
   * bytes, which is the device's business and not the decoder's. */
  std::size_t consumed = 0u;
  { std::uint8_t block[block_bytes];
    WireArena arena(block, sizeof block);
    std::uint8_t reply[64] = {};
    WireReader payload(frame, writer.offset());
    WireWriter answer(reply, sizeof reply);
    CHECK(backend.run(av::clproto::kCmd_clGetPlatformIDs, payload, answer, arena));
    consumed = writer.offset() - payload.remaining(); }
  CHECK(consumed > 0u);

  for (std::size_t length = 0u; length < consumed; length++) {
      std::uint8_t block[block_bytes];
      WireArena arena(block, sizeof block);
      std::uint8_t reply[64] = {};
      WireReader payload(frame, length);
      WireWriter answer(reply, sizeof reply);
      CHECK(!backend.run(av::clproto::kCmd_clGetPlatformIDs, payload, answer, arena));
  }
  /* Only the one that decoded ran. */
  CHECK_EQ(backend.ran(), 1u);
  backend.shut_down(); }

/* Importing means clCreateBufferWithProperties with a dma-buf, and whether
 * that works is a per-device question answered where it is used. Until then a
 * blob is copied at the boundary, which is what import_kinds already says. */
TEST(opencl_backend, takes_no_blob_yet_and_says_so)
{ OpenClBackend backend;
  av::ImportedBlob blob;
  CHECK(!backend.adopt(blob));
  backend.abandon(blob);
  CHECK_EQ(blob.object(), 0u); }
