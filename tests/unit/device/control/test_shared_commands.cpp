/* The driver's own release of a shared object, request byte by request byte:
 * a well-formed one tells the contexts with this device's host GPU and is
 * answered, a malformed one is refused and tells nobody, and every other type
 * is left to the rest of the device. */
#include "device/control/control_request.hpp"
#include "device/control/shared_commands.hpp"
#include "harness/context/recording_context_plane.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/gpu_request_builder.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"
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

class Fixture {
public:
    Fixture() : commands_(host_, plane_, gpu) {}

    /* The response type, or 0 when the core left the request to the rest. */
    std::uint32_t send(std::uint32_t type, const GpuRequestBuilder &builder)
    {
        MockRequest    mock(builder.bytes(), builder.size());
        ControlRequest request(host_, &mock);
        return commands_.answer(request, type) ? mock.response_type() : 0u;
    }

    av::RecordingContextPlane &plane() { return plane_; }

private:
    MockHost                  host_;
    av::RecordingContextPlane plane_;
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
