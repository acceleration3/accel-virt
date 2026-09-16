/* IMPORT_BLOB and RELEASE_BLOB as a process sends them (feature 045,
 * contracts/host_visible_memory.md §2), over MockTransport playing the host:
 * the request is the contract's 32 bytes, the answer's 16 are the host's word
 * and judged as hostile, and the device memory it names is released once. */
#include "core/support/status.hpp"
#include "guest/transport/blob_import.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/transport/mock_transport.hpp"
#include "proto/submission/blob_import_answer.hpp"
#include "proto/submission/blob_import_request.hpp"
#include "proto/submission/command_header.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace av {

struct BlobImportProbe {
    /* A refusal and a handle held at once: what judging an answer never leaves. */
    static void refused_and_held(BlobImport &import) { import.memory_ = 1u; import.host_status_ = 1u; }
};

}  // namespace av

using av::BlobImport;
using av::BlobImportAnswer;
using av::BlobImportProbe;
using av::BlobImportRequest;
using av::CommandHeader;
using av::MockTransport;
using av::Status;

namespace {

constexpr std::uint32_t chunk_resource = 0x00100003u;
constexpr std::uint64_t device_memory = 0x0000700000000042ull;
constexpr std::uint64_t one_mebibyte = 1u << 20u;

class Answer {
public:
    Answer(std::uint32_t status, std::uint32_t reserved, std::uint64_t memory) : bytes_{}
    {
        std::memcpy(bytes_, &status, sizeof status);
        std::memcpy(bytes_ + 4u, &reserved, sizeof reserved);
        std::memcpy(bytes_ + 8u, &memory, sizeof memory);
    }
    const std::uint8_t *bytes() const { return bytes_; }
    std::uint8_t       *bytes() { return bytes_; }

private:
    std::uint8_t bytes_[BlobImportAnswer::encoded_size + 1u];
};

std::uint32_t u32_at(const std::uint8_t *bytes, std::size_t at)
{
    std::uint32_t value = 0u;
    std::memcpy(&value, bytes + at, sizeof value);
    return value;
}

std::uint64_t u64_at(const std::uint8_t *bytes, std::size_t at)
{
    std::uint64_t value = 0u;
    std::memcpy(&value, bytes + at, sizeof value);
    return value;
}

}  // namespace

TEST(blob_import, import_sends_the_contracts_request_and_holds_the_hosts_memory)
{ MockTransport transport;
  BlobImport import(transport);
  const Answer taken(0u, 0u, device_memory);
  transport.set_reply(taken.bytes(), BlobImportAnswer::encoded_size);
  CHECK_EQ(import.memory(), 0u);
  CHECK_STATUS(import.import(chunk_resource, 4096u, one_mebibyte, 5u), Status::ok);
  CHECK(import.invariants_hold());
  CHECK_EQ(import.memory(), device_memory);
  CHECK_EQ(import.host_status(), 0u);
  CHECK_EQ(transport.end_calls(), 1u);
  CHECK_EQ(transport.last_command(), CommandHeader::import_blob_id);
  CHECK_EQ(transport.last_length(), BlobImportRequest::encoded_size);
  const std::uint8_t *const request = transport.last_payload();
  CHECK_EQ(u32_at(request, 0u), chunk_resource);
  CHECK_EQ(u32_at(request, 4u), 0u);
  CHECK_EQ(u64_at(request, 8u), 4096u);
  CHECK_EQ(u64_at(request, 16u), one_mebibyte);
  CHECK_EQ(u32_at(request, 24u), 5u);
  CHECK_EQ(u32_at(request, 28u), 0u);
  /* One import per object: a second is refused without a send. */
  CHECK_STATUS(import.import(chunk_resource, 0u, one_mebibyte, 5u), Status::invalid_state);
  CHECK_EQ(transport.sends(), 1u);
  transport.set_reply(taken.bytes(), 0u);
  CHECK_STATUS(import.release(), Status::ok);
  CHECK(import.invariants_hold());
  CHECK_EQ(transport.last_command(), CommandHeader::release_blob_id);
  CHECK_EQ(transport.end_calls(), 2u);
  CHECK_EQ(transport.last_length(), 8u);
  CHECK_EQ(u64_at(transport.last_payload(), 0u), device_memory);
  CHECK_EQ(import.memory(), 0u);
  /* Nothing held, nothing sent. */
  CHECK_STATUS(import.release(), Status::ok);
  CHECK_EQ(transport.sends(), 2u); }

/* A refusal is the host's status with no memory; the object can import again,
 * and a later import it takes clears the refusal. */
TEST(blob_import, a_refusal_keeps_the_hosts_status_and_holds_nothing)
{ MockTransport transport;
  BlobImport import(transport);
  const Answer refused(BlobImportAnswer::executor_refused, 0u, 0u);
  transport.set_reply(refused.bytes(), BlobImportAnswer::encoded_size);
  CHECK_STATUS(import.import(chunk_resource, 0u, one_mebibyte, 2u), Status::invalid_parameter);
  CHECK(import.invariants_hold());
  CHECK_EQ(import.host_status(), static_cast<std::uint32_t>(BlobImportAnswer::executor_refused));
  CHECK_EQ(import.memory(), 0u);
  CHECK_STATUS(import.release(), Status::ok);
  CHECK_EQ(transport.sends(), 1u);
  const Answer taken(0u, 0u, device_memory);
  transport.set_reply(taken.bytes(), BlobImportAnswer::encoded_size);
  CHECK_STATUS(import.import(chunk_resource, 0u, one_mebibyte, 3u), Status::ok);
  CHECK_EQ(import.host_status(), 0u);
  CHECK_EQ(import.memory(), device_memory); }

/* Every answer the contract does not allow, and a failed call, holds nothing;
 * an empty one is told apart as a device that does not know the command. */
TEST(blob_import, hostile_answers_hold_nothing)
{ struct Case { const char *what; std::uint32_t status; std::uint32_t reserved; std::uint64_t memory;
                std::size_t length; Status expected; };
  const Case cases[] = {
      {"empty", 0u, 0u, device_memory, 0u, Status::not_supported},
      {"cut in the status", 0u, 0u, device_memory, 3u, Status::malformed},
      {"cut after the status", 0u, 0u, device_memory, 4u, Status::malformed},
      {"cut after reserved", 0u, 0u, device_memory, 8u, Status::malformed},
      {"cut in the memory", 0u, 0u, device_memory, 15u, Status::malformed},
      {"one byte long", 0u, 0u, device_memory, 17u, Status::malformed},
      {"reserved set", 0u, 1u, device_memory, 16u, Status::malformed},
      {"reserved high bit", 0u, 0x80000000u, device_memory, 16u, Status::malformed},
      {"taken without memory", 0u, 0u, 0u, 16u, Status::malformed},
      {"refused with memory", 1u, 0u, device_memory, 16u, Status::malformed},
      {"refused with all ones", 0xFFFFFFFFu, 0u, ~std::uint64_t{0}, 16u, Status::malformed},
      {"a status past no room", BlobImportAnswer::no_room + 1u, 0u, 0u, 16u, Status::malformed},
  };
  for (const Case &one : cases) {
      MockTransport transport;
      BlobImport import(transport);
      const Answer answer(one.status, one.reserved, one.memory);
      transport.set_reply(answer.bytes(), one.length);
      CHECK_STATUS(import.import(chunk_resource, 0u, one_mebibyte, 0u), one.expected);
      CHECK_EQ(import.memory(), 0u);
      CHECK_EQ(import.host_status(), 0u);
      CHECK(import.invariants_hold());
  }
  MockTransport transport;
  BlobImport import(transport);
  transport.fail_next();
  CHECK_STATUS(import.import(chunk_resource, 0u, one_mebibyte, 0u), Status::invalid_state);
  CHECK_EQ(import.memory(), 0u); }

/* One bit of an accepted answer flipped anywhere: a refusal only if it names no
 * memory, and whatever is accepted is exactly what the bytes say. */
TEST(blob_import, a_byte_flip_sweep_accepts_only_what_the_bytes_say)
{ for (std::size_t byte = 0u; byte < BlobImportAnswer::encoded_size; byte++) {
      for (unsigned bit = 0u; bit < 8u; bit++) {
          MockTransport transport;
          BlobImport    import(transport);
          Answer        answer(0u, 0u, device_memory);
          answer.bytes()[byte] = static_cast<std::uint8_t>(answer.bytes()[byte] ^ (1u << bit));
          transport.set_reply(answer.bytes(), BlobImportAnswer::encoded_size);
          const Status status = import.import(chunk_resource, 0u, one_mebibyte, 0u);
          CHECK(import.invariants_hold());
          if (status == Status::ok) {
              CHECK(byte >= 8u);
              CHECK_EQ(import.memory(), u64_at(answer.bytes(), 8u));
          } else {
              CHECK_STATUS(status, Status::malformed);
              CHECK_EQ(import.memory(), 0u);
          }
      }
  } }

/* The import is forgotten whatever the release answers: a failed call or an
 * answer with bytes is reported, and the handle is never named again. */
TEST(blob_import, release_forgets_the_import_whatever_the_host_answers)
{ { MockTransport transport;
    BlobImport import(transport);
    const Answer taken(0u, 0u, device_memory);
    transport.set_reply(taken.bytes(), BlobImportAnswer::encoded_size);
    CHECK_STATUS(import.import(chunk_resource, 0u, one_mebibyte, 0u), Status::ok);
    CHECK_STATUS(import.release(), Status::malformed);
    CHECK_EQ(import.memory(), 0u);
    CHECK_STATUS(import.release(), Status::ok);
    CHECK_EQ(transport.sends(), 2u); }
  { MockTransport transport;
    BlobImport import(transport);
    const Answer taken(0u, 0u, device_memory);
    transport.set_reply(taken.bytes(), BlobImportAnswer::encoded_size);
    CHECK_STATUS(import.import(chunk_resource, 0u, one_mebibyte, 0u), Status::ok);
    transport.fail_next();
    CHECK_STATUS(import.release(), Status::invalid_state);
    CHECK_EQ(import.memory(), 0u);
    CHECK(import.invariants_hold()); } }

TEST(blob_import, invariants_refuse_a_refusal_that_holds_memory)
{ MockTransport transport;
  BlobImport import(transport);
  BlobImportProbe::refused_and_held(import);
  CHECK(!import.invariants_hold()); }
