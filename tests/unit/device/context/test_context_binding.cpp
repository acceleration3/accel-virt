#include "core/support/status.hpp"
#include "device/context/context_binding.hpp"
#include "device/control/control_request.hpp"
#include "device/fence/fence_window.hpp"
#include "device/memory/blob_table.hpp"
#include "device/ring/ring_window.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/gpu_request_builder.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"

#include <cstdint>

using av::BlobTable;
using av::CapsetInfo;
using av::ContextBinding;
using av::ControlRequest;
using av::GpuRequestBuilder;
using av::ContextSetup;
using av::GpuIdentity;
using av::HostViolation;
using av::MockHost;
using av::MockRequest;
using av::ProtocolVersion;
using av::Status;
using av::SubApi;

namespace {

constexpr std::uint32_t ring = 10u, fence = 11u, small_ring = 12u, tiny = 13u;
constexpr std::uint8_t  uuid[GpuIdentity::uuid_bytes] = {1};

CapsetInfo capset(std::uint16_t minor = 0u, std::uint8_t max_order = 24u)
{
    return CapsetInfo(ProtocolVersion(1u, minor), static_cast<std::uint32_t>(SubApi::vulkan), 64u, 12u, max_order, 0u,
                      GpuIdentity(1u, 2u, GpuIdentity::discrete_gpu, uuid, 3u, 4u, "gpu"));
}

ContextSetup setup(std::uint32_t ring_id = ring, std::uint32_t fence_id = fence, std::uint8_t order = 12u,
                   ProtocolVersion version = ProtocolVersion(1u, 0u), SubApi api = SubApi::vulkan,
                   std::uint32_t doorbell = 9u)
{
    return ContextSetup(version, api, ring_id, order, fence_id, doorbell);
}

class Fixture {
public:
    Fixture() : blobs_(host_), binding_(host_, blobs_)
    {
        host_.add_guest_ram(0x100000u, 0x1000u);
        static_cast<void>(blobs_.create_host(ring, 0x2000u));
        static_cast<void>(blobs_.create_host(fence, 0x1000u));
        static_cast<void>(blobs_.create_host(small_ring, 0x1000u));
    }

    MockHost       &host() { return host_; }
    BlobTable      &blobs() { return blobs_; }
    ContextBinding &binding() { return binding_; }

private:
    MockHost       host_;
    BlobTable      blobs_;
    ContextBinding binding_;
};

}  // namespace

TEST(context_binding_bind, records_the_setup_at_the_negotiated_version_and_pins_both_blobs)
{ Fixture fixture;
  CHECK(!fixture.binding().bound());
  CHECK_STATUS(fixture.binding().bind(setup(ring, fence, 12u, ProtocolVersion(1u, 3u)), capset(2u)), Status::ok);
  CHECK(fixture.binding().bound());
  CHECK_EQ(fixture.binding().version().major_version(), 1u);
  CHECK_EQ(fixture.binding().version().minor_version(), 2u);
  CHECK(fixture.binding().sub_api() == SubApi::vulkan);
  CHECK_EQ(fixture.binding().ring_resource(), ring);
  CHECK_EQ(fixture.binding().ring_order(), 12u);
  CHECK_EQ(fixture.binding().fence_resource(), fence);
  CHECK_EQ(fixture.binding().doorbell(), 9u);
  CHECK_EQ(fixture.binding().ring_window().capacity(), 0x1000u);
  CHECK(fixture.binding().ring_window().invariants_hold());
  CHECK(fixture.binding().fence_window().invariants_hold());
  CHECK_STATUS(fixture.blobs().release(ring), Status::invalid_state);
  CHECK_STATUS(fixture.blobs().release(fence), Status::invalid_state);
  CHECK_STATUS(fixture.binding().bind(setup(), capset()), Status::invalid_state);
  fixture.binding().unbind();
  CHECK(!fixture.binding().bound());
  CHECK_EQ(fixture.binding().ring_window().capacity(), 0u);
  CHECK(!fixture.binding().fence_window().invariants_hold());
  fixture.binding().unbind();
  CHECK_STATUS(fixture.blobs().release(ring), Status::ok);
  CHECK_STATUS(fixture.blobs().release(fence), Status::ok); }

TEST(context_binding_bind, a_setup_the_capset_does_not_allow_is_not_supported)
{ Fixture fixture;
  CHECK_STATUS(fixture.binding().bind(setup(ring, fence, 12u, ProtocolVersion(2u, 0u)), capset()),
               Status::not_supported);
  CHECK_STATUS(fixture.binding().bind(setup(ring, fence, 12u, ProtocolVersion(1u, 0u), SubApi::opengl), capset()),
               Status::not_supported);
  CHECK_STATUS(fixture.binding().bind(setup(ring, fence, 13u), capset(0u, 12u)), Status::not_supported);
  CHECK(!fixture.binding().bound());
  CHECK_STATUS(fixture.blobs().release(ring), Status::ok); }

TEST(context_binding_bind, a_missing_or_undersized_blob_pins_nothing)
{ Fixture fixture;
  CHECK_STATUS(fixture.binding().bind(setup(99u, fence), capset()), Status::not_found);
  CHECK_STATUS(fixture.binding().bind(setup(small_ring, fence), capset()), Status::invalid_parameter);
  CHECK_STATUS(fixture.binding().bind(setup(ring, fence, 13u), capset()), Status::invalid_parameter);
  CHECK_STATUS(fixture.binding().bind(setup(ring, 99u), capset()), Status::not_found);
  CHECK_STATUS(fixture.blobs().release(ring), Status::ok);
  CHECK(!fixture.binding().bound()); }

TEST(context_binding_bind, a_fence_blob_smaller_than_the_fence_page_is_refused_and_the_ring_let_go)
{ Fixture fixture;
  GpuRequestBuilder entries(0u);
  entries.u64(0x100000u).u32(16u).u32(0u);
  MockRequest mock(entries.bytes(), entries.size());
  ControlRequest request(fixture.host(), &mock);
  CHECK_STATUS(fixture.blobs().create_guest(tiny, 16u, request, 24u, 1u), Status::ok);
  CHECK_STATUS(fixture.binding().bind(setup(ring, tiny), capset()), Status::invalid_parameter);
  CHECK_STATUS(fixture.blobs().release(ring), Status::ok);
  CHECK_STATUS(fixture.blobs().release(tiny), Status::ok); }

TEST(context_binding, lets_go_of_its_blobs_when_destroyed)
{ MockHost host;
  BlobTable blobs(host);
  CHECK_STATUS(blobs.create_host(ring, 0x2000u), Status::ok);
  CHECK_STATUS(blobs.create_host(fence, 0x1000u), Status::ok);
  { ContextBinding binding(host, blobs);
    CHECK_STATUS(binding.bind(setup(), capset()), Status::ok); }
  CHECK_STATUS(blobs.release(ring), Status::ok);
  CHECK_STATUS(blobs.release(fence), Status::ok); }

TEST(context_binding, is_main_loop_only)
{ Fixture fixture;
  void *thread = nullptr;
  CHECK_STATUS(fixture.host().start_thread(
                   [](void *context) {
                       auto *const binding = static_cast<ContextBinding *>(context);
                       static_cast<void>(binding->bind(setup(), capset(0u, 11u)));
                       binding->unbind();
                   },
                   &fixture.binding(), thread),
               Status::ok);
  fixture.host().join_thread(thread);
  CHECK(fixture.host().claim(HostViolation::main_loop_required));
  CHECK(fixture.host().claim(HostViolation::main_loop_required)); }
