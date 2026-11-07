#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "device/execution/answer_piece_command.hpp"
#include "device/execution/context_executor.hpp"
#include "device/execution/executor_library.hpp"
#include "device/execution/executor_shares.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/submission/command_header.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::AnswerPieceCommand;
using av::CommandHeader;
using av::ContextExecutor;
using av::Status;

namespace {

constexpr std::uint8_t uuid[16] = {0x5C};

class Fixture {
public:
    Fixture() : ops_(AV_EXECUTOR_SUB_API_VULKAN), library_(), shares_(host_, library_), executor_(host_, library_, shares_, 2u)
    {
        static_cast<void>(library_.accept(&ops_.table()));
        static_cast<void>(library_.open_domain(uuid, 0u, 0u, 0u));
        static_cast<void>(shares_.initialize());
        executor_.configure(av::SubApi::vulkan, uuid, av::GlContextOptions());
    }

    template <typename Body>
    void on_worker(Body body)
    {
        struct Call {
            Fixture *fixture;
            Body    *body;
        } call{this, &body};
        void *thread = nullptr;
        CHECK_STATUS(host_.start_thread([](void *context) {
                         auto *const c = static_cast<Call *>(context);
                         (*c->body)(*c->fixture);
                     },
                                        &call, thread),
                     Status::ok);
        host_.join_thread(thread);
    }

    ContextExecutor &executor() { return executor_; }
    Status run(std::uint64_t offset, std::uint32_t body_bytes, std::uint8_t flags, std::size_t capacity)
    {
        std::uint8_t body[16] = {};
        std::memcpy(body, &offset, sizeof offset);
        const CommandHeader header(CommandHeader::answer_piece_id, CommandHeader::encoded_size + body_bytes, flags);
        return AnswerPieceCommand(executor_, header, body).run(capacity != 0u ? reply_ : nullptr, capacity, produced_);
    }
    std::size_t         produced() const { return produced_; }
    const std::uint8_t *reply() const { return reply_; }

private:
    av::MockHost        host_;
    av::MockExecutorOps ops_;
    av::ExecutorLibrary library_;
    av::ExecutorShares  shares_;
    ContextExecutor     executor_;
    std::uint8_t        reply_[24] = {};
    std::size_t         produced_ = 0u;
};

}

TEST(answer_piece_command_run, hands_on_the_kept_answer_from_an_offset_behind_its_length)
{ Fixture fixture;
  fixture.on_worker([](Fixture &f) {
      CHECK_STATUS(f.executor().create(), Status::ok);
      std::uint8_t payload[40] = {};
      for (std::uint8_t index = 0u; index < sizeof payload; index++) payload[index] = index;
      std::uint8_t small[8] = {};
      std::size_t  reply_size = 0u;
      CHECK_STATUS(f.executor().execute(0x10001u, payload, sizeof payload, small, sizeof small, reply_size),
                   Status::ok);
      CHECK_STATUS(f.run(30u, 8u, CommandHeader::generate_reply, 24u), Status::ok);
      CHECK_EQ(f.produced(), 8u + 10u);
      std::uint64_t length = 0u;
      std::memcpy(&length, f.reply(), sizeof length);
      CHECK_EQ(length, 40u);
      CHECK_EQ(f.reply()[8], 30u);
      CHECK_EQ(f.reply()[17], 39u);
      CHECK_STATUS(f.run(0u, 8u, CommandHeader::generate_reply, 24u), Status::ok);
      CHECK_EQ(f.produced(), 24u);
      CHECK_EQ(f.reply()[23], 15u);
      f.executor().destroy();
  }); }

TEST(answer_piece_command_run, a_body_of_another_length_or_no_answer_asked_is_framing)
{ Fixture fixture;
  fixture.on_worker([](Fixture &f) {
      CHECK_STATUS(f.executor().create(), Status::ok);
      CHECK_STATUS(f.run(0u, 0u, CommandHeader::generate_reply, 24u), Status::malformed);
      CHECK_STATUS(f.run(0u, 9u, CommandHeader::generate_reply, 24u), Status::malformed);
      CHECK_STATUS(f.run(0u, 8u, 0u, 24u), Status::malformed);
      CHECK_STATUS(f.run(0u, 8u, CommandHeader::generate_reply, 0u), Status::malformed);
      CHECK_STATUS(f.run(0u, 8u, CommandHeader::generate_reply, 7u), Status::malformed);
      CHECK_EQ(f.produced(), 0u);
      CHECK_STATUS(f.run(0u, 8u, CommandHeader::generate_reply, 24u), Status::ok);
      CHECK_EQ(f.produced(), 8u);
      f.executor().destroy();
  }); }
