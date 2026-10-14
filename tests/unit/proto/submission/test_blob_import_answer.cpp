/* IMPORT_BLOB's 16-byte answer: an import always names its device memory, and
 * a refusal never does, so the ICD cannot mistake one for the other. */
#include "core/support/status.hpp"
#include "harness/framework/byte_layout.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/blob_import_answer.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::BlobImportAnswer;
using av::ByteLayout;
using av::GuardedBuffer;
using av::Status;

namespace {

bool rejects(const std::uint8_t *bytes, std::size_t length)
{
    BlobImportAnswer answer;
    const Status     status = answer.decode(bytes, length);
    if (status == Status::ok) {
        std::uint8_t again[BlobImportAnswer::encoded_size] = {};
        if (answer.encode(again, sizeof again) != Status::ok || std::memcmp(again, bytes, sizeof again) != 0)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "accepted input did not round trip");
    }
    return status != Status::ok;
}

}  // namespace

TEST(blob_import_answer_encode, lays_out_the_status_and_the_handle)
{ std::uint8_t bytes[BlobImportAnswer::encoded_size];
  std::memset(bytes, 0xEE, sizeof bytes);
  CHECK_STATUS(BlobImportAnswer(BlobImportAnswer::imported, 0x4000000000000007u).encode(bytes, sizeof bytes),
               Status::ok);
  const ByteLayout layout(bytes, sizeof bytes);
  CHECK(layout.at(0u, 4u) == 0u);
  CHECK(layout.at(4u, 4u) == 0u);
  CHECK(layout.at(8u, 8u) == 0x4000000000000007u); }

TEST(blob_import_answer_encode, a_short_destination_is_too_large_and_untouched)
{ GuardedBuffer buffer(BlobImportAnswer::encoded_size - 1u);
  CHECK_STATUS(BlobImportAnswer(BlobImportAnswer::no_room, 0u).encode(buffer.data(), buffer.size()), Status::too_large);
  CHECK(buffer.guards_intact()); }

TEST(blob_import_answer_decode, every_refusal_round_trips_without_a_handle)
{ CHECK_EQ(BlobImportAnswer::refused_request, 1u);
  CHECK_EQ(BlobImportAnswer::no_room, 4u);
  for (std::uint32_t status = BlobImportAnswer::refused_request; status <= BlobImportAnswer::no_room; status++) {
      std::uint8_t bytes[BlobImportAnswer::encoded_size] = {};
      CHECK_STATUS(BlobImportAnswer(status, 0u).encode(bytes, sizeof bytes), Status::ok);
      BlobImportAnswer decoded;
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::ok);
      CHECK_EQ(decoded.status(), status);
      CHECK_EQ(decoded.device_memory(), 0u);
  } }

TEST(blob_import_answer_decode, an_import_without_a_handle_a_refusal_with_one_or_an_unknown_status_is_malformed)
{ const BlobImportAnswer cases[] = {
      BlobImportAnswer(BlobImportAnswer::imported, 0u),
      BlobImportAnswer(BlobImportAnswer::executor_refused, 1u),
      BlobImportAnswer(BlobImportAnswer::no_room + 1u, 0u),
      BlobImportAnswer(0xFFFFFFFFu, 0u),
  };
  for (const BlobImportAnswer &c : cases) {
      std::uint8_t bytes[BlobImportAnswer::encoded_size] = {};
      CHECK_STATUS(c.encode(bytes, sizeof bytes), Status::ok);
      BlobImportAnswer decoded(BlobImportAnswer::imported, 42u);
      CHECK_STATUS(decoded.decode(bytes, sizeof bytes), Status::malformed);
      CHECK_EQ(decoded.device_memory(), 42u);
  }
  std::uint8_t reserved[BlobImportAnswer::encoded_size] = {};
  CHECK_STATUS(BlobImportAnswer(BlobImportAnswer::imported, 42u).encode(reserved, sizeof reserved), Status::ok);
  reserved[7] = 1u;
  BlobImportAnswer decoded;
  CHECK_STATUS(decoded.decode(reserved, sizeof reserved), Status::malformed);
  CHECK_STATUS(decoded.decode(reserved, 12u), Status::malformed); }

TEST(blob_import_answer_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[BlobImportAnswer::encoded_size] = {};
  CHECK_STATUS(BlobImportAnswer(BlobImportAnswer::imported, 0x4000000000000001u).encode(bytes, sizeof bytes),
               Status::ok);
  const av::HostileSweep sweep(rejects, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > 128u);
  /* An import names its device memory, so the handle's 64 bits are free: the
   * handle has two bits set and no single flip can zero it. A flipped status
   * is a refusal still carrying a handle, and the reserved word must be zero.
   * Every shorter input is short of 16 bytes, all-zero is an import with no
   * handle, all-ones a status past no_room, and the untouched input is the
   * last acceptance. */
  const std::uint32_t acceptable = 64u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }
