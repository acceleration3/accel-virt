/* LOSE_HOST_DEVICE (docs/protocol.md, "Losing the host device"): the context's
 * executor is asked to lose its host device and the answer says whether it
 * did; no body is framing, an executor-less context or a host that cannot
 * lose one keeps it, and a slot too small is written nothing. */
#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "device/execution/context_executor.hpp"
#include "device/execution/executor_library.hpp"
#include "device/execution/executor_shares.hpp"
#include "device/execution/lose_device_command.hpp"
#include "device/execution/transport_command.hpp"
#include "device/execution/blob_imports.hpp"
#include "device/memory/host_blob_table.hpp"
#include "device/recording/wire_recorder.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/submission/command_header.hpp"

#include <cstddef>
#include <cstdint>

using av::CommandHeader;
using av::ContextExecutor;
using av::LoseDeviceCommand;
using av::Status;

namespace {

constexpr std::uint8_t uuid[16] = {0x6B};

class Fixture {
public:
    Fixture()
        : ops_(AV_EXECUTOR_SUB_API_D3D), library_(), shares_(host_, library_), executor_(host_, library_, shares_, 3u),
          blobs_(host_), recorder_(host_), imports_(host_, blobs_, executor_, recorder_, 3u)
    {
        static_cast<void>(library_.accept(&ops_.table()));
        static_cast<void>(library_.open_domain(uuid));
        static_cast<void>(shares_.initialize());
        executor_.configure(av::SubApi::d3d, uuid, av::GlContextOptions());
    }

    av::MockExecutorOps &ops() { return ops_; }

    /* `body` on the context's worker, which LOSE_HOST_DEVICE runs on. */
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
    Status run(std::uint32_t size, std::size_t capacity)
    {
        for (std::uint8_t &byte : reply_) byte = 0xEEu;
        produced_ = 99u;
        const CommandHeader header(CommandHeader::lose_host_device_id, size, CommandHeader::generate_reply);
        return av::TransportCommand(imports_, executor_, header, nullptr).run(reply_, capacity, produced_);
    }
    std::size_t   produced() const { return produced_; }
    std::uint32_t word(std::size_t at) const
    {
        return static_cast<std::uint32_t>(reply_[at]) | static_cast<std::uint32_t>(reply_[at + 1u]) << 8u |
               static_cast<std::uint32_t>(reply_[at + 2u]) << 16u | static_cast<std::uint32_t>(reply_[at + 3u]) << 24u;
    }

private:
    av::MockHost        host_;
    av::MockExecutorOps ops_;
    av::ExecutorLibrary library_;
    av::ExecutorShares  shares_;
    ContextExecutor     executor_;
    av::HostBlobTable   blobs_;
    av::WireRecorder    recorder_;
    av::BlobImports     imports_;
    std::uint8_t        reply_[16] = {};
    std::size_t         produced_ = 0u;
};

}  // namespace

TEST(lose_device_command_run, the_host_loses_the_device_and_says_so)
{ Fixture fixture;
  fixture.on_worker([](Fixture &f) {
      CHECK_STATUS(f.executor().create(), Status::ok);
      CHECK_STATUS(f.run(CommandHeader::encoded_size, 16u), Status::ok);
      CHECK_EQ(f.produced(), CommandHeader::lose_host_device_answer);
      CHECK_EQ(f.word(0u), CommandHeader::host_device_lost);
      CHECK_EQ(f.word(4u), 0u);
      /* The context is not faulted by the command: its poll is. */
      CHECK(f.executor().poll() == av::HostPoll::lost);
      f.executor().destroy();
  });
  CHECK_EQ(fixture.ops().losses(), 1u); }

TEST(lose_device_command_run, a_host_that_cannot_lose_one_or_no_executor_keeps_it)
{ Fixture fixture;
  fixture.ops().set_losable(false);
  fixture.on_worker([](Fixture &f) {
      CHECK_STATUS(f.run(CommandHeader::encoded_size, 16u), Status::ok);
      CHECK_EQ(f.word(0u), CommandHeader::host_device_kept);
      CHECK_STATUS(f.executor().create(), Status::ok);
      CHECK_STATUS(f.run(CommandHeader::encoded_size, 16u), Status::ok);
      CHECK_EQ(f.produced(), CommandHeader::lose_host_device_answer);
      CHECK_EQ(f.word(0u), CommandHeader::host_device_kept);
      CHECK(f.executor().poll() == av::HostPoll::idle);
      f.executor().destroy();
  });
  CHECK_EQ(fixture.ops().losses(), 0u); }

/* No producer frames a body on it: that is the context's framing fault, and
 * nothing is lost. A slot short of the answer is written nothing, and the
 * length it needed is said. */
TEST(lose_device_command_run, a_body_is_framing_and_a_short_slot_is_written_nothing)
{ Fixture fixture;
  fixture.on_worker([](Fixture &f) {
      CHECK_STATUS(f.executor().create(), Status::ok);
      const std::uint32_t sizes[] = {CommandHeader::encoded_size + 1u, CommandHeader::encoded_size + 8u,
                                     CommandHeader::max_size};
      for (const std::uint32_t size : sizes) {
          CHECK_STATUS(f.run(size, 16u), Status::malformed);
          CHECK_EQ(f.produced(), 0u);
      }
      CHECK(f.executor().poll() == av::HostPoll::idle);
      CHECK_STATUS(f.run(CommandHeader::encoded_size, CommandHeader::lose_host_device_answer - 1u), Status::ok);
      CHECK_EQ(f.produced(), CommandHeader::lose_host_device_answer);
      CHECK_EQ(f.word(0u), 0xEEEEEEEEu);
      f.executor().destroy();
  });
  CHECK_EQ(fixture.ops().losses(), 1u); }
