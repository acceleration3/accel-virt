#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "device/execution/channel_stats_command.hpp"
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

using av::ChannelStatsCommand;
using av::CommandHeader;
using av::ContextExecutor;
using av::Status;

namespace {

constexpr std::uint8_t uuid[16] = {0x6C};

struct Seen {
    std::uint32_t calls = 0u;
    std::uint32_t context = 0u;
    std::size_t   size = 0u;
    std::uint8_t  body[16] = {};
};

Seen seen;

void record(void *, AvExecutor *, std::uint32_t context, const std::uint8_t *body, std::size_t size)
{
    seen.calls++;
    seen.context = context;
    seen.size = size;
    if (size <= sizeof seen.body) std::memcpy(seen.body, body, size);
}

class Fixture {
public:
    Fixture() : ops_(AV_EXECUTOR_SUB_API_D3D), library_(), shares_(host_, library_), executor_(host_, library_, shares_, 3u)
    {
        seen = Seen();
        ops_.editable().channel_stats = record;
        static_cast<void>(library_.accept(&ops_.table()));
        static_cast<void>(library_.open_domain(uuid));
        static_cast<void>(shares_.initialize());
        executor_.configure(av::SubApi::d3d, uuid, av::GlContextOptions());
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
    Status           run(std::uint32_t size, std::uint8_t flags, const std::uint8_t *body)
    {
        std::size_t produced = 99u;
        const CommandHeader header(CommandHeader::channel_stats_id, size, flags);
        const Status status = ChannelStatsCommand(executor_, header, body).run(produced);
        CHECK_EQ(produced, 0u);
        return status;
    }

private:
    av::MockHost        host_;
    av::MockExecutorOps ops_;
    av::ExecutorLibrary library_;
    av::ExecutorShares  shares_;
    ContextExecutor     executor_;
};

}

TEST(channel_stats_command_run, hands_the_executor_the_body_as_the_context_sent_it)
{ Fixture fixture;
  fixture.on_worker([](Fixture &f) {
      CHECK_STATUS(f.executor().create(), Status::ok);
      const std::uint8_t body[4] = {1u, 2u, 3u, 4u};
      CHECK_STATUS(f.run(CommandHeader::encoded_size + sizeof body, 0u, body), Status::ok);
      f.executor().destroy();
  });
  CHECK_EQ(seen.calls, 1u);
  CHECK_EQ(seen.context, 3u);
  CHECK_EQ(seen.size, 4u);
  CHECK_EQ(seen.body[0], 1u);
  CHECK_EQ(seen.body[3], 4u); }

TEST(channel_stats_command_run, a_command_asking_for_a_reply_or_cut_short_is_malformed)
{ Fixture fixture;
  fixture.on_worker([](Fixture &f) {
      CHECK_STATUS(f.executor().create(), Status::ok);
      const std::uint8_t body[4] = {};
      CHECK_STATUS(f.run(CommandHeader::encoded_size + sizeof body, CommandHeader::generate_reply, body),
                   Status::malformed);
      CHECK_STATUS(f.run(CommandHeader::encoded_size - 1u, 0u, body), Status::malformed);
      f.executor().destroy();
  });
  CHECK_EQ(seen.calls, 0u); }

TEST(channel_stats_command_run, an_executor_not_made_yet_hears_nothing)
{ Fixture fixture;
  fixture.on_worker([](Fixture &f) {
      const std::uint8_t body[4] = {9u, 9u, 9u, 9u};
      CHECK_STATUS(f.run(CommandHeader::encoded_size + sizeof body, 0u, body), Status::ok);
  });
  CHECK_EQ(seen.calls, 0u); }
