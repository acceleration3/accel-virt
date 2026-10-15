/* A test's GPU fault asked of the host (LOSE_HOST_DEVICE, docs/protocol.md
 * "Losing the host device"), over MockTransport playing the host: the answer
 * is read as lost or kept, a call that failed is no answer, and an answer of
 * any other shape is malformed. */
#include "core/support/status.hpp"
#include "guest/transport/host_loss.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/transport/mock_transport.hpp"
#include "proto/submission/command_header.hpp"

#include <cstdint>

using av::CommandHeader;
using av::HostLoss;
using av::MockTransport;
using av::Status;

namespace {

/* The host's answer: u32 status, u32 reserved. */
void answer(MockTransport &transport, std::uint32_t status, std::uint32_t reserved, std::size_t length = 8u)
{
    static std::uint8_t bytes[12];
    for (std::uint32_t at = 0u; at < 4u; at++) {
        bytes[at] = static_cast<std::uint8_t>(status >> (8u * at));
        bytes[4u + at] = static_cast<std::uint8_t>(reserved >> (8u * at));
        bytes[8u + at] = 0u;
    }
    transport.set_reply(bytes, length);
}

}  // namespace

TEST(host_loss, the_host_losing_its_device_is_ok_and_the_call_names_nothing_else)
{ MockTransport transport;
  answer(transport, CommandHeader::host_device_lost, 0u);
  CHECK_STATUS(HostLoss(transport).send(), Status::ok);
  CHECK_EQ(transport.sends(), 1u);
  CHECK_EQ(transport.end_calls(), 1u);
  CHECK_EQ(transport.last_command(), CommandHeader::lose_host_device_id);
  CHECK_EQ(transport.last_length(), 0u); }

/* A host started without the layer that loses devices keeps its own. */
TEST(host_loss, a_host_that_keeps_its_device_is_not_supported)
{ MockTransport transport;
  answer(transport, CommandHeader::host_device_kept, 0u);
  CHECK_STATUS(HostLoss(transport).send(), Status::not_supported); }

/* A context already faulted fails the call: no answer at all. */
TEST(host_loss, a_failed_call_is_invalid_state)
{ MockTransport transport;
  transport.fail_next();
  CHECK_STATUS(HostLoss(transport).send(), Status::invalid_state);
  CHECK_EQ(transport.end_calls(), 1u); }

/* What no device answers: a status past the two, a reserved word set, or an
 * answer longer or shorter than its eight bytes. */
TEST(host_loss, an_answer_of_any_other_shape_is_malformed)
{ MockTransport transport;
  answer(transport, 2u, 0u);
  CHECK_STATUS(HostLoss(transport).send(), Status::malformed);
  answer(transport, CommandHeader::host_device_lost, 1u);
  CHECK_STATUS(HostLoss(transport).send(), Status::malformed);
  answer(transport, CommandHeader::host_device_lost, 0u, 4u);
  CHECK_STATUS(HostLoss(transport).send(), Status::malformed);
  answer(transport, CommandHeader::host_device_lost, 0u, 12u);
  CHECK_STATUS(HostLoss(transport).send(), Status::malformed);
  answer(transport, CommandHeader::host_device_lost, 0u, 0u);
  CHECK_STATUS(HostLoss(transport).send(), Status::malformed); }
