#include "device/display/display_clock.hpp"
#include "device/display/head_planes.hpp"
#include "device/export/export_connection.hpp"
#include "device/export/exported_gpu.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "harness/context/recording_context_plane.hpp"
#include "harness/export/export_connection_probe.hpp"
#include "harness/export/fake_export_consumer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/export/export_answer.hpp"
#include "proto/export/export_request.hpp"
#include "proto/pages/vblank_record.hpp"

#include <cstdint>
#include <cstring>
#include <ctime>
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>


using av::ExportAnswer;
using av::ExportConnection;
using av::ExportGreeting;
using av::ExportOperation;
using av::ExportRequest;
using av::ExportRevocation;
using av::FakeExportConsumer;
using av::Status;

namespace {

const std::uint8_t      gpu[ExportGreeting::uuid_bytes] = {0x5Au};
const AvExportedImage   picture = {-1, 0u, 64u, 64u, 256u, 0u, 0x34325241u, 0u, 0x00FFFFFFFFFFFFFFull};
constexpr std::uint64_t first = 0x100u;

void unshown(void *) {}

class Fixture {
public:
    Fixture()
        : host_(), plane_(), gpu_(host_), interrupts_(host_), clock_(host_, interrupts_, &unshown, nullptr),
          connection_(host_, gpu_, clock_)
    {
        plane_.export_as(first, picture, 3u, 2u * ExportConnection::first_room + 2u);
        gpu_.attach(plane_, gpu, ExportGreeting(gpu), heads_);
    }

    av::MockHost      &host() { return host_; }
    ExportConnection  &connection() { return connection_; }
    av::DisplayClock  &clock() { return clock_; }

    ExportAnswer ask(FakeExportConsumer &consumer, ExportOperation operation, std::uint64_t surface)
    {
        ExportAnswer answer;
        CHECK(consumer.send(ExportRequest(operation, surface, surface + 1000u)));
        CHECK_STATUS(consumer.receive(answer), Status::ok);
        CHECK(answer.operation() == operation);
        CHECK_EQ(answer.tag(), surface + 1000u);
        return answer;
    }

    bool ended_within(int milliseconds)
    {
        for (int waited = 0; waited < milliseconds; waited++) {
            if (connection_.ended()) return true;
            const timespec pause = {0, 1000000};
            nanosleep(&pause, nullptr);
        }
        return false;
    }

private:
    av::MockHost              host_;
    av::HeadPlanes            heads_{host_};
    av::RecordingContextPlane plane_;
    av::ExportedGpu           gpu_;
    av::FenceInterrupts       interrupts_;
    av::DisplayClock          clock_;
    ExportConnection          connection_;
};

bool readable(int fd)
{
    pollfd watched = {fd, POLLIN, 0};
    return ::poll(&watched, 1u, 1000) == 1;
}

}

TEST(export_connection, answers_hello_export_sync_and_release_in_turn)
{ Fixture fixture;
  FakeExportConsumer consumer;
  CHECK_STATUS(fixture.connection().open(consumer.device_end()), Status::ok);
  CHECK(fixture.connection().serving());
  CHECK(!fixture.connection().ended());
  ExportAnswer answer = fixture.ask(consumer, ExportOperation::hello, 0u);
  CHECK_STATUS(answer.status(), Status::ok);
  CHECK(answer.greeting().valid());
  CHECK_EQ(answer.greeting().uuid()[0], 0x5Au);
  CHECK_STATUS(fixture.ask(consumer, ExportOperation::sync, first).status(), Status::not_found);
  answer = fixture.ask(consumer, ExportOperation::export_surface, first);
  CHECK_STATUS(answer.status(), Status::ok);
  CHECK_EQ(answer.layout().stride(), 256u);
  struct stat file = {};
  CHECK(consumer.descriptor() >= 0 && ::fstat(consumer.descriptor(), &file) == 0 && file.st_size == 4096);
  CHECK_EQ(fixture.connection().held(), 1u);
  CHECK_STATUS(fixture.ask(consumer, ExportOperation::export_surface, first).status(), Status::ok);
  CHECK_EQ(fixture.connection().held(), 1u);
  answer = fixture.ask(consumer, ExportOperation::sync, first);
  CHECK_STATUS(answer.status(), Status::ok);
  CHECK_EQ(answer.writes(), 3u);
  CHECK(readable(consumer.descriptor()));
  CHECK_STATUS(fixture.ask(consumer, ExportOperation::release, first).status(), Status::ok);
  CHECK_EQ(fixture.connection().held(), 0u);
  CHECK_STATUS(fixture.ask(consumer, ExportOperation::release, first).status(), Status::ok);
  CHECK_STATUS(fixture.ask(consumer, ExportOperation::sync, first).status(), Status::not_found);
  CHECK_EQ(fixture.host().exports_closed(), 3u);
  CHECK(fixture.connection().invariants_hold()); }

TEST(export_connection, a_timing_moves_the_display_clock_once_there_is_one)
{ Fixture fixture;
  FakeExportConsumer consumer;
  CHECK_STATUS(fixture.connection().open(consumer.device_end()), Status::ok);
  ExportAnswer answer;
  CHECK(consumer.send(ExportRequest(5000, 77u)));
  CHECK_STATUS(consumer.receive(answer), Status::ok);
  CHECK(answer.operation() == ExportOperation::timing);
  CHECK_STATUS(answer.status(), Status::invalid_state);
  CHECK_EQ(answer.tag(), 77u);
  CHECK_STATUS(fixture.clock().initialize(), Status::ok);
  CHECK_STATUS(fixture.clock().retime_all(144000u), Status::ok);
  CHECK(consumer.send(ExportRequest(-5000, 78u)));
  CHECK_STATUS(consumer.receive(answer), Status::ok);
  CHECK_STATUS(answer.status(), Status::ok);
  CHECK_EQ(answer.surface(), 0u);
  CHECK_EQ(answer.tag(), 78u);
  constexpr std::uint64_t ram = 0x200000u;
  fixture.host().add_guest_ram(ram, 0x2000u);
  CHECK_STATUS(fixture.clock().attach(ram + 0x1000u), Status::ok);
  av::VblankRecord record;
  CHECK_STATUS(record.decode(fixture.host().guest_ram(ram + 0x1000u + 32u + av::VblankRecord::fields_at),
                             av::VblankRecord::encoded_size),
               Status::ok);
  CHECK_EQ(record.refresh_millihz(), 144000u);
  fixture.clock().detach();
  CHECK(fixture.connection().invariants_hold()); }

TEST(export_connection, what_cannot_be_read_is_answered_and_the_connection_goes_on)
{ Fixture fixture;
  FakeExportConsumer consumer;
  CHECK_STATUS(fixture.connection().open(consumer.device_end()), Status::ok);
  std::uint8_t bytes[ExportRequest::encoded_size + 8u] = {};
  CHECK_STATUS(ExportRequest(ExportOperation::export_surface, first, 4u).encode(bytes, sizeof bytes), Status::ok);
  struct Case {
      std::size_t   length;
      std::uint8_t  major;
      Status        expected;
  };
  const Case cases[] = {{ExportRequest::encoded_size - 1u, 1u, Status::malformed},
                        {ExportRequest::encoded_size + 8u, 1u, Status::malformed},
                        {ExportRequest::encoded_size, 2u, Status::not_supported},
                        {1u, 1u, Status::malformed}};
  for (const Case &item : cases) {
      bytes[4] = item.major;
      CHECK(consumer.send_bytes(bytes, item.length));
      ExportAnswer answer;
      CHECK_STATUS(consumer.receive(answer), Status::ok);
      CHECK(answer.operation() == ExportOperation::unreadable);
      CHECK_STATUS(answer.status(), item.expected);
      CHECK_EQ(answer.tag(), 0u);
      CHECK_EQ(consumer.descriptor(), -1);
  }
  CHECK_EQ(fixture.connection().held(), 0u);
  CHECK_STATUS(fixture.ask(consumer, ExportOperation::hello, 0u).status(), Status::ok); }

TEST(export_connection, a_consumer_holds_past_its_first_room_and_is_refused_only_for_memory)
{ Fixture fixture;
  FakeExportConsumer consumer;
  CHECK_STATUS(fixture.connection().open(consumer.device_end()), Status::ok);
  const std::uint32_t full = 2u * ExportConnection::first_room;
  for (std::uint64_t surface = first; surface < first + full; surface++)
      CHECK_STATUS(fixture.ask(consumer, ExportOperation::export_surface, surface).status(), Status::ok);
  CHECK_EQ(fixture.connection().held(), full);
  CHECK(fixture.connection().invariants_hold());
  const std::uint64_t past = first + full;
  fixture.host().fail_allocation_at(1u);
  const ExportAnswer refused = fixture.ask(consumer, ExportOperation::export_surface, past);
  CHECK_STATUS(refused.status(), Status::no_space);
  CHECK_EQ(consumer.descriptor(), -1);
  fixture.host().fail_allocation_at(0u);
  CHECK_STATUS(fixture.ask(consumer, ExportOperation::export_surface, first + 5u).status(), Status::ok);
  CHECK_STATUS(fixture.ask(consumer, ExportOperation::release, first + 5u).status(), Status::ok);
  CHECK_STATUS(fixture.ask(consumer, ExportOperation::export_surface, past).status(), Status::ok);
  CHECK_EQ(fixture.connection().held(), full);
  CHECK_STATUS(fixture.ask(consumer, ExportOperation::export_surface, past + 1u).status(), Status::ok);
  CHECK_EQ(fixture.connection().held(), full + 1u);
  CHECK_STATUS(fixture.ask(consumer, ExportOperation::release, first).status(), Status::ok);
  CHECK_STATUS(fixture.ask(consumer, ExportOperation::export_surface, first + 0x10000u).status(),
               Status::not_found);
  CHECK_EQ(fixture.connection().held(), full);
  CHECK(fixture.connection().invariants_hold()); }

TEST(export_connection, a_revocation_is_told_only_to_a_consumer_holding_the_surface)
{ Fixture fixture;
  FakeExportConsumer consumer;
  CHECK_STATUS(fixture.connection().open(consumer.device_end()), Status::ok);
  CHECK_STATUS(fixture.ask(consumer, ExportOperation::export_surface, first).status(), Status::ok);
  CHECK_STATUS(fixture.ask(consumer, ExportOperation::export_surface, first + 1u).status(), Status::ok);
  fixture.connection().revoke(first + 2u, ExportRevocation::released);
  ExportAnswer event;
  CHECK_STATUS(consumer.receive(event, 50), Status::invalid_state);
  fixture.connection().revoke(first, ExportRevocation::released);
  CHECK_STATUS(consumer.receive(event), Status::ok);
  CHECK(event.operation() == ExportOperation::revoked);
  CHECK_EQ(event.surface(), first);
  CHECK_EQ(event.reason(), 1u);
  CHECK_EQ(fixture.connection().held(), 1u);
  CHECK_STATUS(fixture.ask(consumer, ExportOperation::sync, first).status(), Status::not_found);
  fixture.connection().revoke(0u, ExportRevocation::reset);
  CHECK_STATUS(consumer.receive(event), Status::ok);
  CHECK_EQ(event.surface(), 0u);
  CHECK_EQ(event.reason(), 2u);
  CHECK_EQ(fixture.connection().held(), 0u);
  CHECK_STATUS(fixture.ask(consumer, ExportOperation::sync, first + 1u).status(), Status::not_found);
  CHECK(fixture.connection().invariants_hold()); }

TEST(export_connection, a_consumer_that_leaves_ends_its_thread_and_frees_the_connection)
{ Fixture fixture;
  {
      FakeExportConsumer consumer;
      CHECK_STATUS(fixture.connection().open(consumer.device_end()), Status::ok);
      CHECK_STATUS(fixture.ask(consumer, ExportOperation::export_surface, first).status(), Status::ok);
      consumer.leave();
      CHECK(fixture.ended_within(5000));
      CHECK_EQ(fixture.connection().held(), 0u);
      fixture.connection().revoke(0u, ExportRevocation::reset);
      const std::uint32_t closed = fixture.host().exports_closed();
      fixture.connection().close();
      CHECK_EQ(fixture.host().exports_closed(), closed + 1u);
      CHECK(!fixture.connection().serving());
      CHECK(!fixture.connection().ended());
      CHECK(fixture.connection().invariants_hold());
  }
  FakeExportConsumer next;
  CHECK_STATUS(fixture.connection().open(next.device_end()), Status::ok);
  CHECK_STATUS(fixture.ask(next, ExportOperation::hello, 0u).status(), Status::ok);
  fixture.connection().close();
  CHECK(next.hung_up());
  fixture.connection().close();
  CHECK(fixture.connection().invariants_hold()); }

TEST(export_connection, a_consumer_that_takes_no_answer_is_shut_out)
{ Fixture fixture;
  {
      FakeExportConsumer consumer;
      CHECK_STATUS(fixture.connection().open(consumer.device_end()), Status::ok);
      fixture.host().fail_export_send_at(1u);
      CHECK(consumer.send(ExportRequest(ExportOperation::hello, 0u, 1u)));
      CHECK(fixture.ended_within(5000));
      CHECK(consumer.hung_up());
      fixture.connection().close();
  }
  FakeExportConsumer consumer;
  CHECK_STATUS(fixture.connection().open(consumer.device_end()), Status::ok);
  CHECK_STATUS(fixture.ask(consumer, ExportOperation::export_surface, first).status(), Status::ok);
  fixture.host().fail_export_send_at(1u);
  fixture.connection().revoke(first, ExportRevocation::released);
  CHECK(consumer.hung_up());
  CHECK(fixture.ended_within(5000)); }

TEST(export_connection, open_refuses_a_second_descriptor_and_a_host_with_no_mutex_or_thread)
{ Fixture fixture;
  FakeExportConsumer refused;
  fixture.host().fail_mutex_at(1u);
  const int end = refused.device_end();
  CHECK_STATUS(fixture.connection().open(end), Status::no_memory);
  CHECK(fixture.host().descriptor_open(end));
  CHECK(!fixture.connection().serving());
  fixture.host().fail_thread_start_at(1u);
  CHECK_STATUS(fixture.connection().open(end), Status::no_memory);
  CHECK(fixture.host().descriptor_open(end));
  CHECK(fixture.connection().invariants_hold());
  refused.refused();
  FakeExportConsumer consumer;
  CHECK_STATUS(fixture.connection().open(consumer.device_end()), Status::ok);
  FakeExportConsumer second;
  const int other = second.device_end();
  CHECK_STATUS(fixture.connection().open(other), Status::invalid_state);
  CHECK(fixture.host().descriptor_open(other));
  second.refused();
  CHECK_STATUS(fixture.ask(consumer, ExportOperation::hello, 0u).status(), Status::ok); }

TEST(export_connection, its_invariants_catch_a_table_no_request_can_make)
{ Fixture fixture;
  ExportConnection &connection = fixture.connection();
  CHECK(connection.invariants_hold());
  av::ExportConnectionProbe::hold(connection, 1u, first);
  CHECK(!connection.invariants_hold());
  av::ExportConnectionProbe::hold(connection, 0u, first);
  av::ExportConnectionProbe::describe(connection, 3);
  CHECK(!connection.invariants_hold());
  av::ExportConnectionProbe::describe(connection, -1);
  FakeExportConsumer consumer;
  CHECK_STATUS(connection.open(consumer.device_end()), Status::ok);
  CHECK_STATUS(fixture.ask(consumer, ExportOperation::hello, 0u).status(), Status::ok);
  av::ExportConnectionProbe::hold(connection, 3u, first);
  av::ExportConnectionProbe::overfill(connection);
  CHECK(!connection.invariants_hold());
  av::ExportConnectionProbe::hold(connection, 3u, first);
  CHECK(connection.invariants_hold());
  av::ExportConnectionProbe::name(connection, 1u, 0u);
  CHECK(!connection.invariants_hold());
  av::ExportConnectionProbe::name(connection, 1u, first);
  CHECK(!connection.invariants_hold());
  av::ExportConnectionProbe::hold(connection, 0u, first);
  CHECK(connection.invariants_hold()); }

