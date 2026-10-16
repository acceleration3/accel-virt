/* The device link one driver context holds (ContextLink): acquired at its
 * make, released at its teardown, and lost - for every context on it - the
 * first time one of them finds its channel faulted. */
#include "core/support/status.hpp"
#include "guest/transport/guest_connection.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/cl_context_rig.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/common/context_link.hpp"
#include "icd/common/host_channel.hpp"
#include "icd/common/icd_connection.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>

using av::ClContextRig;
using av::MockKmt;
using av::Status;
using av::SubApi;
using av::TransportResult;
using av::WireReader;
using av::WireWriter;
using av::icd::ChannelShape;
using av::icd::ContextLink;
using av::icd::HostChannel;
using av::icd::IcdConnection;

namespace {

ChannelShape plain()
{
    return ChannelShape(SubApi::opencl, 12u, 4096u, 4096u, 0u, 0u);
}

/* A command no answer was queued for: the transport faults waiting on it. */
void fault(HostChannel &channel)
{
    WireWriter writer = channel.transport().request_writer();
    WireReader reply(nullptr, 0u);
    CHECK(channel.transport().send_sync(0x10001u, writer, reply) != TransportResult::ok);
    CHECK(channel.faulted());
}

}  // namespace

TEST(context_link, an_unheld_link_names_nothing_and_loses_nothing)
{ ClContextRig rig;
  ContextLink  link(rig.connection());
  HostChannel  channel(rig.platform(), rig.log(), "avtest: ", plain(), rig.send_lock());
  CHECK(!link.held());
  CHECK_EQ(link.link(), IcdConnection::no_link);
  CHECK(!link.lost(channel));
  link.release();
  CHECK_EQ(rig.connection().users(), 0u);
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(context_link, acquiring_holds_a_device_until_it_is_released)
{ ClContextRig rig;
  ContextLink  link(rig.connection());
  CHECK_STATUS(link.acquire(), Status::ok);
  CHECK(link.held());
  CHECK(link.link() != IcdConnection::no_link);
  CHECK(link.device().device() != 0u);
  CHECK_EQ(rig.connection().users(), 1u);
  CHECK_EQ(rig.kmt().open_objects(), ClContextRig::link_objects);
  /* A held link is not acquired twice: the second would be a user the
   * connection counts and nothing ever releases. */
  CHECK_STATUS(link.acquire(), Status::invalid_state);
  CHECK_EQ(rig.connection().users(), 1u);
  link.release();
  CHECK(!link.held());
  CHECK_EQ(rig.connection().users(), 0u);
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(context_link, a_link_that_goes_gives_its_device_back)
{ ClContextRig rig;
  { ContextLink link(rig.connection());
    CHECK_STATUS(link.acquire(), Status::ok);
    CHECK_EQ(rig.connection().users(), 1u); }
  CHECK_EQ(rig.connection().users(), 0u);
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(context_link, without_a_device_nothing_is_held)
{ ClContextRig rig;
  ContextLink  link(rig.connection());
  rig.kmt().fail_at(MockKmt::Call::create_device, 1u, MockKmt::invalid_parameter);
  CHECK(link.acquire() != Status::ok);
  CHECK(!link.held());
  CHECK_EQ(rig.connection().users(), 0u); }

/* One context's faulted channel loses the device for the other context on
 * the same link, which finds out with a channel that never faulted; the next
 * context made gets a device of its own. */
TEST(context_link, a_faulted_channel_loses_the_device_for_every_context_on_it)
{ ClContextRig rig;
  ContextLink  first(rig.connection());
  ContextLink  sibling(rig.connection());
  CHECK_STATUS(first.acquire(), Status::ok);
  CHECK_STATUS(sibling.acquire(), Status::ok);
  CHECK_EQ(first.link(), sibling.link());
  HostChannel faulting(rig.platform(), rig.log(), "avtest: ", plain(), rig.send_lock());
  HostChannel quiet(rig.platform(), rig.log(), "avtest: ", plain(), rig.send_lock());
  CHECK_STATUS(faulting.open(first.device()), Status::ok);
  CHECK(!first.lost(faulting));
  CHECK(!sibling.lost(quiet));
  fault(faulting);
  CHECK(first.lost(faulting));
  CHECK(sibling.lost(quiet));
  ContextLink later(rig.connection());
  CHECK_STATUS(later.acquire(), Status::ok);
  CHECK(later.link() != first.link());
  CHECK(!later.lost(quiet));
  faulting.close(true); }
