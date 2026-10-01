/* SHARE_OBJECT, OPEN_SHARED_OBJECT and RELEASE_SHARED_OBJECT as a guest frames
 * them (docs/protocol.md, Transport commands).
 *
 * Two rules divide this file. A body of the wrong length is *framing* - no
 * producer writes one, so the context stops and nothing is answered. Anything
 * else is an answer with a status in it, and the context runs on: a guest that
 * asks for something the device will not do is a guest making a mistake, not
 * an attack on the frame. Every status in data-model.md has a case here. */
#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "device/execution/shared_object_command.hpp"
#include "harness/executor/shared_object_rig.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/shared_object_answer.hpp"
#include "proto/submission/shared_object_request.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::CommandHeader;
using av::SharedObjectAnswer;
using av::SharedObjectCommand;
using av::SharedObjectKind;
using av::SharedObjectRequest;
using av::SharedObjectRig;
using av::Status;

namespace {

constexpr std::uint64_t size = 8u * 4096u;
constexpr std::uint64_t made_by_guest = 0x40u;

/* One command sent by one context of the rig, with the answer it wrote. */
class Sent {
public:
    Sent(av::SharedObjectContext &context, std::uint32_t id, const SharedObjectRequest &request,
         std::size_t body_bytes = SharedObjectRequest::encoded_size, std::size_t capacity = 32u)
        : ran_(Status::invalid_state), produced_(99u), reply_{}
    {
        std::uint8_t body[SharedObjectRequest::encoded_size] = {};
        CHECK_STATUS(request.encode(body, sizeof body), Status::ok);
        const CommandHeader header(id, static_cast<std::uint32_t>(CommandHeader::encoded_size + body_bytes),
                                   0u);
        context.on_worker([&] {
            ran_ = SharedObjectCommand(context.objects(), header, body).run(reply_, capacity, produced_);
        });
    }

    Status      ran() const { return ran_; }
    std::size_t produced() const { return produced_; }
    std::uint32_t status() const
    {
        SharedObjectAnswer answer;
        if (answer.decode(reply_, SharedObjectAnswer::encoded_size) != Status::ok) {
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "answer does not decode");
            return 0xFFFFFFFFu;
        }
        return answer.status();
    }
    std::uint64_t wire_handle() const
    {
        SharedObjectAnswer answer;
        static_cast<void>(answer.decode(reply_, SharedObjectAnswer::encoded_size));
        return answer.wire_handle();
    }

private:
    Status       ran_;
    std::size_t  produced_;
    std::uint8_t reply_[32];
};

SharedObjectRequest memory(std::uint64_t id, std::uint64_t wire_handle)
{
    return SharedObjectRequest(id, SharedObjectKind(SharedObjectKind::memory), wire_handle, size);
}

void check(SharedObjectRig &rig)
{
    if (!rig.invariants_hold() || !rig.first().objects().invariants_hold() ||
        !rig.second().objects().invariants_hold())
        av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "invariants broken");
}

}  // namespace

TEST(shared_object_command_run, a_share_then_an_open_from_another_context_names_one_host_object)
{ SharedObjectRig rig;
  const Sent shared(rig.first(), CommandHeader::share_object_id, memory(7u, made_by_guest));
  CHECK_STATUS(shared.ran(), Status::ok);
  CHECK_EQ(shared.produced(), SharedObjectAnswer::encoded_size);
  CHECK_EQ(shared.status(), SharedObjectAnswer::done);
  CHECK_EQ(shared.wire_handle(), 0u);
  check(rig);

  const Sent opened(rig.second(), CommandHeader::open_shared_object_id, memory(7u, 0u));
  CHECK_STATUS(opened.ran(), Status::ok);
  CHECK_EQ(opened.status(), SharedObjectAnswer::done);
  CHECK(opened.wire_handle() != 0u);
  check(rig);
  CHECK_EQ(rig.shared().live(), 1u);
  CHECK_EQ(rig.ops().exported_objects(), 1u);
  CHECK_EQ(rig.ops().imported_objects(), 1u); }

TEST(shared_object_command_run, a_release_destroys_what_this_context_made_and_answers_done)
{ SharedObjectRig rig;
  CHECK_EQ(Sent(rig.first(), CommandHeader::share_object_id, memory(7u, made_by_guest)).status(),
           SharedObjectAnswer::done);
  const Sent opened(rig.second(), CommandHeader::open_shared_object_id, memory(7u, 0u));
  CHECK_EQ(opened.status(), SharedObjectAnswer::done);
  const Sent released(rig.second(), CommandHeader::release_shared_object_id, memory(7u, 0u));
  CHECK_STATUS(released.ran(), Status::ok);
  CHECK_EQ(released.status(), SharedObjectAnswer::done);
  CHECK_EQ(released.wire_handle(), 0u);
  check(rig);
  CHECK_EQ(rig.ops().released_objects(), 1u);
  CHECK_EQ(rig.shared().live(), 1u);
  /* An id this context never held answers done and does nothing, as
   * RELEASE_BLOB does. */
  const Sent again(rig.second(), CommandHeader::release_shared_object_id, memory(9u, 0u));
  CHECK_EQ(again.status(), SharedObjectAnswer::done);
  check(rig);
  CHECK_EQ(rig.ops().released_objects(), 1u); }

TEST(shared_object_command_run, a_body_of_any_length_but_its_own_is_framing_and_answers_nothing)
{ SharedObjectRig rig;
  const std::uint32_t ids[] = {CommandHeader::share_object_id, CommandHeader::open_shared_object_id,
                               CommandHeader::release_shared_object_id};
  const std::size_t   lengths[] = {0u, 8u, 16u, 24u, 31u, 33u, 40u};
  for (const std::uint32_t id : ids)
      for (const std::size_t length : lengths) {
          const Sent sent(rig.first(), id, memory(7u, made_by_guest), length);
          CHECK_STATUS(sent.ran(), Status::malformed);
          CHECK_EQ(sent.produced(), 0u);
      }
  check(rig);
  CHECK_EQ(rig.shared().live(), 0u); }

TEST(shared_object_command_run, an_id_that_is_not_one_of_the_three_is_not_known)
{ SharedObjectRig rig;
  const std::uint32_t ids[] = {0u, CommandHeader::nop_id, CommandHeader::release_blob_id, 7u, 0xFFFFu};
  for (const std::uint32_t id : ids) {
      const Sent sent(rig.first(), id, memory(7u, made_by_guest));
      CHECK_STATUS(sent.ran(), Status::not_found);
      CHECK_EQ(sent.produced(), 0u);
  } }

TEST(shared_object_command_run, a_body_no_request_can_be_is_refused_as_a_request_and_the_context_runs_on)
{ SharedObjectRig rig;
  /* An id of 0, and a kind this protocol does not have. */
  const SharedObjectRequest bodies[] = {
      SharedObjectRequest(0u, SharedObjectKind(SharedObjectKind::memory), made_by_guest, size),
      SharedObjectRequest(7u, SharedObjectKind(4u), made_by_guest, size),
      /* Memory with no length, and a fence carrying one. */
      SharedObjectRequest(7u, SharedObjectKind(SharedObjectKind::memory), made_by_guest, 0u),
      SharedObjectRequest(7u, SharedObjectKind(SharedObjectKind::native_fence), made_by_guest, size),
  };
  for (const SharedObjectRequest &body : bodies) {
      const Sent sent(rig.first(), CommandHeader::share_object_id, body);
      CHECK_STATUS(sent.ran(), Status::ok);
      CHECK_EQ(sent.status(), SharedObjectAnswer::refused_request);
      CHECK_EQ(sent.wire_handle(), 0u);
  }
  check(rig);
  CHECK_EQ(rig.shared().live(), 0u);
  CHECK_EQ(rig.ops().exported_objects(), 0u); }

TEST(shared_object_command_run, only_a_share_may_carry_a_wire_handle)
{ SharedObjectRig rig;
  CHECK_EQ(Sent(rig.first(), CommandHeader::share_object_id, memory(7u, made_by_guest)).status(),
           SharedObjectAnswer::done);
  const std::uint32_t ids[] = {CommandHeader::open_shared_object_id,
                               CommandHeader::release_shared_object_id};
  for (const std::uint32_t id : ids) {
      const Sent sent(rig.second(), id, memory(7u, made_by_guest));
      CHECK_STATUS(sent.ran(), Status::ok);
      CHECK_EQ(sent.status(), SharedObjectAnswer::refused_request);
  }
  check(rig);
  /* And a share that names no object is a share of nothing. */
  const Sent nothing(rig.second(), CommandHeader::share_object_id, memory(9u, 0u));
  CHECK_EQ(nothing.status(), SharedObjectAnswer::refused_request);
  check(rig);
  CHECK_EQ(rig.shared().live(), 1u); }

TEST(shared_object_command_run, an_open_of_an_id_the_device_never_published_is_an_unknown_object)
{ SharedObjectRig rig;
  const Sent opened(rig.second(), CommandHeader::open_shared_object_id, memory(7u, 0u));
  CHECK_STATUS(opened.ran(), Status::ok);
  CHECK_EQ(opened.status(), SharedObjectAnswer::unknown_object);
  CHECK_EQ(opened.wire_handle(), 0u);
  check(rig); }

TEST(shared_object_command_run, an_open_whose_kind_or_size_disagrees_is_a_mismatched_object)
{ SharedObjectRig rig;
  CHECK_EQ(Sent(rig.first(), CommandHeader::share_object_id, memory(7u, made_by_guest)).status(),
           SharedObjectAnswer::done);
  const SharedObjectRequest wrong[] = {
      SharedObjectRequest(7u, SharedObjectKind(SharedObjectKind::monitored_fence), 0u, 0u),
      SharedObjectRequest(7u, SharedObjectKind(SharedObjectKind::memory), 0u, size + 4096u),
  };
  for (const SharedObjectRequest &body : wrong) {
      const Sent sent(rig.second(), CommandHeader::open_shared_object_id, body);
      CHECK_STATUS(sent.ran(), Status::ok);
      CHECK_EQ(sent.status(), SharedObjectAnswer::mismatched_object);
      CHECK_EQ(sent.wire_handle(), 0u);
  }
  check(rig);
  CHECK_EQ(rig.ops().imported_objects(), 0u); }

TEST(shared_object_command_run, an_executor_that_will_not_export_or_import_is_an_executor_refusal)
{ SharedObjectRig rig;
  rig.ops().refuse_export_at(1u);
  const Sent refused(rig.first(), CommandHeader::share_object_id, memory(7u, made_by_guest));
  CHECK_STATUS(refused.ran(), Status::ok);
  CHECK_EQ(refused.status(), SharedObjectAnswer::executor_refused);
  check(rig);

  CHECK_EQ(Sent(rig.first(), CommandHeader::share_object_id, memory(7u, made_by_guest)).status(),
           SharedObjectAnswer::done);
  rig.ops().refuse_object_import_at(1u);
  const Sent opened(rig.second(), CommandHeader::open_shared_object_id, memory(7u, 0u));
  CHECK_EQ(opened.status(), SharedObjectAnswer::executor_refused);
  CHECK_EQ(opened.wire_handle(), 0u);
  check(rig);
  CHECK_EQ(rig.shared().live(), 1u); }

TEST(shared_object_command_run, a_library_from_before_the_sharing_version_refuses_every_one_of_them)
{ SharedObjectRig rig;
  rig.ops().set_abi_version(AV_EXECUTOR_ABI_VERSION_EVENTS);
  /* The library was accepted at its newest version, so this is the device
   * finding out mid-flight what a real one says at accept: either way the
   * guest is answered, not faulted. */
  const Sent shared(rig.first(), CommandHeader::share_object_id, memory(7u, made_by_guest));
  CHECK_STATUS(shared.ran(), Status::ok);
  CHECK_EQ(shared.status(), SharedObjectAnswer::executor_refused);
  check(rig);
  CHECK_EQ(rig.shared().live(), 0u); }

TEST(shared_object_command_run, a_second_publisher_of_one_id_and_a_second_open_by_one_context_conflict)
{ SharedObjectRig rig;
  CHECK_EQ(Sent(rig.first(), CommandHeader::share_object_id, memory(7u, made_by_guest)).status(),
           SharedObjectAnswer::done);
  const Sent twice(rig.second(), CommandHeader::share_object_id, memory(7u, made_by_guest));
  CHECK_STATUS(twice.ran(), Status::ok);
  CHECK_EQ(twice.status(), SharedObjectAnswer::conflict);
  check(rig);

  CHECK_EQ(Sent(rig.second(), CommandHeader::open_shared_object_id, memory(7u, 0u)).status(),
           SharedObjectAnswer::done);
  const Sent again(rig.second(), CommandHeader::open_shared_object_id, memory(7u, 0u));
  CHECK_EQ(again.status(), SharedObjectAnswer::conflict);
  CHECK_EQ(again.wire_handle(), 0u);
  check(rig);
  CHECK_EQ(rig.ops().imported_objects(), 1u); }

TEST(shared_object_command_run, a_context_holding_all_it_can_has_no_room)
{ SharedObjectRig rig;
  for (std::uint64_t id = 1u; id <= av::ContextSharedObjects::max_objects; id++)
      CHECK_EQ(Sent(rig.first(), CommandHeader::share_object_id, memory(id, made_by_guest)).status(),
               SharedObjectAnswer::done);
  check(rig);
  const Sent full(rig.first(), CommandHeader::share_object_id, memory(0xFFFFu, made_by_guest));
  CHECK_STATUS(full.ran(), Status::ok);
  CHECK_EQ(full.status(), SharedObjectAnswer::no_room);
  check(rig); }

TEST(shared_object_command_run, an_answer_slot_too_small_is_written_nothing_and_still_says_what_it_needed)
{ SharedObjectRig rig;
  const Sent shared(rig.first(), CommandHeader::share_object_id, memory(7u, made_by_guest),
                    SharedObjectRequest::encoded_size, SharedObjectAnswer::encoded_size - 1u);
  CHECK_STATUS(shared.ran(), Status::ok);
  CHECK_EQ(shared.produced(), SharedObjectAnswer::encoded_size);
  check(rig);
  /* The command still did what it was asked; only the answer did not fit. */
  CHECK_EQ(rig.shared().live(), 1u);
  CHECK_EQ(rig.ops().exported_objects(), 1u); }
