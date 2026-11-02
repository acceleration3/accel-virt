#include "core/support/status.hpp"
#include "device/execution/blob_imports.hpp"
#include "device/recording/wire_recorder.hpp"
#include "device/execution/context_executor.hpp"
#include "device/execution/executor_library.hpp"
#include "device/execution/executor_shares.hpp"
#include "device/execution/transport_command.hpp"
#include "device/memory/host_blob_table.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/submission/blob_import_answer.hpp"
#include "proto/submission/command_header.hpp"

#include <cstddef>
#include <cstdint>

using av::BlobImportAnswer;
using av::CommandHeader;
using av::HostViolation;
using av::Status;
using av::TransportCommand;

namespace {
class Fixture {
public:
    Fixture()
        : shares_(host_, library_), executor_(host_, library_, shares_, 1u), blobs_(host_),
          recorder_(host_), imports_(host_, blobs_, executor_, recorder_, 1u)
    {}

    av::MockHost &host() { return host_; }
    Status run(const CommandHeader &header, const std::uint8_t *body = nullptr)
    {
        reply_[0] = 0xEEu;
        produced_ = 99u;
        return TransportCommand(imports_, executor_, header, body).run(reply_, sizeof reply_, produced_);
    }
    std::size_t         produced() const { return produced_; }
    const std::uint8_t *reply() const { return reply_; }

private:
    av::MockHost        host_;
    av::ExecutorLibrary library_;
    av::ExecutorShares  shares_;
    av::ContextExecutor executor_;
    av::HostBlobTable   blobs_;
    av::WireRecorder    recorder_;
    av::BlobImports     imports_;
    std::uint8_t        reply_[32] = {};
    std::size_t         produced_ = 0u;
};

}

TEST(transport_command_run, a_nop_without_a_body_runs_whether_or_not_it_asks_for_an_answer)
{ Fixture fixture;
  CHECK_EQ(CommandHeader::nop_id, 0x00000001u);
  CHECK_EQ(CommandHeader(CommandHeader::nop_id, 8u, 0u).api(), CommandHeader::transport_api);
  CHECK_STATUS(fixture.run(CommandHeader(CommandHeader::nop_id, 8u, 0u)), Status::ok);
  CHECK_EQ(fixture.produced(), 0u);
  CHECK_STATUS(fixture.run(CommandHeader(CommandHeader::nop_id, 8u, CommandHeader::generate_reply)), Status::ok);
  CHECK_EQ(fixture.produced(), 0u);
  CHECK_EQ(fixture.reply()[0], 0xEEu); }

TEST(transport_command_run, a_nop_of_any_size_but_its_header_is_malformed)
{ Fixture fixture;
  const std::uint32_t sizes[] = {0u, 1u, 7u, 9u, 15u, 16u, 0x1000u, CommandHeader::max_size - 1u,
                                 CommandHeader::max_size, 0xFFFFFFFFu};
  for (const std::uint32_t size : sizes) {
      CHECK_STATUS(fixture.run(CommandHeader(CommandHeader::nop_id, size, 0u)), Status::malformed);
      CHECK_STATUS(fixture.run(CommandHeader(CommandHeader::nop_id, size, CommandHeader::generate_reply)),
                   Status::malformed);
      CHECK_EQ(fixture.produced(), 0u);
  } }

TEST(transport_command_run, a_transport_id_the_device_does_not_answer_is_not_known_whatever_its_size)
{ Fixture fixture;
  const std::uint32_t ids[] = {0x00000000u, 0x0000000Eu, 0x00000101u, 0x00008001u, 0x0000FFFFu};
  for (const std::uint32_t id : ids) {
      CHECK_STATUS(fixture.run(CommandHeader(id, 8u, 0u)), Status::not_found);
      CHECK_STATUS(fixture.run(CommandHeader(id, 9u, CommandHeader::generate_reply)), Status::not_found);
      CHECK_EQ(fixture.produced(), 0u);
  } }

TEST(transport_command_run, the_blob_ids_are_answered_by_the_blob_command)
{ Fixture fixture;
  CHECK_EQ(CommandHeader::import_blob_id, 0x00000002u);
  CHECK_EQ(CommandHeader::release_blob_id, 0x00000003u);
  std::uint8_t reserved_set[8] = {};
  reserved_set[4] = 1u;
  CHECK_STATUS(fixture.run(CommandHeader(CommandHeader::import_blob_id, 16u, CommandHeader::generate_reply),
                           reserved_set),
               Status::ok);
  CHECK_EQ(fixture.produced(), BlobImportAnswer::encoded_size);
  BlobImportAnswer answer;
  CHECK_STATUS(answer.decode(fixture.reply(), BlobImportAnswer::encoded_size), Status::ok);
  CHECK_EQ(answer.status(), BlobImportAnswer::refused_request);
  CHECK_STATUS(fixture.run(CommandHeader(CommandHeader::import_blob_id, 8u, 0u)), Status::malformed);
  const std::uint8_t handle[8] = {};
  CHECK_STATUS(fixture.run(CommandHeader(CommandHeader::release_blob_id, 16u, 0u), handle), Status::ok);
  CHECK_EQ(fixture.produced(), 0u);
  CHECK(fixture.host().claim(HostViolation::worker_required));
  CHECK_STATUS(fixture.run(CommandHeader(CommandHeader::release_blob_id, 8u, 0u)), Status::malformed); }

TEST(transport_command_run, every_bit_flip_of_a_nop_is_refused_skipped_or_still_a_nop)
{ Fixture fixture;
  std::uint8_t bytes[CommandHeader::encoded_size] = {};
  CHECK_STATUS(CommandHeader(CommandHeader::nop_id, 8u, 0u).encode(bytes, sizeof bytes), Status::ok);
  for (std::uint32_t bit = 0u; bit < 64u; bit++) {
      std::uint8_t flipped[CommandHeader::encoded_size] = {};
      for (std::uint32_t i = 0u; i < sizeof flipped; i++) flipped[i] = bytes[i];
      flipped[bit / 8u] = static_cast<std::uint8_t>(flipped[bit / 8u] ^ (1u << (bit % 8u)));
      CommandHeader header;
      const Status  decoded = header.decode(flipped, sizeof flipped);
      if (bit == 35u || bit > 56u) {
          CHECK_STATUS(decoded, Status::malformed);
          continue;
      }
      CHECK_STATUS(decoded, Status::ok);
      if (bit >= 16u && bit < 32u) {
          CHECK(header.api() != CommandHeader::transport_api);
          continue;
      }
      const bool known = (header.id() >= CommandHeader::nop_id &&
                          header.id() <= CommandHeader::release_shared_object_id) ||
                         header.id() == CommandHeader::answer_piece_id;
      const Status expected = bit == 1u || (bit >= 32u && bit != 56u) ? Status::malformed
                              : bit < 16u                             ? (known ? Status::malformed
                                                                               : Status::not_found)
                                                                      : Status::ok;
      CHECK_STATUS(fixture.run(header), expected);
  } }
