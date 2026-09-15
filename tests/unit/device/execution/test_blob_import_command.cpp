/* IMPORT_BLOB and RELEASE_BLOB as the device answers them (feature 045, T013):
 * an import of a context's guest chunk answers status 0 and the executor's
 * handle; every refusal is an answer with its own status and the context runs
 * on; only a body of the wrong length is framing. Run over ImportRig, on the
 * context's worker, with guest pages behind every blob. */
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

constexpr std::uint64_t page = ImportRig::page;
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
    ImportBody(std::uint32_t resource, std::uint64_t offset, std::uint64_t size, std::uint32_t memory_type)
    {
        CHECK_STATUS(BlobImportRequest(resource, offset, size, memory_type).encode(bytes_, sizeof bytes_), Status::ok);
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

/* A rig whose chunk 0 is a guest blob over pages 12, 7 and 8. */
class Chunked {
public:
    Chunked()
    {
        const std::uint32_t pages[] = {12u, 7u, 8u};
        CHECK_STATUS(rig_.create_guest_blob(rig_.chunk(0u), pages, 3u), Status::ok);
    }
    ImportRig &rig() { return rig_; }

private:
    ImportRig rig_;
};

}  // namespace

TEST(blob_import_command_run, an_import_of_a_guest_chunk_answers_status_0_and_the_executors_handle)
{ Chunked chunked;
  ImportRig &rig = chunked.rig();
  const Run run = import(rig, ImportBody(rig.chunk(0u), page, 2u * page, 6u));
  CHECK_STATUS(run.status(), Status::ok);
  CHECK_EQ(run.produced(), BlobImportAnswer::encoded_size);
  CHECK_EQ(run.answer().status(), BlobImportAnswer::imported);
  CHECK_EQ(run.answer().device_memory(), 1u | MockExecutorOps::handle_bit);
  RvkBlobHandle handed = {};
  CHECK(rig.ops().import_of(1u, handed));
  CHECK_EQ(handed.reserved, 6u);
  CHECK_EQ(handed.size, 2u * page);
  CHECK(std::memcmp(handed.pointer, rig.guest_page(7u), page) == 0);
  CHECK_EQ(rig.imports().imports(), 1u); }

TEST(blob_import_command_run, every_refusal_is_its_own_status_with_no_handle_and_no_fault)
{ Chunked chunked;
  ImportRig &rig = chunked.rig();
  CHECK_STATUS(rig.blobs().create_host(rig.chunk(1u), page), Status::ok);
  struct Case { const char *name; ImportBody body; std::uint32_t status; };
  ImportBody reserved(rig.chunk(0u), 0u, page, 0u);
  reserved.bytes()[5] = 1u;
  ImportBody trailing(rig.chunk(0u), 0u, page, 0u);
  trailing.bytes()[31] = 0x80u;
  const Case cases[] = {
      {"a reserved field", reserved, BlobImportAnswer::refused_request},
      {"a trailing reserved field", trailing, BlobImportAnswer::refused_request},
      {"an offset inside a page", ImportBody(rig.chunk(0u), 1u, page, 0u), BlobImportAnswer::refused_request},
      {"a size short of a page", ImportBody(rig.chunk(0u), 0u, page - 1u, 0u), BlobImportAnswer::refused_request},
      {"no size", ImportBody(rig.chunk(0u), 0u, 0u, 0u), BlobImportAnswer::refused_request},
      {"a range wrapping 2^64", ImportBody(rig.chunk(0u), 0xFFFFFFFFFFFFF000u, 2u * page, 0u),
       BlobImportAnswer::refused_request},
      {"the ring", ImportBody(ImportRig::ring, 0u, page, 0u), BlobImportAnswer::unknown_resource},
      {"past the last chunk slot", ImportBody(ImportRig::ring + 67u, 0u, page, 0u), BlobImportAnswer::unknown_resource},
      {"a chunk slot with no blob", ImportBody(rig.chunk(2u), 0u, page, 0u), BlobImportAnswer::unknown_resource},
      {"a blob the host backs", ImportBody(rig.chunk(1u), 0u, page, 0u), BlobImportAnswer::unknown_resource},
      {"a page past the blob", ImportBody(rig.chunk(0u), 3u * page, page, 0u), BlobImportAnswer::outside_blob},
      {"one page too many", ImportBody(rig.chunk(0u), 0u, 4u * page, 0u), BlobImportAnswer::outside_blob},
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
  rig.host().fail_alias_at(1u);
  CHECK_EQ(import(rig, ImportBody(rig.chunk(0u), 0u, page, 0u)).answer().status(), BlobImportAnswer::unmappable_pages);
  rig.ops().refuse_import_at(1u);
  CHECK_EQ(import(rig, ImportBody(rig.chunk(0u), 0u, page, 0u)).answer().status(), BlobImportAnswer::executor_refused);
  CHECK_EQ(rig.imports().imports(), 0u);
  CHECK_EQ(rig.host().live_ranges(), 0u);
  CHECK_EQ(rig.ops().imported(), 0u);
  /* After all of that, the context imports as if nothing happened. */
  CHECK_EQ(import(rig, ImportBody(rig.chunk(0u), 0u, page, 0u)).answer().status(), BlobImportAnswer::imported); }

TEST(blob_import_command_run, a_context_with_no_room_for_another_import_answers_no_room)
{ Chunked chunked;
  ImportRig &rig = chunked.rig();
  for (std::uint32_t index = 0u; index < av::BlobImports::max_imports; index++)
      CHECK_EQ(import(rig, ImportBody(rig.chunk(0u), 0u, page, 0u)).answer().status(), BlobImportAnswer::imported);
  const Run full = import(rig, ImportBody(rig.chunk(0u), 0u, page, 0u));
  CHECK_STATUS(full.status(), Status::ok);
  CHECK_EQ(full.answer().status(), BlobImportAnswer::no_room); }

TEST(blob_import_command_run, a_body_of_the_wrong_length_is_framing_and_answers_nothing)
{ Chunked chunked;
  ImportRig &rig = chunked.rig();
  std::uint8_t bytes[64] = {};
  const ImportBody valid(rig.chunk(0u), 0u, page, 0u);
  std::memcpy(bytes, valid.bytes(), BlobImportRequest::encoded_size);
  const std::size_t import_lengths[] = {0u, 8u, 16u, 31u, 33u, 40u, 64u};
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
  const std::uint64_t first = import(rig, ImportBody(rig.chunk(0u), 0u, page, 0u)).answer().device_memory();
  const std::uint64_t second = import(rig, ImportBody(rig.chunk(0u), page, page, 1u)).answer().device_memory();
  CHECK_EQ(rig.host().live_ranges(), 1u);   /* one alias per blob, however many imports */
  const Run never = release(rig, 0x1234u);
  CHECK_STATUS(never.status(), Status::ok);
  CHECK_EQ(never.produced(), 0u);
  CHECK_EQ(never.reply()[0], untouched);
  CHECK_EQ(rig.ops().released(), 0u);
  CHECK_STATUS(release(rig, first).status(), Status::ok);
  CHECK_EQ(rig.ops().live_imports(), 1u);
  CHECK_EQ(rig.host().live_ranges(), 1u);
  CHECK_STATUS(release(rig, second).status(), Status::ok);
  CHECK_EQ(rig.ops().live_imports(), 0u);
  CHECK_EQ(rig.host().live_ranges(), 0u);   /* released with its last import */
  CHECK_STATUS(release(rig, second).status(), Status::ok);
  CHECK_EQ(rig.ops().released(), 2u); }

TEST(blob_import_command_run, a_slot_too_small_for_the_answer_is_written_nothing_but_told_its_length)
{ Chunked chunked;
  ImportRig &rig = chunked.rig();
  const ImportBody body(rig.chunk(0u), 0u, page, 0u);
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
 * is right - whose status the contract decides, and never a handle without an
 * import behind it. */
TEST(blob_import_command_run, every_bit_flip_of_an_import_is_an_answer_and_leaves_the_context_consistent)
{ Chunked chunked;
  ImportRig &rig = chunked.rig();
  const ImportBody valid(rig.chunk(0u), page, page, 2u);
  for (std::uint32_t bit = 0u; bit < BlobImportRequest::encoded_size * 8u; bit++) {
      ImportBody flipped = valid;
      flipped.bytes()[bit / 8u] = static_cast<std::uint8_t>(flipped.bytes()[bit / 8u] ^ (1u << (bit % 8u)));
      const Run run = import(rig, flipped);
      CHECK_STATUS(run.status(), Status::ok);
      const BlobImportAnswer answer = run.answer();
      const bool             memory_type = bit >= 192u && bit < 224u;
      if (memory_type) CHECK_EQ(answer.status(), BlobImportAnswer::imported);
      if ((bit >= 32u && bit < 64u) || bit >= 224u) CHECK_EQ(answer.status(), BlobImportAnswer::refused_request);
      if (answer.status() == BlobImportAnswer::imported) CHECK_STATUS(release(rig, answer.device_memory()).status(), Status::ok);
      CHECK_EQ(rig.imports().imports(), 0u);
      CHECK_EQ(rig.host().live_ranges(), 0u);
  } }
