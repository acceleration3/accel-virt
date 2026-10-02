/* One context's setup and teardown as the requests the device reads, in order,
 * and every answer it might give. The device core itself answers in the
 * loopback suite; here each answer is written by hand. */
#include "miniport/av_context/context_pages.hpp"
#include "miniport/av_context/context_request_sequence.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/resources/guest_page_list.hpp"
#include "proto/virtio_gpu/blob_request.hpp"
#include "proto/virtio_gpu/context_request.hpp"
#include "proto/virtio_gpu/create_blob_request.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstdint>
#include <cstring>

using av::BlobRequest;
using av::ContextPages;
using av::ContextRequest;
using av::ContextRequestSequence;
using av::ContextSetup;
using av::CreateBlobRequest;
using av::GpuResponse;
using av::GuestPageList;
using av::Irql;
using av::MockPlatform;
using av::ProtocolVersion;
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
constexpr std::uint64_t page = GuestPageList::page_bytes;
constexpr std::uint32_t max_entries = 4u;
constexpr std::size_t   request_capacity = 256u;

ContextSetup setup()
{
    return ContextSetup(ProtocolVersion(1u, 0u), SubApi::vulkan, 70u, 12u, 71u, 3u, 72u);
}

/* A process's allocations as VidMm scatters them: a three-page ring in two runs
 * - a pair, then a lone page far away - and a page each for fence and reply. */
constexpr std::uint64_t ring_frames[] = {0x81000u, 0x82000u, 0x2A000u};
constexpr std::uint64_t fence_frame[] = {0x44000u};
constexpr std::uint64_t reply_frame[] = {0x9F000u};

struct Sent {
    std::uint32_t type = 0u;
    std::uint32_t id = 0u;       /* context or resource */
    std::uint64_t size = 0u;
    std::uint32_t entries = 0u;
    std::uint64_t addresses[max_entries] = {};
    std::uint32_t lengths[max_entries] = {};
};

class Fixture {
public:
    Fixture()
        : sequence_(platform_),
          pages_(GuestPageList(ring_frames, 3u), GuestPageList(fence_frame, 1u), GuestPageList(reply_frame, 1u),
                 ring_view_, fence_view_) {}

    MockPlatform           &platform() { return platform_; }
    const ContextPages     &pages() const { return pages_; }
    ContextRequestSequence &sequence() { return sequence_; }

    /* The next request, read back as the device reads it. */
    Status next(Sent &sent)
    {
        std::uint8_t bytes[request_capacity] = {};
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
        case 0x010Cu: decode_blob(blob, reader, bytes, length, sent); break;
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
    /* Every memory entry, and a request exactly as long as its entries say. */
    void decode_blob(CreateBlobRequest &blob, WireReader &reader, const std::uint8_t *bytes, std::size_t length,
                     Sent &sent) const
    {
        CHECK_STATUS(blob.decode(reader), Status::ok);
        CHECK_EQ(blob.memory(), CreateBlobRequest::memory_guest);
        CHECK_EQ(length, CreateBlobRequest::fixed_size + blob.entries() * CreateBlobRequest::entry_size);
        sent.id = blob.resource_id();
        sent.size = blob.size();
        sent.entries = blob.entries();
        for (std::uint32_t entry = 0u; entry < blob.entries() && entry < max_entries; entry++) {
            const std::size_t at = CreateBlobRequest::fixed_size + entry * CreateBlobRequest::entry_size;
            std::memcpy(&sent.addresses[entry], bytes + at, 8u);
            std::memcpy(&sent.lengths[entry], bytes + at + 8u, 4u);
        }
    }

    MockPlatform           platform_;
    ContextRequestSequence sequence_;
    std::uint8_t           ring_view_[32] = {};
    std::uint8_t           fence_view_[32] = {};
    ContextPages           pages_;
};

/* Answers the setup's requests OK until `steps` have been sent. */
void answer_setup(Fixture &fixture, std::uint32_t steps)
{
    for (std::uint32_t step = 0u; step < steps; step++) {
        Sent sent;
        CHECK_STATUS(fixture.next(sent), Status::ok);
        CHECK_STATUS(fixture.answer(GpuResponse::ok_nodata), Status::ok);
    }
}

}  // namespace

TEST(context_request_sequence_setup, creates_the_context_its_three_blobs_then_sets_it_up)
{ Fixture fixture;
  CHECK_STATUS(fixture.sequence().begin_setup(context_id, setup(), fixture.pages()), Status::ok);
  CHECK_STATUS(fixture.sequence().begin_setup(context_id, setup(), fixture.pages()), Status::invalid_state);
  const std::uint32_t types[] = {0x0200u, 0x010Cu, 0x010Cu, 0x010Cu, 0x0207u};
  const std::uint32_t ids[] = {context_id, 70u, 71u, 72u, context_id};
  const std::uint64_t sizes[] = {0u, 3u * page, page, page, 70u};
  for (std::uint32_t step = 0u; step < 5u; step++) {
      CHECK(!fixture.sequence().finished());
      Sent sent;
      CHECK_STATUS(fixture.next(sent), Status::ok);
      CHECK_EQ(sent.type, types[step]);
      CHECK_EQ(sent.id, ids[step]);
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

/* Each blob carries its allocation's own frames, in order, one entry per run:
 * the scattered ring as its pair then its lone page, the fence and reply as
 * one page each. */
TEST(context_request_sequence_setup, each_blob_names_its_pages_one_entry_per_run)
{ Fixture fixture;
  CHECK_STATUS(fixture.sequence().begin_setup(context_id, setup(), fixture.pages()), Status::ok);
  answer_setup(fixture, 1u);
  Sent ring;
  CHECK_STATUS(fixture.next(ring), Status::ok);
  CHECK_EQ(ring.id, 70u);
  CHECK_EQ(ring.entries, 2u);
  CHECK(ring.addresses[0] == 0x81000u);
  CHECK_EQ(ring.lengths[0], 2u * page);
  CHECK(ring.addresses[1] == 0x2A000u);
  CHECK_EQ(ring.lengths[1], page);
  CHECK_STATUS(fixture.answer(GpuResponse::ok_nodata), Status::ok);
  Sent fence;
  CHECK_STATUS(fixture.next(fence), Status::ok);
  CHECK_EQ(fence.id, 71u);
  CHECK_EQ(fence.entries, 1u);
  CHECK(fence.addresses[0] == 0x44000u);
  CHECK_EQ(fence.lengths[0], page);
  CHECK_STATUS(fixture.answer(GpuResponse::ok_nodata), Status::ok);
  Sent reply;
  CHECK_STATUS(fixture.next(reply), Status::ok);
  CHECK_EQ(reply.id, 72u);
  CHECK_EQ(reply.entries, 1u);
  CHECK(reply.addresses[0] == 0x9F000u);
  CHECK_EQ(reply.lengths[0], page);
  CHECK(fixture.sequence().invariants_hold()); }

/* The pages are the caller's: a setup begun on them keeps what it was handed
 * even once the caller's own copy is gone. */
TEST(context_request_sequence_setup, keeps_the_pages_it_was_handed)
{ Fixture fixture;
  {
      const ContextPages copy = fixture.pages();
      CHECK_STATUS(fixture.sequence().begin_setup(context_id, setup(), copy), Status::ok);
  }
  answer_setup(fixture, 1u);
  Sent ring;
  CHECK_STATUS(fixture.next(ring), Status::ok);
  CHECK_EQ(ring.entries, 2u);
  CHECK(ring.addresses[1] == 0x2A000u); }

TEST(context_request_sequence_teardown, destroys_the_context_then_releases_its_blobs)
{ Fixture fixture;
  CHECK_STATUS(fixture.sequence().begin_teardown(), Status::invalid_state);   /* nothing set up */
  CHECK_STATUS(fixture.sequence().begin_setup(context_id, setup(), fixture.pages()), Status::ok);
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
      CHECK_STATUS(fixture.sequence().begin_setup(context_id, setup(), fixture.pages()), Status::ok);
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
  CHECK_STATUS(fixture.sequence().begin_setup(context_id, setup(), fixture.pages()), Status::ok);
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
  CHECK_STATUS(fixture.sequence().begin_setup(context_id, setup(), fixture.pages()), Status::ok);
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
  CHECK_STATUS(fixture.sequence().begin_setup(context_id, setup(), fixture.pages()), Status::ok); }

TEST(context_request_sequence_teardown, an_unreadable_answer_keeps_the_teardown_going)
{ Fixture fixture;
  CHECK_STATUS(fixture.sequence().begin_setup(context_id, setup(), fixture.pages()), Status::ok);
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
  CHECK_STATUS(fixture.sequence().begin_setup(context_id, setup(), fixture.pages()), Status::ok);
  Sent sent;
  CHECK_STATUS(fixture.next(sent), Status::ok);
  const std::uint8_t bytes[8] = {};
  CHECK_STATUS(fixture.sequence().accept(bytes, sizeof bytes), Status::malformed);
  CHECK(fixture.sequence().finished());
  CHECK(fixture.sequence().refused());
  CHECK_STATUS(fixture.sequence().begin_teardown(), Status::ok);   /* the context may exist */
  CHECK_STATUS(fixture.next(sent), Status::ok);
  CHECK_EQ(sent.type, 0x0201u); }

TEST(context_request_sequence_begin_setup, pages_not_complete_are_refused)
{ Fixture fixture;
  const std::uint64_t unmapped[] = {0u};
  std::uint8_t        view[32] = {};
  const ContextPages  hole(GuestPageList(ring_frames, 3u), GuestPageList(unmapped, 1u), GuestPageList(reply_frame, 1u),
                           view, view);
  CHECK_STATUS(fixture.sequence().begin_setup(context_id, setup(), ContextPages()), Status::invalid_parameter);
  CHECK_STATUS(fixture.sequence().begin_setup(context_id, setup(), hole), Status::invalid_parameter);
  Sent none;
  CHECK_STATUS(fixture.next(none), Status::invalid_state);   /* nothing begun */
  CHECK_STATUS(fixture.sequence().begin_setup(context_id, setup(), fixture.pages()), Status::ok); }

/* Both the fixed-size CTX_CREATE and the ring's two-entry blob (88 bytes) are
 * refused by a destination one byte short, write nothing, and are sent intact
 * once there is room. */
TEST(context_request_sequence_next_request, a_destination_too_small_writes_nothing)
{ Fixture fixture;
  CHECK_STATUS(fixture.sequence().begin_setup(context_id, setup(), fixture.pages()), Status::ok);
  std::uint8_t small[95] = {};
  std::memset(small, 0xEE, sizeof small);
  std::size_t length = 9u;
  CHECK_STATUS(fixture.sequence().next_request(small, sizeof small, length), Status::too_large);
  CHECK_EQ(length, 0u);
  CHECK_EQ(small[0], 0xEEu);
  answer_setup(fixture, 1u);
  length = 9u;
  CHECK_STATUS(fixture.sequence().next_request(small, 87u, length), Status::too_large);
  CHECK_EQ(length, 0u);
  CHECK_EQ(small[0], 0xEEu);
  CHECK_STATUS(fixture.sequence().next_request(small, 88u, length), Status::ok);
  CHECK_EQ(length, 88u);
  CHECK_EQ(small[0], 0x0Cu);
  CHECK_STATUS(fixture.answer(GpuResponse::ok_nodata), Status::ok); }

TEST(context_request_sequence, runs_at_dispatch_at_most)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::isr);
  CHECK_STATUS(fixture.sequence().begin_teardown(), Status::invalid_state);
  std::uint8_t bytes[request_capacity] = {};
  std::size_t  length = 0u;
  static_cast<void>(fixture.sequence().next_request(bytes, sizeof bytes, length));
  static_cast<void>(fixture.sequence().accept(bytes, 0u));
  static_cast<void>(fixture.sequence().begin_setup(context_id, setup(), fixture.pages()));
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
  CHECK_STATUS(fixture.sequence().begin_setup(context_id, setup(), fixture.pages()), Status::ok);
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

/* A refusal belongs to the setup it ended: once what it created is released,
 * the next setup starts unrefused and runs to the end. */
TEST(context_request_sequence_begin_setup, a_new_setup_forgets_the_last_refusal)
{ Fixture fixture;
  CHECK_STATUS(fixture.sequence().begin_setup(context_id, setup(), fixture.pages()), Status::ok);
  answer_setup(fixture, 1u);
  Sent sent;
  CHECK_STATUS(fixture.next(sent), Status::ok);
  CHECK_STATUS(fixture.answer(GpuResponse::err_out_of_memory), Status::not_supported);
  CHECK(fixture.sequence().refused());
  CHECK_STATUS(fixture.sequence().begin_teardown(), Status::ok);
  while (fixture.next(sent) == Status::ok) CHECK_STATUS(fixture.answer(GpuResponse::ok_nodata), Status::ok);
  CHECK_STATUS(fixture.sequence().begin_setup(context_id, setup(), fixture.pages()), Status::ok);
  CHECK(!fixture.sequence().refused());
  CHECK_EQ(fixture.sequence().refusal(), 0u);
  answer_setup(fixture, 5u);
  CHECK(fixture.sequence().finished());
  CHECK(!fixture.sequence().refused()); }
