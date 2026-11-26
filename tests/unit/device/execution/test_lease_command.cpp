#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "device/execution/blob_imports.hpp"
#include "device/execution/context_executor.hpp"
#include "device/execution/executor_library.hpp"
#include "device/execution/executor_shares.hpp"
#include "device/execution/transport_command.hpp"
#include "device/memory/host_blob_table.hpp"
#include "device/recording/wire_recorder.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/lease_record.hpp"

#include <cstddef>
#include <cstdint>

using av::CommandHeader;
using av::ContextExecutor;
using av::LeaseRecord;
using av::Status;

namespace {

constexpr std::uint8_t  uuid[16] = {0x6C};
constexpr std::uint64_t mib = 1ull << 20;

class Fixture {
public:
    Fixture()
        : ops_(AV_EXECUTOR_SUB_API_D3D), library_(), shares_(host_, library_), executor_(host_, library_, shares_, 3u),
          blobs_(host_), recorder_(host_), imports_(host_, blobs_, executor_, recorder_, 3u)
    {
        static_cast<void>(library_.accept(&ops_.table()));
        static_cast<void>(library_.open_domain(uuid, 0u, 0u, 0u, 0u));
        static_cast<void>(shares_.initialize());
        executor_.configure(av::SubApi::d3d, uuid, av::GlContextOptions());
    }

    av::MockExecutorOps &ops() { return ops_; }

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

    Status send(std::uint32_t id, const LeaseRecord &record, bool reply, std::size_t length = LeaseRecord::encoded_size)
    {
        for (std::uint8_t &byte : reply_) byte = 0xEEu;
        produced_ = 99u;
        std::uint8_t body[LeaseRecord::encoded_size + 8u] = {};
        static_cast<void>(record.encode(body, sizeof body));
        const CommandHeader header(id, static_cast<std::uint32_t>(CommandHeader::encoded_size + length),
                                   reply ? CommandHeader::generate_reply : std::uint8_t{0u});
        return av::TransportCommand(imports_, executor_, header, body).run(reply_, sizeof reply_, produced_);
    }

    std::size_t   produced() const { return produced_; }
    std::uint64_t granted() const
    {
        std::uint64_t value = 0u;
        for (std::uint32_t byte = 0u; byte < 8u; byte++) value |= static_cast<std::uint64_t>(reply_[byte]) << (8u * byte);
        return value;
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

}

TEST(lease_command_run, an_extension_answers_the_bytes_the_executor_granted)
{ Fixture fixture;
  fixture.ops().set_lease_room(40 * mib);
  fixture.on_worker([](Fixture &f) {
      CHECK_STATUS(f.executor().create(), Status::ok);
      CHECK_STATUS(f.send(CommandHeader::extend_lease_id, LeaseRecord(LeaseRecord::Kind::host, 64 * mib, 32 * mib), true),
                   Status::ok);
      CHECK_EQ(f.produced(), LeaseRecord::answer_size);
      CHECK_EQ(f.granted(), 32 * mib);
      CHECK_STATUS(f.send(CommandHeader::extend_lease_id, LeaseRecord(LeaseRecord::Kind::device, 16 * mib, 16 * mib), true),
                   Status::ok);
      CHECK_EQ(f.granted(), 0u);
      f.executor().destroy();
  });
  CHECK_EQ(fixture.ops().lease_extensions(), 2u);
  CHECK_EQ(fixture.ops().last_lease_kind(), 0u);
  CHECK_EQ(fixture.ops().last_lease_least(), 16 * mib); }

TEST(lease_command_run, an_extension_before_the_executor_exists_is_granted_nothing)
{ Fixture fixture;
  fixture.on_worker([](Fixture &f) {
      CHECK_STATUS(f.send(CommandHeader::extend_lease_id, LeaseRecord(LeaseRecord::Kind::device, mib, mib), true),
                   Status::ok);
      CHECK_EQ(f.produced(), LeaseRecord::answer_size);
      CHECK_EQ(f.granted(), 0u);
  });
  CHECK_EQ(fixture.ops().lease_extensions(), 0u); }

TEST(lease_command_run, a_return_reaches_the_executor_and_answers_nothing)
{ Fixture fixture;
  fixture.on_worker([](Fixture &f) {
      CHECK_STATUS(f.executor().create(), Status::ok);
      CHECK_STATUS(f.send(CommandHeader::return_lease_id, LeaseRecord(LeaseRecord::Kind::host, 48 * mib, 0u), false),
                   Status::ok);
      CHECK_EQ(f.produced(), 0u);
      f.executor().destroy();
  });
  CHECK_EQ(fixture.ops().lease_returned(), 48 * mib);
  CHECK_EQ(fixture.ops().last_lease_kind(), 1u); }

TEST(lease_command_run, a_wrong_reply_flag_a_bad_record_or_a_return_naming_a_least_is_framing)
{ Fixture fixture;
  fixture.on_worker([](Fixture &f) {
      CHECK_STATUS(f.executor().create(), Status::ok);
      const LeaseRecord record(LeaseRecord::Kind::device, mib, 0u);
      CHECK_STATUS(f.send(CommandHeader::extend_lease_id, record, false), Status::malformed);
      CHECK_STATUS(f.send(CommandHeader::return_lease_id, record, true), Status::malformed);
      CHECK_STATUS(f.send(CommandHeader::extend_lease_id, record, true, LeaseRecord::encoded_size - 1u), Status::malformed);
      CHECK_STATUS(f.send(CommandHeader::extend_lease_id, record, true, LeaseRecord::encoded_size + 8u), Status::malformed);
      CHECK_STATUS(f.send(CommandHeader::return_lease_id, LeaseRecord(LeaseRecord::Kind::device, mib, 1u), false),
                   Status::malformed);
      f.executor().destroy();
  });
  CHECK_EQ(fixture.ops().lease_extensions(), 0u);
  CHECK_EQ(fixture.ops().lease_returned(), 0u); }
