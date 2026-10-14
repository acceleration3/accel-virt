/* IMPORT_BLOB and RELEASE_BLOB as the device answers them (docs/host-memory.md):
 * an import of a host blob the context's process opened answers status 0 and
 * the executor's handle; every refusal is an answer with its own status and
 * the context runs on; only a body of the wrong length is framing. Run over
 * ImportRig, on the context's worker, with library memory behind every blob. */
#include "core/support/status.hpp"
#include "device/execution/blob_import_command.hpp"
#include "device/execution/blob_imports.hpp"
#include "harness/executor/import_rig.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "proto/submission/blob_import_answer.hpp"
#include "proto/submission/blob_import_request.hpp"
#include "proto/submission/command_header.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::BlobImportAnswer;
using av::BlobImportCommand;
using av::BlobImportRequest;
using av::CommandHeader;
using av::ImportRig;
using av::MockExecutorOps;
using av::Status;

namespace {

constexpr std::uint8_t  untouched = 0xEEu;

/* One command run on the worker, as the batch runs it: its status, what it
 * produced, and the reply slot it wrote. */
class Run {
public:
    Run(ImportRig &rig, std::uint32_t id, const std::uint8_t *body, std::size_t length,
        std::size_t capacity = sizeof(reply_))
        : status_(Status::invalid_state), produced_(99u)
    {
        std::memset(reply_, untouched, sizeof reply_);
        const CommandHeader header(id, static_cast<std::uint32_t>(CommandHeader::encoded_size + length), 0u);
        rig.on_worker([&] {
            status_ = BlobImportCommand(rig.imports(), header, body).run(reply_, capacity, produced_);
        });
        CHECK(rig.imports().invariants_hold());
    }

    Status           status() const { return status_; }
    std::size_t      produced() const { return produced_; }
    const std::uint8_t *reply() const { return reply_; }
    BlobImportAnswer answer() const
    {
        BlobImportAnswer decoded(BlobImportAnswer::no_room, 0u);
        CHECK_STATUS(decoded.decode(reply_, BlobImportAnswer::encoded_size), Status::ok);
        return decoded;
    }

private:
    Status       status_;
    std::size_t  produced_;
    std::uint8_t reply_[32];
};

class ImportBody {
public:
    explicit ImportBody(std::uint32_t resource)
    {
        CHECK_STATUS(BlobImportRequest(resource).encode(bytes_, sizeof bytes_), Status::ok);
    }
    std::uint8_t       *bytes() { return bytes_; }
    const std::uint8_t *bytes() const { return bytes_; }

private:
    std::uint8_t bytes_[BlobImportRequest::encoded_size] = {};
};

Run import(ImportRig &rig, const ImportBody &body)
{
    return Run(rig, CommandHeader::import_blob_id, body.bytes(), BlobImportRequest::encoded_size);
}

Run release(ImportRig &rig, std::uint64_t device_memory)
{
    std::uint8_t bytes[8] = {};
    std::memcpy(bytes, &device_memory, sizeof bytes);
    return Run(rig, CommandHeader::release_blob_id, bytes, sizeof bytes);
}

/* A rig whose host blob 0 is three pages the context opened. */
class Chunked {
public:
    Chunked() { CHECK_STATUS(rig_.create_host_blob(rig_.blob(0u), ImportRig::context, 3u), Status::ok); }
    ImportRig &rig() { return rig_; }

private:
    ImportRig rig_;
};

}  // namespace

TEST(blob_import_command_run, an_import_of_a_host_blob_answers_status_0_and_the_executors_handle)
{ Chunked chunked;
  ImportRig &rig = chunked.rig();
  const Run run = import(rig, ImportBody(rig.blob(0u)));
  CHECK_STATUS(run.status(), Status::ok);
  CHECK_EQ(run.produced(), BlobImportAnswer::encoded_size);
  CHECK_EQ(run.answer().status(), BlobImportAnswer::imported);
  CHECK_EQ(run.answer().device_memory(), 1u | MockExecutorOps::handle_bit);
  std::uint64_t key = 0u;
  CHECK(rig.ops().import_of(1u, key));
  CHECK_EQ(key, rig.blob(0u));   /* the memory allocated under the blob's id */
  CHECK_EQ(rig.imports().imports(), 1u);
  CHECK_STATUS(release(rig, run.answer().device_memory()).status(), Status::ok);
  CHECK_EQ(rig.imports().imports(), 0u); }

TEST(blob_import_command_run, every_refusal_is_its_own_status_with_no_handle_and_no_fault)
{ Chunked chunked;
  ImportRig &rig = chunked.rig();
  CHECK_STATUS(rig.create_host_blob(rig.blob(1u), ImportRig::other_context), Status::ok);
  struct Case { const char *name; ImportBody body; std::uint32_t status; };
  ImportBody reserved(rig.blob(0u));
  reserved.bytes()[5] = 1u;
  ImportBody high_reserved(rig.blob(0u));
  high_reserved.bytes()[7] = 0x80u;
  const Case cases[] = {
      {"a reserved field", reserved, BlobImportAnswer::refused_request},
      {"the top of the reserved field", high_reserved, BlobImportAnswer::refused_request},
      {"no resource", ImportBody(0u), BlobImportAnswer::refused_request},
      {"another context's blob", ImportBody(rig.blob(1u)), BlobImportAnswer::unknown_resource},
      {"a host blob id with no blob", ImportBody(rig.blob(2u)), BlobImportAnswer::unknown_resource},
      {"an id below the host blob range", ImportBody(rig.blob(0u) - 1u), BlobImportAnswer::unknown_resource},
  };
  for (const Case &c : cases) {
      const Run run = import(rig, c.body);
      CHECK_STATUS(run.status(), Status::ok);
      CHECK_EQ(run.produced(), BlobImportAnswer::encoded_size);
      if (run.answer().status() != c.status)
          av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "%s answered %u", c.name,
                                                      run.answer().status());
      CHECK_EQ(run.answer().device_memory(), 0u);
  }
  rig.host().fail_allocation_at(1u);   /* the first import takes the context's table */
  CHECK_EQ(import(rig, ImportBody(rig.blob(0u))).answer().status(), BlobImportAnswer::no_room);
  rig.ops().refuse_import_at(1u);
  CHECK_EQ(import(rig, ImportBody(rig.blob(0u))).answer().status(), BlobImportAnswer::executor_refused);
  CHECK_EQ(rig.imports().imports(), 0u);
  CHECK_EQ(rig.ops().imported(), 0u);
  /* After all of that, the context imports as if nothing happened. */
  CHECK_EQ(import(rig, ImportBody(rig.blob(0u))).answer().status(), BlobImportAnswer::imported); }

TEST(blob_import_command_run, a_context_with_no_room_for_another_import_answers_no_room)
{ Chunked chunked;
  ImportRig &rig = chunked.rig();
  /* Filled on the worker directly: a Run per import would judge the whole
   * table's invariants 4096 times over. */
  Status filled = Status::ok;
  rig.on_worker([&] {
      std::uint64_t handle = 0u;
      for (std::uint32_t index = 0u; index < av::BlobImports::max_imports && filled == Status::ok; index++)
          filled = rig.imports().import(rig.blob(0u), handle);
  });
  CHECK_STATUS(filled, Status::ok);
  const Run full = import(rig, ImportBody(rig.blob(0u)));
  CHECK_STATUS(full.status(), Status::ok);
  CHECK_EQ(full.answer().status(), BlobImportAnswer::no_room); }

TEST(blob_import_command_run, a_body_of_the_wrong_length_is_framing_and_answers_nothing)
{ Chunked chunked;
  ImportRig &rig = chunked.rig();
  std::uint8_t bytes[64] = {};
  const ImportBody valid(rig.blob(0u));
  std::memcpy(bytes, valid.bytes(), BlobImportRequest::encoded_size);
  const std::size_t import_lengths[] = {0u, 4u, 7u, 9u, 16u, 32u, 64u};
  for (const std::size_t length : import_lengths) {
      const Run run(rig, CommandHeader::import_blob_id, bytes, length);
      CHECK_STATUS(run.status(), Status::malformed);
      CHECK_EQ(run.produced(), 0u);
      CHECK_EQ(run.reply()[0], untouched);
  }
  const std::size_t release_lengths[] = {0u, 4u, 7u, 9u, 16u};
  for (const std::size_t length : release_lengths) {
      const Run run(rig, CommandHeader::release_blob_id, bytes, length);
      CHECK_STATUS(run.status(), Status::malformed);
      CHECK_EQ(run.produced(), 0u);
  }
  CHECK_EQ(rig.ops().imported(), 0u);
  CHECK_EQ(rig.ops().released(), 0u); }

TEST(blob_import_command_run, release_answers_empty_whether_or_not_the_handle_was_imported)
{ Chunked chunked;
  ImportRig &rig = chunked.rig();
  const std::uint64_t first = import(rig, ImportBody(rig.blob(0u))).answer().device_memory();
  const std::uint64_t second = import(rig, ImportBody(rig.blob(0u))).answer().device_memory();
  const Run never = release(rig, 0x1234u);
  CHECK_STATUS(never.status(), Status::ok);
  CHECK_EQ(never.produced(), 0u);
  CHECK_EQ(never.reply()[0], untouched);
  CHECK_EQ(rig.ops().released(), 0u);
  CHECK_STATUS(release(rig, first).status(), Status::ok);
  CHECK_EQ(rig.ops().live_imports(), 1u);
  CHECK_STATUS(release(rig, second).status(), Status::ok);
  CHECK_EQ(rig.ops().live_imports(), 0u);
  CHECK_STATUS(release(rig, second).status(), Status::ok);
  CHECK_EQ(rig.ops().released(), 2u); }

TEST(blob_import_command_run, a_slot_too_small_for_the_answer_is_written_nothing_but_told_its_length)
{ Chunked chunked;
  ImportRig &rig = chunked.rig();
  const ImportBody body(rig.blob(0u));
  const Run small(rig, CommandHeader::import_blob_id, body.bytes(), BlobImportRequest::encoded_size,
                  BlobImportAnswer::encoded_size - 1u);
  CHECK_STATUS(small.status(), Status::ok);
  CHECK_EQ(small.produced(), BlobImportAnswer::encoded_size);
  CHECK_EQ(small.reply()[0], untouched);
  CHECK_EQ(rig.imports().imports(), 1u);   /* the import stands: the guest asked for it */
  const Run none(rig, CommandHeader::import_blob_id, body.bytes(), BlobImportRequest::encoded_size, 0u);
  CHECK_EQ(none.produced(), BlobImportAnswer::encoded_size); }

TEST(blob_import_command_run, a_transport_id_that_is_neither_is_not_its_to_answer)
{ ImportRig rig;
  const std::uint32_t ids[] = {0x00000000u, CommandHeader::nop_id, 0x00000004u, 0x0000FFFFu};
  for (const std::uint32_t id : ids) {
      const Run run(rig, id, nullptr, 0u);
      CHECK_STATUS(run.status(), Status::not_found);
      CHECK_EQ(run.produced(), 0u);
  } }

/* Every single-bit corruption of a valid import: always an answer - the length
 * is right - whose status the contract decides. A flip in the resource names
 * another id, no blob of this context's - or, for the one bit set, no resource
 * at all; a flip in the reserved half is a request no driver writes. */
TEST(blob_import_command_run, every_bit_flip_of_an_import_is_an_answer_and_leaves_the_context_consistent)
{ Chunked chunked;
  ImportRig &rig = chunked.rig();
  const ImportBody    valid(rig.blob(0u));
  const std::uint32_t only_bit = 24u;   /* HostBlobId::first is 1 << 24 */
  for (std::uint32_t bit = 0u; bit < BlobImportRequest::encoded_size * 8u; bit++) {
      ImportBody flipped = valid;
      flipped.bytes()[bit / 8u] = static_cast<std::uint8_t>(flipped.bytes()[bit / 8u] ^ (1u << (bit % 8u)));
      const Run run = import(rig, flipped);
      CHECK_STATUS(run.status(), Status::ok);
      const BlobImportAnswer answer = run.answer();
      const std::uint32_t    expected = bit >= 32u || bit == only_bit ? BlobImportAnswer::refused_request
                                                                      : BlobImportAnswer::unknown_resource;
      if (answer.status() != expected)
          av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "bit %u answered %u", bit, answer.status());
      CHECK_EQ(answer.device_memory(), 0u);
      CHECK_EQ(rig.imports().imports(), 0u);
  }
  CHECK_EQ(rig.ops().imported(), 0u); }
