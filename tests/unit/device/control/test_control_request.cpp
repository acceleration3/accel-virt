#include "device/control/control_request.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"

#include <cstdint>

using av::ControlRequest;
using av::GpuResponse;
using av::HostViolation;
using av::MockHost;
using av::MockRequest;

TEST(control_request, reads_the_request_and_responds_once)
{ MockHost host;
  const std::uint8_t bytes[] = {1, 2, 3, 4, 5};
  MockRequest mock(bytes, sizeof bytes);
  ControlRequest request(host, &mock);
  std::uint8_t read[8] = {};
  CHECK_EQ(request.read(1u, read, sizeof read), 4u);
  CHECK_EQ(read[0], 2u);
  CHECK_EQ(request.read(9u, read, sizeof read), 0u);
  CHECK(!request.responded());
  GpuResponse response;
  response.set(GpuResponse::err_invalid_parameter);
  request.respond(response);
  CHECK(request.responded());
  CHECK_EQ(mock.response_type(), GpuResponse::err_invalid_parameter);
  response.set(GpuResponse::ok_nodata);
  request.respond(response);
  CHECK_EQ(mock.response_type(), GpuResponse::err_invalid_parameter); }

TEST(control_request, is_main_loop_only)
{ MockHost host;
  const std::uint8_t bytes[] = {0};
  MockRequest mock(bytes, sizeof bytes);
  struct Call {
      MockHost    *host;
      MockRequest *mock;
  } call = {&host, &mock};
  void *thread = nullptr;
  CHECK_STATUS(host.start_thread(
                   [](void *context) {
                       auto *const c = static_cast<Call *>(context);
                       ControlRequest request(*c->host, c->mock);
                       std::uint8_t byte = 0u;
                       static_cast<void>(request.read(0u, &byte, 1u));
                       request.respond(GpuResponse());
                   },
                   &call, thread),
               av::Status::ok);
  host.join_thread(thread);
  for (int violation = 0; violation < 4; violation++) CHECK(host.claim(HostViolation::main_loop_required)); }
