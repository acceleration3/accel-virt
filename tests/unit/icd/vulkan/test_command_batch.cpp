/* A command buffer's batch (research.md §5): the generated client records into
 * it as it would send, nothing reaches the host until end, and end publishes
 * every recorded command in one submission framed exactly as the device reads
 * it. A command that asks for an answer is refused; discard drops a recording; a
 * batch past the connection's command limit, or one whose memory could not be
 * had, answers no_memory at end and ships nothing; a connection that refuses
 * the batch answers invalid_state. Invariants after every step. */
#include "core/support/status.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/icd_rig.hpp"
#include "harness/icd/shipped_batch.hpp"
#include "icd/vulkan/command_batch.hpp"
#include "icd/vulkan/connection.hpp"
#include "proto/client_blob_arena.hpp"
#include "proto/generated/vulkan/vk_dispatch_client.h"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>

using av::CommandHeader;
using av::IcdRig;
using av::KmtTransport;
using av::MockKmt;
using av::ShippedBatch;
using av::Status;
using av::TransportResult;
using av::WireReader;
using av::WireWriter;
using av::icd::CommandBatch;
using av::icd::Connection;
using namespace av::vkproto;

namespace av {

struct KmtTransportProbe {
    static std::uint32_t ring(KmtTransport &t) { return t.allocations_[0]; }
};

}  // namespace av

namespace av::icd {

struct ConnectionProbe {
    static KmtTransport &transport(Connection &c) { return c.transport_.object(); }
};

struct CommandBatchProbe {
    static std::uint8_t *block(CommandBatch &b) { return b.block_; }
    static std::size_t  &used(CommandBatch &b) { return b.used_; }
    static std::size_t  &capacity(CommandBatch &b) { return b.capacity_; }
    static std::size_t  &last(CommandBatch &b) { return b.last_; }
    static Status       &failure(CommandBatch &b) { return b.failure_; }
    static std::uint8_t *swap_block(CommandBatch &b, std::uint8_t *block)
    {
        std::uint8_t *const kept = b.block_;
        b.block_ = block;
        return kept;
    }
};

}  // namespace av::icd

using av::KmtTransportProbe;
using av::icd::CommandBatchProbe;
using av::icd::ConnectionProbe;

namespace {

/* A vkCmdDraw on the wire: its header, the command buffer and four counts. */
constexpr std::uint32_t draw_bytes = 32u;
/* What fills the connection's command limit exactly. */
constexpr std::uint32_t draws_to_the_limit = static_cast<std::uint32_t>(Connection::command_bytes / draw_bytes);
/* Not a handle the root made: it crosses as 0, which is all a batch needs. */
const VkCommandBuffer command_buffer = VK_NULL_HANDLE;

/* A connection of its own on the rig's MockKmt, under the rig's send lock, and
 * a batch shipping through it; the rig's root is what the generated encoders
 * unwrap handles through. */
class BatchRig {
public:
    BatchRig() : connection_(rig_.kmt(), rig_.platform(), rig_.send_lock()), batch_(rig_.platform(), connection_)
    {
        CHECK_STATUS(connection_.open(), Status::ok);
        baseline_ = rig_.platform().live_allocations();
        CHECK(batch_.invariants_hold());
    }

    IcdRig       &rig() { return rig_; }
    MockKmt      &kmt() { return rig_.kmt(); }
    Connection   &connection() { return connection_; }
    CommandBatch &batch() { return batch_; }
    std::uint32_t baseline() const { return baseline_; }

    void draw(std::uint32_t vertices)
    {
        rvk_vkCmdDraw(batch_, command_buffer, vertices, 1u, 0u, 0u);
        CHECK(batch_.invariants_hold());
    }

    /* The batch the last submission carried, as the device reads it. */
    void check_shipped_draws(ShippedBatch &shipped, std::uint32_t first, std::uint32_t count)
    {
        CHECK_STATUS(shipped.read(), Status::ok);
        CHECK_EQ(shipped.commands(), count);
        for (std::uint32_t index = 0u; index < shipped.commands() && index < count; index++) {
            CHECK_EQ(shipped.header(index).id(), static_cast<std::uint32_t>(kCmd_vkCmdDraw));
            CHECK_EQ(shipped.header(index).size(), draw_bytes);
            CHECK(!shipped.header(index).generates_reply());
            WireReader body(shipped.body(index), draw_bytes - CommandHeader::encoded_size);
            CHECK_EQ(body.read_u64(), 0u);
            CHECK_EQ(body.read_u32(), first + index);
        }
    }

    std::uint32_t ring() { return KmtTransportProbe::ring(ConnectionProbe::transport(connection_)); }

private:
    IcdRig        rig_;
    Connection    connection_;
    CommandBatch  batch_;
    std::uint32_t baseline_ = 0u;
};

}  // namespace

TEST(command_batch, a_thousand_recorded_commands_cost_one_submission)
{ BatchRig rig;
  const std::uint32_t submitted = rig.kmt().calls(MockKmt::Call::submit);
  for (std::uint32_t index = 0u; index < 1000u; index++) rig.draw(index);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), submitted);
  rig.rig().trace().clear();
  CHECK_STATUS(rig.batch().end(), Status::ok);
  CHECK(rig.batch().invariants_hold());
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), submitted + 1u);
  /* One publish under the send lock, and no wait: a recorded command never answers. */
  CHECK(std::strcmp(rig.rig().trace().text(), "Ss") == 0);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_context), 0u);
  ShippedBatch shipped(rig.kmt(), rig.ring());
  rig.check_shipped_draws(shipped, 0u, 1000u); }

TEST(command_batch, end_empties_the_batch_for_the_next_recording)
{ BatchRig rig;
  rig.draw(7u);
  CHECK_STATUS(rig.batch().end(), Status::ok);
  rig.draw(8u);
  rig.draw(9u);
  CHECK_STATUS(rig.batch().end(), Status::ok);
  CHECK(rig.batch().invariants_hold());
  ShippedBatch shipped(rig.kmt(), rig.ring());
  rig.check_shipped_draws(shipped, 8u, 2u);
  /* The block is kept for the next recording rather than taken again. */
  CHECK_EQ(rig.rig().platform().live_allocations(), rig.baseline() + 1u); }

TEST(command_batch, a_command_that_asks_for_an_answer_is_refused_and_not_recorded)
{ BatchRig rig;
  rig.draw(1u);
  WireWriter writer = rig.batch().request_writer();
  writer.write_u64(0u);
  WireReader reply(nullptr, 0u);
  CHECK(rig.batch().send_sync(kCmd_vkGetFenceStatus, writer, reply) == TransportResult::failed);
  CHECK_EQ(reply.remaining(), 0u);
  CHECK(rig.batch().invariants_hold());
  /* Through the generated client: the call fails as a lost device would. */
  CHECK_EQ(rvk_vkGetFenceStatus(rig.batch(), VK_NULL_HANDLE, VK_NULL_HANDLE), VK_ERROR_DEVICE_LOST);
  rig.batch().end_call();
  CHECK(rig.batch().invariants_hold());
  rig.draw(2u);
  CHECK_STATUS(rig.batch().end(), Status::ok);
  ShippedBatch shipped(rig.kmt(), rig.ring());
  rig.check_shipped_draws(shipped, 1u, 2u); }

TEST(command_batch, discard_drops_what_was_recorded)
{ BatchRig rig;
  for (std::uint32_t index = 0u; index < 3u; index++) rig.draw(100u + index);
  rig.batch().discard();
  CHECK(rig.batch().invariants_hold());
  CHECK_EQ(CommandBatchProbe::used(rig.batch()), 0u);
  rig.draw(5u);
  rig.draw(6u);
  CHECK_STATUS(rig.batch().end(), Status::ok);
  ShippedBatch shipped(rig.kmt(), rig.ring());
  rig.check_shipped_draws(shipped, 5u, 2u); }

TEST(command_batch, a_batch_filling_the_command_limit_exactly_ships)
{ BatchRig rig;
  for (std::uint32_t index = 0u; index < draws_to_the_limit; index++) rig.draw(index);
  CHECK_EQ(CommandBatchProbe::used(rig.batch()), Connection::command_bytes);
  CHECK_STATUS(rig.batch().end(), Status::ok);
  ShippedBatch shipped(rig.kmt(), rig.ring());
  rig.check_shipped_draws(shipped, 0u, draws_to_the_limit); }

TEST(command_batch, a_batch_past_the_command_limit_answers_no_memory_at_end_and_ships_nothing)
{ BatchRig rig;
  const std::uint32_t submitted = rig.kmt().calls(MockKmt::Call::submit);
  for (std::uint32_t index = 0u; index <= draws_to_the_limit; index++) rig.draw(index);
  CHECK_EQ(CommandBatchProbe::used(rig.batch()), Connection::command_bytes);
  /* Later commands are refused too, rather than recorded after a gap. */
  rig.draw(0xFFFFu);
  CHECK_STATUS(rig.batch().end(), Status::no_memory);
  CHECK(rig.batch().invariants_hold());
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), submitted);
  /* The failure went with the recording: the next one ships. */
  rig.draw(3u);
  CHECK_STATUS(rig.batch().end(), Status::ok);
  ShippedBatch shipped(rig.kmt(), rig.ring());
  rig.check_shipped_draws(shipped, 3u, 1u); }

/* The block grows 32, 64, 128 and 256 KiB: whichever of those allocations is
 * refused, end answers no_memory, nothing is shipped, and the memory the
 * batch holds afterwards is at most the block it kept. */
TEST(command_batch, memory_refused_while_recording_answers_no_memory_at_end_and_leaks_nothing)
{ for (std::uint32_t refused = 1u; refused <= 4u; refused++) {
      BatchRig rig;
      const std::uint32_t submitted = rig.kmt().calls(MockKmt::Call::submit);
      rig.rig().platform().fail_allocation_at(refused);
      for (std::uint32_t index = 0u; index < draws_to_the_limit; index++) rig.draw(index);
      CHECK_STATUS(rig.batch().end(), Status::no_memory);
      CHECK(rig.batch().invariants_hold());
      CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), submitted);
      CHECK_EQ(rig.rig().platform().live_allocations(), rig.baseline() + (refused == 1u ? 0u : 1u));
  }
}

TEST(command_batch, the_block_goes_with_the_batch)
{ IcdRig rig;
  Connection connection(rig.kmt(), rig.platform(), rig.send_lock());
  CHECK_STATUS(connection.open(), Status::ok);
  const std::uint32_t baseline = rig.platform().live_allocations();
  {
      CommandBatch batch(rig.platform(), connection);
      rvk_vkCmdDraw(batch, command_buffer, 3u, 1u, 0u, 0u);
      CHECK(batch.invariants_hold());
      CHECK_EQ(rig.platform().live_allocations(), baseline + 1u);
  }
  CHECK_EQ(rig.platform().live_allocations(), baseline);
  /* A batch never recorded into holds nothing. */
  { CommandBatch batch(rig.platform(), connection);
    CHECK(batch.invariants_hold()); }
  CHECK_EQ(rig.platform().live_allocations(), baseline); }

TEST(command_batch, lends_each_command_its_room_and_refuses_one_larger)
{ BatchRig rig;
  WireWriter writer = rig.batch().request_writer();
  writer.write_zeros(CommandBatch::command_room);
  CHECK_STATUS(writer.status(), Status::ok);
  CHECK(rig.batch().send_async(kCmd_vkCmdDraw, writer) == TransportResult::ok);
  CHECK(rig.batch().invariants_hold());
  writer = rig.batch().request_writer();
  writer.write_zeros(CommandBatch::command_room + 1u);
  CHECK_STATUS(writer.status(), Status::too_large);
  CHECK(rig.batch().send_async(kCmd_vkCmdDraw, writer) == TransportResult::failed);
  CHECK(rig.batch().invariants_hold());
  CHECK_STATUS(rig.batch().end(), Status::no_memory); }

TEST(command_batch, a_connection_refusing_the_batch_answers_invalid_state_and_drops_it)
{ BatchRig rig;
  /* A faulted transport refuses every publish. */
  rig.kmt().withhold_answer_at(1u);
  std::uint64_t instance = 0u;
  VkInstanceCreateInfo create = {};
  create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  CHECK_EQ(rvk_vkCreateInstance(rig.connection().transport(), &create, nullptr, &instance), VK_ERROR_DEVICE_LOST);
  CHECK(rig.connection().faulted());
  rig.draw(1u);
  const std::uint32_t submitted = rig.kmt().calls(MockKmt::Call::submit);
  CHECK_STATUS(rig.batch().end(), Status::invalid_state);
  CHECK(rig.batch().invariants_hold());
  CHECK_EQ(CommandBatchProbe::used(rig.batch()), 0u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), submitted); }

TEST(command_batch, answers_no_decode_room_and_no_blobs)
{ BatchRig rig;
  CHECK(rig.batch().decode_arena().allocate<std::uint32_t>() == nullptr);
  CHECK(!rig.batch().blobs().ready()); }

TEST(command_batch, invariants_see_a_recording_that_does_not_frame_or_does_not_fit)
{ BatchRig rig;
  rig.draw(1u);
  rig.draw(2u);
  CommandBatch &batch = rig.batch();
  std::size_t &used = CommandBatchProbe::used(batch);
  std::size_t &capacity = CommandBatchProbe::capacity(batch);
  used -= 1u;
  CHECK(!batch.invariants_hold());
  used += 1u;
  CHECK(batch.invariants_hold());
  std::size_t &last = CommandBatchProbe::last(batch);
  last = used;
  CHECK(!batch.invariants_hold());
  last = draw_bytes;
  CHECK(batch.invariants_hold());
  /* The last command's size, then its flags. */
  const std::uint32_t fields[] = {draw_bytes + 4u, draw_bytes + 7u};
  for (const std::uint32_t at : fields) {
      const std::uint8_t kept = CommandBatchProbe::block(batch)[at];
      CommandBatchProbe::block(batch)[at] = 0xFFu;
      CHECK(!batch.invariants_hold());
      CommandBatchProbe::block(batch)[at] = kept;
      CHECK(batch.invariants_hold());
  }
  const std::size_t held = capacity;
  capacity = used - 1u;
  CHECK(!batch.invariants_hold());
  capacity = Connection::command_bytes + 1u;
  CHECK(!batch.invariants_hold());
  capacity = held;
  std::uint8_t *const block = CommandBatchProbe::swap_block(batch, nullptr);
  CHECK(!batch.invariants_hold());
  CommandBatchProbe::swap_block(batch, block);
  CommandBatchProbe::failure(batch) = Status::invalid_parameter;
  CHECK(!batch.invariants_hold());
  CommandBatchProbe::failure(batch) = Status::no_memory;
  CHECK(batch.invariants_hold());
  batch.discard();
  CHECK(batch.invariants_hold());
  /* Nothing recorded, yet a last command named. */
  last = draw_bytes;
  CHECK(!batch.invariants_hold());
  last = 0u;
  CHECK(batch.invariants_hold()); }
