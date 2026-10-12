/* The driver's own release of a shared object, request byte by request byte:
 * a well-formed one tells the contexts with this device's host GPU, then the
 * consumers outside QEMU it was exported to, and is answered; a malformed one
 * is refused and tells nobody, and every other type is left to the rest of
 * the device. */
#include "device/control/control_request.hpp"
#include "device/control/shared_commands.hpp"
#include "device/display/display_clock.hpp"
#include "device/export/export_connection.hpp"
#include "device/export/export_service.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "harness/context/recording_context_plane.hpp"
#include "harness/export/fake_export_consumer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/gpu_request_builder.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"
#include "proto/export/export_greeting.hpp"
#include "proto/virtio_gpu/shared_release_request.hpp"

#include <cstdint>
#include <cstring>

using av::ControlRequest;
using av::GpuRequestBuilder;
using av::MockHost;
using av::MockRequest;
using av::SharedCommands;
using av::SharedReleaseRequest;

namespace {

constexpr std::uint32_t ok_nodata = 0x1100u, err_invalid_parameter = 0x1205u;
const std::uint8_t      gpu[16] = {0x10u, 0x20u};

/* No page is registered, so the display clock never shows anything. */
void unshown(void *) {}

class Fixture {
public:
    Fixture()
        : interrupts_(host_), clock_(host_, interrupts_, &unshown, nullptr), exports_(host_, clock_),
          commands_(host_, plane_, exports_, gpu)
    {
        exports_.attach(plane_, gpu, av::ExportGreeting(gpu));
    }

    /* The response type, or 0 when the core left the request to the rest. */
    std::uint32_t send(std::uint32_t type, const GpuRequestBuilder &builder)
    {
        MockRequest    mock(builder.bytes(), builder.size());
        ControlRequest request(host_, &mock);
        return commands_.answer(request, type) ? mock.response_type() : 0u;
    }

    av::RecordingContextPlane &plane() { return plane_; }
    av::ExportService         &exports() { return exports_; }

private:
    MockHost                  host_;
    av::RecordingContextPlane plane_;
    av::FenceInterrupts       interrupts_;
    av::DisplayClock          clock_;
    av::ExportService         exports_;
    SharedCommands            commands_;
};

GpuRequestBuilder release(std::uint64_t id)
{
    GpuRequestBuilder request(SharedReleaseRequest::type);
    request.u64(id);
    return request;
}

}  // namespace

TEST(shared_commands, a_release_tells_the_contexts_on_this_gpu_and_is_answered)
{ Fixture fixture;
  CHECK_EQ(fixture.send(SharedReleaseRequest::type, release(0x2Au)), ok_nodata);
  CHECK_EQ(fixture.plane().shared_count(), 1u);
  CHECK(fixture.plane().shared(0u) == 0x2Au);
  CHECK(fixture.plane().shared_gpu() == gpu);
  /* An id nothing holds is still the driver's word, and still answered. */
  CHECK_EQ(fixture.send(SharedReleaseRequest::type, release(0x2Au)), ok_nodata);
  CHECK_EQ(fixture.plane().shared_count(), 2u); }

/* The consumer that exported the surface is told after the executor let go of
 * it; a malformed release tells it nothing. */
TEST(shared_commands, a_release_revokes_the_surface_from_the_consumer_that_exported_it)
{ Fixture fixture;
  fixture.plane().export_as(0x2Au, AvExportedImage{-1, 0u, 64u, 64u, 256u, 0u, 0x34325241u, 0u, 0u}, 0u);
  av::FakeExportConsumer consumer;
  CHECK_STATUS(fixture.exports().accept(consumer.device_end(), true), av::Status::ok);
  av::ExportAnswer answer;
  CHECK(consumer.send(av::ExportRequest(av::ExportOperation::export_surface, 0x2Au, 1u)));
  CHECK_STATUS(consumer.receive(answer), av::Status::ok);
  CHECK_STATUS(answer.status(), av::Status::ok);
  CHECK_EQ(fixture.send(SharedReleaseRequest::type, release(0u)), err_invalid_parameter);
  CHECK_STATUS(consumer.receive(answer, 50), av::Status::invalid_state);
  CHECK_EQ(fixture.send(SharedReleaseRequest::type, release(0x2Au)), ok_nodata);
  CHECK_STATUS(consumer.receive(answer), av::Status::ok);
  CHECK(answer.operation() == av::ExportOperation::revoked);
  CHECK_EQ(answer.surface(), 0x2Au);
  CHECK_EQ(answer.reason(), static_cast<std::uint32_t>(av::ExportRevocation::released));
  CHECK_EQ(fixture.plane().shared_count(), 1u); }

TEST(shared_commands, a_request_too_short_or_naming_no_object_is_refused_and_tells_nobody)
{ Fixture fixture;
  GpuRequestBuilder short_request(SharedReleaseRequest::type);
  short_request.u32(0x2Au);
  CHECK_EQ(fixture.send(SharedReleaseRequest::type, short_request), err_invalid_parameter);
  CHECK_EQ(fixture.send(SharedReleaseRequest::type, release(0u)), err_invalid_parameter);
  CHECK_EQ(fixture.plane().shared_count(), 0u); }

TEST(shared_commands, every_other_type_is_left_to_the_rest_of_the_device)
{ Fixture fixture;
  CHECK_EQ(fixture.send(0x0102u, release(0x2Au)), 0u);
  CHECK_EQ(fixture.send(0x02F0u, release(0x2Au)), 0u);
  CHECK_EQ(fixture.plane().shared_count(), 0u); }
