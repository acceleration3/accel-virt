/* QEMU's end of the renderer socket: every message framed with its kind and a
 * sequence of its own, an answer found by that sequence and nothing else,
 * every descriptor that came with anything else closed, and every wait
 * bounded by the clock. */
#include "core/support/status.hpp"
#include "device/abi/rvk_executor.h"
#include "device/isolation/renderer_link.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/scripted_renderer_peer.hpp"
#include "proto/renderer/context_message.hpp"
#include "proto/renderer/ready_message.hpp"
#include "proto/renderer/renderer_message_header.hpp"
#include "proto/renderer/reply_message.hpp"

#include <cstdint>
#include <cstring>

using av::MockHost;
using av::RendererLink;
using av::RendererMessageHeader;
using av::ScriptedRendererPeer;
using av::Status;
using Kind = av::RendererMessageHeader::Kind;

namespace {

constexpr std::uint64_t millisecond = 1000000u;

class Fixture {
public:
    Fixture() : peer_(host_), link_(host_)
    {
        host_.connect_renderer(peer_);
        static_cast<void>(host_.start_renderer("rvk-renderer"));
    }

    MockHost             &host() { return host_; }
    ScriptedRendererPeer &peer() { return peer_; }
    RendererLink         &link() { return link_; }

    /* A create_context request for context 5, its outcome in `outcome`. */
    Status create(Status &outcome)
    {
        std::uint8_t body[av::ContextMessage::encoded_size] = {};
        static_cast<void>(av::ContextMessage(5u).encode(body, sizeof body));
        return link_.request(Kind::create_context, body, sizeof body, nullptr, 0u, outcome);
    }

private:
    MockHost             host_;
    ScriptedRendererPeer peer_;
    RendererLink         link_;
};

}  // namespace

/* Each message carries the next sequence, whatever its kind, and its body
 * byte for byte; its descriptors reach the renderer as its own. */
TEST(renderer_link_post, frames_each_message_with_the_next_sequence_and_its_body)
{ Fixture fixture;
  const std::uint8_t body[3] = {0xA1u, 0xB2u, 0xC3u};
  const int          sent = fixture.host().open_descriptor();
  CHECK_STATUS(fixture.link().post(Kind::region, body, sizeof body, &sent, 1u), Status::ok);
  CHECK_STATUS(fixture.link().post(Kind::interrupts_off, nullptr, 0u, nullptr, 0u), Status::ok);
  CHECK_EQ(fixture.peer().delivered(), 2u);
  CHECK(fixture.peer().kind_at(0u) == Kind::region);
  CHECK_EQ(fixture.peer().sequence_at(0u), 1u);
  CHECK_EQ(fixture.peer().body_length_at(0u), 3u);
  CHECK(std::memcmp(fixture.peer().body_at(0u), body, sizeof body) == 0);
  CHECK_EQ(fixture.peer().descriptors_at(0u), 1u);
  CHECK_EQ(fixture.peer().open_descriptors_at(0u), 1u);
  CHECK(fixture.peer().kind_at(1u) == Kind::interrupts_off);
  CHECK_EQ(fixture.peer().sequence_at(1u), 2u);
  CHECK_EQ(fixture.peer().body_length_at(1u), 0u);
  /* The caller's descriptor is still the caller's. */
  CHECK(fixture.host().descriptor_open(sent));
  fixture.host().close_descriptor(sent);
  CHECK_EQ(fixture.peer().queued(), 0u); }

/* A body past the largest a message has, or bytes promised with no bytes,
 * is refused before anything is sent - and the refusal spends no sequence. */
TEST(renderer_link_post, refuses_a_body_too_long_or_missing)
{ Fixture fixture;
  static std::uint8_t body[RendererMessageHeader::max_body_length + 1u];
  CHECK_STATUS(fixture.link().post(Kind::guest_blob, body, RendererMessageHeader::max_body_length + 1u, nullptr, 0u),
               Status::invalid_parameter);
  CHECK_STATUS(fixture.link().post(Kind::region, nullptr, 1u, nullptr, 0u), Status::invalid_parameter);
  CHECK_EQ(fixture.peer().delivered(), 0u);
  CHECK_STATUS(fixture.link().post(Kind::guest_blob, body, RendererMessageHeader::max_body_length, nullptr, 0u),
               Status::ok);
  CHECK_EQ(fixture.peer().delivered(), 1u);
  CHECK_EQ(fixture.peer().sequence_at(0u), 1u); }

TEST(renderer_link_post, a_renderer_that_is_gone_is_not_found)
{ Fixture fixture;
  fixture.host().renderer_gone();
  CHECK_STATUS(fixture.link().post(Kind::interrupts_off, nullptr, 0u, nullptr, 0u), Status::not_found);
  CHECK_EQ(fixture.peer().delivered(), 0u); }

/* The renderer's outcome is the request's, a refusal as much as a success;
 * the link itself answered. */
TEST(renderer_link_request, answers_with_the_outcome_the_renderer_replied)
{ Fixture fixture;
  Status outcome = Status::invalid_state;
  CHECK_STATUS(fixture.create(outcome), Status::ok);
  CHECK_STATUS(outcome, Status::ok);
  fixture.peer().answer_requests_with(Status::conflict);
  CHECK_STATUS(fixture.create(outcome), Status::ok);
  CHECK_STATUS(outcome, Status::conflict);
  CHECK_EQ(fixture.peer().delivered(), 2u);
  CHECK_EQ(fixture.peer().sequence_at(1u), 2u);
  /* A request's wait is the request's bound, to the millisecond. */
  CHECK_EQ(fixture.host().last_receive_timeout(), RendererLink::request_timeout_ms); }

TEST(renderer_link_request, a_renderer_gone_before_the_send_or_during_the_wait_is_not_found)
{ Fixture fixture;
  Status outcome = Status::ok;
  fixture.host().renderer_gone();
  CHECK_STATUS(fixture.create(outcome), Status::not_found);
  CHECK_STATUS(outcome, Status::invalid_state);
  CHECK_EQ(fixture.peer().delivered(), 0u);
  CHECK_STATUS(fixture.host().start_renderer("rvk-renderer"), Status::ok);
  fixture.peer().die_on_next_delivery();
  outcome = Status::ok;
  CHECK_STATUS(fixture.create(outcome), Status::not_found);
  CHECK_STATUS(outcome, Status::invalid_state);
  CHECK_EQ(fixture.peer().delivered(), 1u); }

TEST(renderer_link_request, a_silent_renderer_is_invalid_state)
{ Fixture fixture;
  fixture.peer().fall_silent();
  Status outcome = Status::ok;
  CHECK_STATUS(fixture.create(outcome), Status::invalid_state);
  CHECK_STATUS(outcome, Status::invalid_state); }

/* A reply to a request that already timed out, an answer of the wrong kind,
 * bytes that are no message: each is dropped, with every descriptor it
 * carried closed, and the answer behind them is the one taken. */
TEST(renderer_link_request, skips_stale_replies_other_kinds_and_garbage_closing_their_descriptors)
{ Fixture fixture;
  fixture.peer().queue_reply(7u, Status::no_memory, 2u);   /* a sequence this link never used */
  std::uint8_t ready[RendererMessageHeader::encoded_size + av::ReadyMessage::encoded_size] = {};
  const std::uint32_t no_kinds[av::ReadyMessage::lanes] = {};
  CHECK_STATUS(RendererMessageHeader(Kind::ready, av::ReadyMessage::encoded_size, 1u).encode(ready, sizeof ready),
               Status::ok);
  CHECK_STATUS(av::ReadyMessage(1u, 1u, 1u, no_kinds).encode(ready + RendererMessageHeader::encoded_size,
                                                             av::ReadyMessage::encoded_size),
               Status::ok);
  fixture.peer().queue(ready, sizeof ready, 1u);            /* the right sequence, the wrong kind */
  const std::uint8_t garbage[5] = {0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu};
  fixture.peer().queue(garbage, sizeof garbage, 3u);
  fixture.peer().answer_requests_with(Status::not_supported);
  Status outcome = Status::ok;
  CHECK_STATUS(fixture.create(outcome), Status::ok);
  CHECK_STATUS(outcome, Status::not_supported);
  CHECK_EQ(fixture.peer().queued(), 0u);
  CHECK_EQ(fixture.host().live_dmabufs(), 0u);   /* all six closed */
  CHECK_EQ(fixture.host().unclaimed_violations(), 0u); }

/* Even the awaited reply's descriptors are closed: no reply carries any. */
TEST(renderer_link_request, closes_descriptors_that_came_with_the_awaited_reply)
{ Fixture fixture;
  fixture.peer().fall_silent();
  fixture.peer().queue_reply(1u, Status::ok, 2u);
  Status outcome = Status::invalid_state;
  CHECK_STATUS(fixture.create(outcome), Status::ok);
  CHECK_STATUS(outcome, Status::ok);
  CHECK_EQ(fixture.host().live_dmabufs(), 0u); }

/* A reply of the right kind and sequence whose body does not decode, or
 * names another request, is something no renderer sends. */
TEST(renderer_link_request, a_reply_that_does_not_decode_or_names_another_request_is_malformed)
{ { Fixture fixture;
    fixture.peer().fall_silent();
    std::uint8_t bad[RendererMessageHeader::encoded_size + av::ReplyMessage::fixed_size] = {};
    CHECK_STATUS(RendererMessageHeader(Kind::reply, av::ReplyMessage::fixed_size, 1u).encode(bad, sizeof bad),
                 Status::ok);
    std::memset(bad + RendererMessageHeader::encoded_size, 0xEE, av::ReplyMessage::fixed_size);
    fixture.peer().queue(bad, sizeof bad, 0u);
    Status outcome = Status::ok;
    CHECK_STATUS(fixture.create(outcome), Status::malformed);
    CHECK_STATUS(outcome, Status::invalid_state); }
  Fixture fixture;
  fixture.peer().fall_silent();
  fixture.peer().queue_reply(1u, 2u, Status::ok);
  Status outcome = Status::ok;
  CHECK_STATUS(fixture.create(outcome), Status::malformed);
  CHECK_STATUS(outcome, Status::invalid_state); }

/* The deadline is the clock's, not a count of datagrams: with the clock a
 * second on at every reading, the first stale reply is taken at one second
 * and the wait ends at two with the rest still unread. */
TEST(renderer_link_request, the_wait_ends_at_its_deadline_however_much_the_renderer_says)
{ Fixture fixture;
  fixture.peer().fall_silent();
  for (std::uint64_t stale = 10u; stale < 13u; stale++) fixture.peer().queue_reply(stale, Status::ok, 0u);
  fixture.host().advance_clock_per_read(1000u * millisecond);
  Status outcome = Status::ok;
  CHECK_STATUS(fixture.create(outcome), Status::invalid_state);
  CHECK_EQ(fixture.peer().queued(), 2u);
  /* What was left of two seconds a second in, rounded up to whole
   * milliseconds so a wait is never cut short. */
  CHECK_EQ(fixture.host().last_receive_timeout(), 1000u); }

TEST(renderer_link_request, a_wait_is_given_what_is_left_rounded_up_to_the_millisecond)
{ Fixture fixture;
  fixture.host().advance_clock_per_read(millisecond + millisecond / 2u);
  Status outcome = Status::invalid_state;
  CHECK_STATUS(fixture.create(outcome), Status::ok);
  CHECK_STATUS(outcome, Status::ok);
  /* 2000 ms less 1.5 ms is 1998.5 ms: the wait is given 1999. */
  CHECK_EQ(fixture.host().last_receive_timeout(), RendererLink::request_timeout_ms - 1u); }

TEST(renderer_link_request, refuses_a_body_it_cannot_frame_without_sending)
{ Fixture fixture;
  Status outcome = Status::ok;
  CHECK_STATUS(fixture.link().request(Kind::create_context, nullptr, 4u, nullptr, 0u, outcome),
               Status::invalid_parameter);
  CHECK_STATUS(outcome, Status::invalid_state);
  CHECK_EQ(fixture.peer().delivered(), 0u); }

/* The hello carries no descriptor and waits the hello's longer bound; the
 * Ready is decoded whole. */
TEST(renderer_link_greet, sends_the_hello_and_decodes_the_ready)
{ Fixture fixture;
  const std::uint32_t kinds[av::ReadyMessage::lanes] = {RVK_BLOB_HOST_POINTER, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
  fixture.peer().ready_with(av::ReadyMessage(1u, 6u, 5u, kinds));
  const std::uint8_t body[4] = {1u, 2u, 3u, 4u};
  av::ReadyMessage   ready;
  CHECK_STATUS(fixture.link().greet(body, sizeof body, ready), Status::ok);
  CHECK(fixture.peer().kind_at(0u) == Kind::hello);
  CHECK_EQ(fixture.peer().descriptors_at(0u), 0u);
  CHECK_EQ(fixture.peer().body_length_at(0u), 4u);
  CHECK_EQ(fixture.host().last_receive_timeout(), RendererLink::hello_timeout_ms);
  CHECK_EQ(ready.version(), 1u);
  CHECK_EQ(ready.executor_abi(), 6u);
  CHECK_EQ(ready.sub_apis(), 5u);
  CHECK_EQ(ready.import_kinds(0u), static_cast<std::uint32_t>(RVK_BLOB_HOST_POINTER)); }

TEST(renderer_link_greet, a_silent_gone_or_garbled_renderer_is_refused)
{ { Fixture fixture;
    fixture.peer().fall_silent();
    av::ReadyMessage ready;
    const std::uint8_t body[1] = {1u};
    CHECK_STATUS(fixture.link().greet(body, sizeof body, ready), Status::invalid_state); }
  { Fixture fixture;
    fixture.host().renderer_gone();
    av::ReadyMessage ready;
    const std::uint8_t body[1] = {1u};
    CHECK_STATUS(fixture.link().greet(body, sizeof body, ready), Status::not_found); }
  { Fixture fixture;
    av::ReadyMessage ready;
    CHECK_STATUS(fixture.link().greet(nullptr, 1u, ready), Status::invalid_parameter);
    CHECK_EQ(fixture.peer().delivered(), 0u); }
  Fixture fixture;
  fixture.peer().fall_silent();
  /* A Ready of the right sequence whose body is a byte short. */
  std::uint8_t short_ready[RendererMessageHeader::encoded_size + av::ReadyMessage::encoded_size - 1u] = {};
  CHECK_STATUS(RendererMessageHeader(Kind::ready, av::ReadyMessage::encoded_size - 1u, 1u)
                   .encode(short_ready, sizeof short_ready),
               Status::ok);
  fixture.peer().queue(short_ready, sizeof short_ready, 0u);
  av::ReadyMessage ready;
  const std::uint8_t body[1] = {1u};
  CHECK_STATUS(fixture.link().greet(body, sizeof body, ready), Status::malformed);
  CHECK_EQ(ready.version(), 0u); }

/* An exchange keeps what came with its answer, up to what the caller has
 * room for; the rest, and whatever came with anything else, is closed. */
TEST(renderer_link_exchange, keeps_the_answers_descriptors_up_to_capacity_and_closes_the_rest)
{ Fixture fixture;
  std::uint8_t       stale[RendererMessageHeader::encoded_size + 4u] = {};
  CHECK_STATUS(RendererMessageHeader(Kind::exported_image, 4u, 9u).encode(stale, sizeof stale), Status::ok);
  fixture.peer().queue(stale, sizeof stale, 2u);   /* another sequence's */
  std::uint8_t answer_bytes[RendererMessageHeader::encoded_size + 4u] = {};
  CHECK_STATUS(RendererMessageHeader(Kind::exported_image, 4u, 1u).encode(answer_bytes, sizeof answer_bytes),
               Status::ok);
  answer_bytes[RendererMessageHeader::encoded_size] = 0x6Bu;
  fixture.peer().queue(answer_bytes, sizeof answer_bytes, 3u);
  const std::uint8_t body[8] = {1u};
  const std::uint8_t *answer = nullptr;
  std::uint32_t       length = 0u;
  int                 kept[2] = {-1, -1};
  std::uint32_t       count = 9u;
  CHECK_STATUS(fixture.link().exchange(Kind::export_image, body, sizeof body, Kind::exported_image, answer, length,
                                       kept, 2u, count),
               Status::ok);
  CHECK(fixture.peer().kind_at(0u) == Kind::export_image);
  CHECK_EQ(fixture.peer().descriptors_at(0u), 0u);
  CHECK_EQ(length, 4u);
  CHECK_EQ(answer[0], 0x6Bu);
  CHECK_EQ(count, 2u);
  CHECK(fixture.host().descriptor_open(kept[0]));
  CHECK(fixture.host().descriptor_open(kept[1]));
  CHECK_EQ(fixture.host().live_dmabufs(), 2u);   /* the stale two and the third closed */
  fixture.host().close_descriptor(kept[0]);
  fixture.host().close_descriptor(kept[1]);
  CHECK_EQ(fixture.host().unclaimed_violations(), 0u); }

TEST(renderer_link_exchange, fails_as_a_request_does)
{ { Fixture fixture;
    fixture.host().renderer_gone();
    const std::uint8_t *answer = nullptr;
    std::uint32_t       length = 0u, count = 5u;
    CHECK_STATUS(fixture.link().exchange(Kind::export_image, nullptr, 0u, Kind::exported_image, answer, length, nullptr,
                                         0u, count),
                 Status::not_found);
    CHECK_EQ(count, 0u); }
  { Fixture fixture;
    const std::uint8_t *answer = nullptr;
    std::uint32_t       length = 0u, count = 0u;
    CHECK_STATUS(fixture.link().exchange(Kind::export_image, nullptr, 3u, Kind::exported_image, answer, length, nullptr,
                                         0u, count),
                 Status::invalid_parameter);
    CHECK_EQ(fixture.peer().delivered(), 0u); }
  Fixture fixture;
  const std::uint8_t *answer = nullptr;
  std::uint32_t       length = 0u, count = 0u;
  CHECK_STATUS(fixture.link().exchange(Kind::export_image, nullptr, 0u, Kind::exported_image, answer, length, nullptr, 0u,
                                       count),
               Status::invalid_state); }
