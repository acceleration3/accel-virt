/* One context's setup and teardown as the requests the device reads, in order,
 * and every answer it might give. The device core itself answers in the
 * loopback suite; here each answer is written by hand. */
#include "core/rvk/context_memory.hpp"
#include "core/rvk/context_request_sequence.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/virtio_gpu/blob_request.hpp"
#include "proto/virtio_gpu/context_request.hpp"
#include "proto/virtio_gpu/create_blob_request.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"
#include "proto/virtio_gpu/rvk_request_writer.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstdint>
#include <cstring>

using av::BlobRequest;
using av::ContextMemory;
using av::ContextRequest;
using av::ContextRequestSequence;
using av::ContextSetup;
using av::CreateBlobRequest;
using av::GpuResponse;
using av::Irql;
using av::MockPlatform;
using av::ProtocolVersion;
using av::RvkRequestWriter;
using av::Status;
using av::SubApi;
using av::Violation;
using av::WireReader;

namespace av {

struct ContextRequestSequenceProbe {
    static void overcreate(ContextRequestSequence &sequence) { sequence.blobs_created_ = 4u; }
    static void blob_without_context(ContextRequestSequence &sequence) { sequence.blobs_created_ = 1u; }
};

}  // namespace av

namespace {

constexpr std::uint32_t context_id = 7u;

ContextSetup setup()
{
    return ContextSetup(ProtocolVersion(1u, 0u), SubApi::vulkan, 70u, 12u, 71u, 3u, 72u);
}

struct Sent {
    std::uint32_t type = 0u;
    std::uint32_t id = 0u;       /* context or resource */
    std::uint64_t address = 0u;
    std::uint64_t size = 0u;
};

class Fixture {
public:
    Fixture() : memory_(platform_), sequence_(platform_) { static_cast<void>(memory_.allocate(12u)); }

    MockPlatform           &platform() { return platform_; }
    ContextMemory          &memory() { return memory_; }
    ContextRequestSequence &sequence() { return sequence_; }

    /* The next request, read back as the device reads it. */
    Status next(Sent &sent)
    {
        std::uint8_t bytes[RvkRequestWriter::max_request_size] = {};
        std::size_t  length = 0u;
        const Status status = sequence_.next_request(bytes, sizeof bytes, length);
        if (status != Status::ok) return status;
        std::memcpy(&sent.type, bytes, 4u);
        WireReader reader(bytes, length);
        ContextRequest    context;
        CreateBlobRequest blob;
        BlobRequest       resource;
        switch (sent.type) {
        case 0x0200u: static_cast<void>(context.decode_create(reader)); sent.id = context.context_id(); break;
        case 0x0201u: static_cast<void>(context.decode_destroy(reader)); sent.id = context.context_id(); break;
        case 0x0207u: {
            static_cast<void>(context.decode_submit(reader));
            sent.id = context.context_id();
            ContextSetup decoded;
            if (decoded.decode(bytes + 32u, length - 32u) == Status::ok) sent.size = decoded.ring_resource();
            break;
        }
        case 0x010Cu:
            static_cast<void>(blob.decode(reader));
            sent.id = blob.resource_id();
            sent.size = blob.size();
            std::memcpy(&sent.address, bytes + 56u, 8u);
            break;
        default: static_cast<void>(resource.decode_resource(reader)); sent.id = resource.resource_id(); break;
        }
        return Status::ok;
    }
    Status answer(std::uint32_t type)
    {
        GpuResponse response;
        response.set(type);
        return sequence_.accept(response.bytes(), response.length());
    }

private:
    MockPlatform           platform_;
    ContextMemory          memory_;
    ContextRequestSequence sequence_;
};

}  // namespace

TEST(context_request_sequence_setup, creates_the_context_its_three_blobs_then_sets_it_up)
{ Fixture fixture;
  CHECK_STATUS(fixture.sequence().begin_setup(context_id, setup(), fixture.memory()), Status::ok);
  CHECK_STATUS(fixture.sequence().begin_setup(context_id, setup(), fixture.memory()), Status::invalid_state);
  const std::uint32_t types[] = {0x0200u, 0x010Cu, 0x010Cu, 0x010Cu, 0x0207u};
  const std::uint32_t ids[] = {context_id, 70u, 71u, 72u, context_id};
  const std::uint64_t addresses[] = {0u, fixture.memory().ring_address(), fixture.memory().fence_address(),
                                     fixture.memory().reply_address(), 0u};
  const std::uint64_t sizes[] = {0u, fixture.memory().ring_bytes(), ContextMemory::page_bytes,
                                 ContextMemory::page_bytes, 70u};
  for (std::uint32_t step = 0u; step < 5u; step++) {
      CHECK(!fixture.sequence().finished());
      Sent sent;
      CHECK_STATUS(fixture.next(sent), Status::ok);
      CHECK_EQ(sent.type, types[step]);
      CHECK_EQ(sent.id, ids[step]);
      if (addresses[step] != 0u) CHECK(sent.address == addresses[step]);
      CHECK(sent.size == sizes[step]);
      Sent waiting;
      CHECK_STATUS(fixture.next(waiting), Status::invalid_state);
      CHECK_STATUS(fixture.answer(GpuResponse::ok_nodata), Status::ok);
      CHECK(fixture.sequence().invariants_hold());
  }
  CHECK(fixture.sequence().finished());
  CHECK(!fixture.sequence().refused());
  Sent after;
  CHECK_STATUS(fixture.next(after), Status::invalid_state);
  CHECK_STATUS(fixture.answer(GpuResponse::ok_nodata), Status::invalid_state); }

TEST(context_request_sequence_teardown, destroys_the_context_then_releases_its_blobs)
{ Fixture fixture;
  CHECK_STATUS(fixture.sequence().begin_teardown(), Status::invalid_state);   /* nothing set up */
  CHECK_STATUS(fixture.sequence().begin_setup(context_id, setup(), fixture.memory()), Status::ok);
  for (std::uint32_t step = 0u; step < 5u; step++) {
      Sent sent;
      CHECK_STATUS(fixture.next(sent), Status::ok);
      CHECK_STATUS(fixture.answer(GpuResponse::ok_nodata), Status::ok);
  }
  CHECK_STATUS(fixture.sequence().begin_teardown(), Status::ok);
  const std::uint32_t types[] = {0x0201u, 0x0102u, 0x0102u, 0x0102u};
  const std::uint32_t ids[] = {context_id, 70u, 71u, 72u};
  for (std::uint32_t step = 0u; step < 4u; step++) {
      Sent sent;
      CHECK_STATUS(fixture.next(sent), Status::ok);
      CHECK_EQ(sent.type, types[step]);
      CHECK_EQ(sent.id, ids[step]);
      CHECK_STATUS(fixture.answer(GpuResponse::ok_nodata), Status::ok);
  }
  CHECK(fixture.sequence().finished());
  CHECK_STATUS(fixture.sequence().begin_teardown(), Status::invalid_state); }

TEST(context_request_sequence_setup, a_refusal_stops_the_setup_and_teardown_releases_only_what_was_created)
{ for (std::uint32_t refused_step = 0u; refused_step < 5u; refused_step++) {
      Fixture fixture;
      CHECK_STATUS(fixture.sequence().begin_setup(context_id, setup(), fixture.memory()), Status::ok);
      for (std::uint32_t step = 0u; step <= refused_step; step++) {
          Sent sent;
          CHECK_STATUS(fixture.next(sent), Status::ok);
          const bool refuse = step == refused_step;
          CHECK_STATUS(fixture.answer(refuse ? GpuResponse::err_invalid_parameter : GpuResponse::ok_nodata),
                       refuse ? Status::not_supported : Status::ok);
      }
      CHECK(fixture.sequence().finished());
      CHECK(fixture.sequence().refused());
      CHECK_EQ(fixture.sequence().refusal(), GpuResponse::err_invalid_parameter);
      Sent none;
      CHECK_STATUS(fixture.next(none), Status::invalid_state);
      /* A refused setup still releases what the device holds: the context once
       * created, and every blob created. */
      const std::uint32_t expected = (refused_step > 0u ? 1u : 0u) + (refused_step > 3u ? 3u : refused_step > 0u ? refused_step - 1u : 0u);
      if (expected == 0u) {
          CHECK_STATUS(fixture.sequence().begin_teardown(), Status::invalid_state);
          continue;
      }
      CHECK_STATUS(fixture.sequence().begin_teardown(), Status::ok);
      std::uint32_t sent_count = 0u;
      Sent sent;
      while (fixture.next(sent) == Status::ok) {
          sent_count++;
          CHECK_STATUS(fixture.answer(GpuResponse::ok_nodata), Status::ok);
      }
      CHECK_EQ(sent_count, expected);
      CHECK_EQ(fixture.platform().logged(), 1u);
  } }

TEST(context_request_sequence_teardown, a_refused_release_is_reported_and_the_rest_still_sent)
{ Fixture fixture;
  CHECK_STATUS(fixture.sequence().begin_setup(context_id, setup(), fixture.memory()), Status::ok);
  Sent sent;
  for (std::uint32_t step = 0u; step < 5u; step++) {
      CHECK_STATUS(fixture.next(sent), Status::ok);
      CHECK_STATUS(fixture.answer(GpuResponse::ok_nodata), Status::ok);
  }
  CHECK_STATUS(fixture.sequence().begin_teardown(), Status::ok);
  CHECK_STATUS(fixture.next(sent), Status::ok);
  CHECK_STATUS(fixture.answer(GpuResponse::err_invalid_context_id), Status::not_supported);
  std::uint32_t releases = 0u;
  while (fixture.next(sent) == Status::ok) {
      releases++;
      CHECK_STATUS(fixture.answer(GpuResponse::ok_nodata), Status::ok);
  }
  CHECK_EQ(releases, 3u);
  CHECK(fixture.sequence().refused());
  CHECK_EQ(fixture.sequence().refusal(), GpuResponse::err_invalid_context_id); }

TEST(context_request_sequence_abandon, a_device_reset_forgets_every_created_resource)
{ Fixture fixture;
  CHECK_STATUS(fixture.sequence().begin_setup(context_id, setup(), fixture.memory()), Status::ok);
  Sent sent;
  for (std::uint32_t step = 0u; step < 3u; step++) {
      CHECK_STATUS(fixture.next(sent), Status::ok);
      CHECK_STATUS(fixture.answer(GpuResponse::ok_nodata), Status::ok);
  }
  CHECK_STATUS(fixture.next(sent), Status::ok);   /* an answer outstanding */
  fixture.sequence().abandon();
  CHECK(fixture.sequence().finished());
  CHECK(fixture.sequence().invariants_hold());
  CHECK_STATUS(fixture.sequence().begin_teardown(), Status::invalid_state);
  CHECK_STATUS(fixture.sequence().begin_setup(context_id, setup(), fixture.memory()), Status::ok); }

TEST(context_request_sequence_teardown, an_unreadable_answer_keeps_the_teardown_going)
{ Fixture fixture;
  CHECK_STATUS(fixture.sequence().begin_setup(context_id, setup(), fixture.memory()), Status::ok);
  Sent sent;
  for (std::uint32_t step = 0u; step < 5u; step++) {
      CHECK_STATUS(fixture.next(sent), Status::ok);
      CHECK_STATUS(fixture.answer(GpuResponse::ok_nodata), Status::ok);
  }
  CHECK_STATUS(fixture.sequence().begin_teardown(), Status::ok);
  CHECK_STATUS(fixture.next(sent), Status::ok);
  const std::uint8_t bytes[4] = {};
  CHECK_STATUS(fixture.sequence().accept(bytes, sizeof bytes), Status::malformed);
  std::uint32_t releases = 0u;
  while (fixture.next(sent) == Status::ok) {
      releases++;
      CHECK_STATUS(fixture.answer(GpuResponse::ok_nodata), Status::ok);
  }
  CHECK_EQ(releases, 3u); }

TEST(context_request_sequence_accept, a_short_answer_is_malformed_and_ends_the_sequence)
{ Fixture fixture;
  CHECK_STATUS(fixture.sequence().begin_setup(context_id, setup(), fixture.memory()), Status::ok);
  Sent sent;
  CHECK_STATUS(fixture.next(sent), Status::ok);
  const std::uint8_t bytes[8] = {};
  CHECK_STATUS(fixture.sequence().accept(bytes, sizeof bytes), Status::malformed);
  CHECK(fixture.sequence().finished());
  CHECK(fixture.sequence().refused());
  CHECK_STATUS(fixture.sequence().begin_teardown(), Status::ok);   /* the context may exist */
  CHECK_STATUS(fixture.next(sent), Status::ok);
  CHECK_EQ(sent.type, 0x0201u); }

TEST(context_request_sequence_begin_setup, memory_not_held_or_a_destination_too_small_sends_nothing)
{ Fixture fixture;
  MockPlatform other;
  ContextMemory empty(other);
  CHECK_STATUS(fixture.sequence().begin_setup(context_id, setup(), empty), Status::invalid_parameter);
  CHECK_STATUS(fixture.sequence().begin_setup(context_id, setup(), fixture.memory()), Status::ok);
  std::uint8_t small[64] = {};
  std::size_t  length = 9u;
  CHECK_STATUS(fixture.sequence().next_request(small, sizeof small, length), Status::too_large);
  CHECK_EQ(length, 0u);
  Sent sent;
  CHECK_STATUS(fixture.next(sent), Status::ok); }

TEST(context_request_sequence, runs_at_dispatch_at_most)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::isr);
  CHECK_STATUS(fixture.sequence().begin_teardown(), Status::invalid_state);
  std::uint8_t bytes[96] = {};
  std::size_t  length = 0u;
  static_cast<void>(fixture.sequence().next_request(bytes, sizeof bytes, length));
  static_cast<void>(fixture.sequence().accept(bytes, 0u));
  static_cast<void>(fixture.sequence().begin_setup(context_id, setup(), fixture.memory()));
  fixture.platform().set_irql(Irql::passive);
  for (int count = 0; count < 4; count++) CHECK(fixture.platform().claim(Violation::irql_ceiling)); }

TEST(context_request_sequence_invariants, more_blobs_created_than_a_context_has_is_detected)
{ Fixture fixture;
  av::ContextRequestSequenceProbe::overcreate(fixture.sequence());
  CHECK(!fixture.sequence().invariants_hold());
  Fixture orphan;
  av::ContextRequestSequenceProbe::blob_without_context(orphan.sequence());
  CHECK(!orphan.sequence().invariants_hold()); }

TEST(context_request_sequence_begin_teardown, waits_for_a_running_sequence)
{ Fixture fixture;
  CHECK_STATUS(fixture.sequence().begin_setup(context_id, setup(), fixture.memory()), Status::ok);
  Sent sent;
  CHECK_STATUS(fixture.next(sent), Status::ok);
  CHECK(!fixture.sequence().finished());   /* an answer is awaited */
  CHECK_STATUS(fixture.answer(GpuResponse::ok_nodata), Status::ok);
  CHECK_STATUS(fixture.sequence().begin_teardown(), Status::invalid_state);   /* still setting up */
  while (fixture.next(sent) == Status::ok) CHECK_STATUS(fixture.answer(GpuResponse::ok_nodata), Status::ok);
  CHECK_STATUS(fixture.sequence().begin_teardown(), Status::ok);
  CHECK_STATUS(fixture.sequence().begin_teardown(), Status::invalid_state);   /* already tearing down */
  CHECK_STATUS(fixture.next(sent), Status::ok);
  CHECK_STATUS(fixture.answer(GpuResponse::err_invalid_context_id), Status::not_supported);
  CHECK_STATUS(fixture.next(sent), Status::ok);
  CHECK_STATUS(fixture.answer(GpuResponse::err_invalid_resource_id), Status::not_supported);
  CHECK_EQ(fixture.sequence().refusal(), GpuResponse::err_invalid_context_id);   /* the first one kept */ }
