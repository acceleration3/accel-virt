#include "device/display/display_clock.hpp"
#include "device/display/head_planes.hpp"
#include "device/export/export_connection.hpp"
#include "device/export/export_service.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "harness/context/recording_context_plane.hpp"
#include "harness/export/export_connection_probe.hpp"
#include "harness/export/fake_export_consumer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/export/export_answer.hpp"
#include "proto/export/export_greeting.hpp"
#include "proto/export/export_request.hpp"

#include <cstdint>
#include <ctime>

namespace av {
struct ExportServiceProbe {
    static ExportConnection &connection(ExportService &service, std::uint32_t index)
    {
        return service.connections_[index].object();
    }
};

}

using av::ExportAnswer;
using av::ExportConnection;
using av::ExportGreeting;
using av::ExportOperation;
using av::ExportRequest;
using av::ExportRevocation;
using av::ExportService;
using av::FakeExportConsumer;
using av::Status;

namespace {

const std::uint8_t      gpu[ExportGreeting::uuid_bytes] = {0x6Bu};
const AvExportedImage   picture = {-1, 0u, 32u, 32u, 128u, 0u, 0x34325241u, 0u, 0u};
constexpr std::uint64_t surface = 0x40u;

void unshown(void *) {}

struct Timing {
    av::FenceInterrupts interrupts;
    av::DisplayClock    clock;

    explicit Timing(av::MockHost &host) : interrupts(host), clock(host, interrupts, &unshown, nullptr) {}
};

ExportAnswer ask(FakeExportConsumer &consumer, ExportOperation operation, std::uint64_t named)
{
    ExportAnswer answer;
    CHECK(consumer.send(ExportRequest(operation, named, 1u)));
    CHECK_STATUS(consumer.receive(answer), Status::ok);
    return answer;
}

}

TEST(export_service, serves_only_this_user_s_processes_up_to_its_connections)
{ av::MockHost host; av::HeadPlanes heads(host);
  av::RecordingContextPlane plane;
  plane.export_as(surface, picture, 0u);
  Timing timing(host);
  ExportService service(host, timing.clock);
  service.attach(plane, gpu, ExportGreeting(gpu), heads);
  FakeExportConsumer stranger;
  CHECK_STATUS(service.accept(stranger.device_end(), false), Status::invalid_parameter);
  stranger.refused();
  CHECK_EQ(service.live(), 0u);
  CHECK(host.logged() >= 1u);
  FakeExportConsumer consumers[ExportService::max_connections];
  for (FakeExportConsumer &consumer : consumers) CHECK_STATUS(service.accept(consumer.device_end(), true), Status::ok);
  CHECK_EQ(service.live(), ExportService::max_connections);
  FakeExportConsumer late;
  CHECK_STATUS(service.accept(late.device_end(), true), Status::no_space);
  late.refused();
  for (FakeExportConsumer &consumer : consumers)
      CHECK_STATUS(ask(consumer, ExportOperation::hello, 0u).status(), Status::ok);
  CHECK(service.invariants_hold()); }

TEST(export_service, a_slot_whose_consumer_left_serves_the_next)
{ av::MockHost host; av::HeadPlanes heads(host);
  av::RecordingContextPlane plane;
  Timing timing(host);
  ExportService service(host, timing.clock);
  service.attach(plane, gpu, ExportGreeting(gpu), heads);
  FakeExportConsumer consumers[ExportService::max_connections];
  for (FakeExportConsumer &consumer : consumers) CHECK_STATUS(service.accept(consumer.device_end(), true), Status::ok);
  consumers[3].leave();
  FakeExportConsumer next;
  bool served = false;
  for (int attempt = 0; attempt < 5000 && !served; attempt++) {
      served = service.accept(next.device_end(), true) == Status::ok;
      if (served) break;
      next.refused();
      const timespec pause = {0, 1000000};
      nanosleep(&pause, nullptr);
  }
  CHECK(served);
  CHECK_EQ(service.live(), ExportService::max_connections);
  CHECK_STATUS(ask(next, ExportOperation::hello, 0u).status(), Status::invalid_state);
  CHECK(service.invariants_hold()); }

TEST(export_service, a_revocation_reaches_every_consumer_holding_the_surface)
{ av::MockHost host; av::HeadPlanes heads(host);
  av::RecordingContextPlane plane;
  plane.export_as(surface, picture, 0u, 2u);
  Timing timing(host);
  ExportService service(host, timing.clock);
  service.attach(plane, gpu, ExportGreeting(gpu), heads);
  FakeExportConsumer holding, other, idle;
  CHECK_STATUS(service.accept(holding.device_end(), true), Status::ok);
  CHECK_STATUS(service.accept(other.device_end(), true), Status::ok);
  CHECK_STATUS(service.accept(idle.device_end(), true), Status::ok);
  CHECK_STATUS(ask(holding, ExportOperation::export_surface, surface).status(), Status::ok);
  CHECK_STATUS(ask(other, ExportOperation::export_surface, surface).status(), Status::ok);
  CHECK_STATUS(ask(other, ExportOperation::export_surface, surface + 1u).status(), Status::ok);
  service.revoke(surface, ExportRevocation::released);
  ExportAnswer event;
  CHECK_STATUS(holding.receive(event), Status::ok);
  CHECK_EQ(event.surface(), surface);
  CHECK_STATUS(other.receive(event), Status::ok);
  CHECK_EQ(event.surface(), surface);
  CHECK_STATUS(idle.receive(event, 50), Status::invalid_state);
  service.revoke(0u, ExportRevocation::reset);
  FakeExportConsumer *const everyone[] = {&holding, &other, &idle};
  for (FakeExportConsumer *consumer : everyone) {
      CHECK_STATUS(consumer->receive(event), Status::ok);
      CHECK_EQ(event.reason(), 2u);
  }
  CHECK(service.invariants_hold()); }

TEST(export_service, a_host_with_no_thread_refuses_the_connection)
{ av::MockHost host; av::HeadPlanes heads(host);
  av::RecordingContextPlane plane;
  Timing timing(host);
  ExportService service(host, timing.clock);
  FakeExportConsumer consumer;
  host.fail_thread_start_at(1u);
  CHECK_STATUS(service.accept(consumer.device_end(), true), Status::no_memory);
  consumer.refused();
  CHECK_EQ(service.live(), 0u);
  CHECK_STATUS(service.accept(consumer.device_end(), true), Status::ok);
  CHECK_EQ(service.live(), 1u); }

TEST(export_service, its_invariants_are_every_connection_s)
{ av::MockHost host; av::HeadPlanes heads(host);
  Timing timing(host);
  ExportService service(host, timing.clock);
  CHECK(service.invariants_hold());
  av::ExportConnectionProbe::describe(av::ExportServiceProbe::connection(service, 5u), 3);
  CHECK(!service.invariants_hold());
  av::ExportConnectionProbe::describe(av::ExportServiceProbe::connection(service, 5u), -1);
  CHECK(service.invariants_hold()); }

