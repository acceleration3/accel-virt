/* One queued present, taken on the main loop and moved later: accepting maps
 * both surfaces and reads the regions out of the request, so the move needs
 * nothing of the request and lands the rows a present copy would; release
 * lets every mapping go and forgets the refusals that followed. A request
 * whose lists or regions do not hold, a host with no memory for the mapping
 * and an image source are refused holding nothing; a region outside a surface
 * is refused where the move reaches it. */
#include "device/control/control_request.hpp"
#include "device/present/queued_present.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"
#include "harness/host/present_scene.hpp"
#include "proto/virtio_gpu/queued_present_request.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstdint>

using av::ControlRequest;
using av::GpuRequestBuilder;
using av::MockHost;
using av::MockRequest;
using av::PresentScene;
using av::QueuedPresent;
using av::QueuedPresentRequest;
using av::Status;
using av::WireReader;

namespace {

/* `builder`'s request accepted into `present`, as the engine hands it over. */
Status accept(MockHost &host, QueuedPresent &present, const GpuRequestBuilder &builder)
{
    MockRequest          mock(builder.bytes(), builder.size());
    ControlRequest       request(host, &mock);
    WireReader           reader(builder.bytes(), builder.size());
    QueuedPresentRequest decoded;
    const Status         status = decoded.decode(reader);
    return status == Status::ok ? present.accept(request, decoded) : status;
}

}  // namespace

TEST(queued_present, a_blt_is_moved_from_the_mappings_alone_after_the_request_is_gone)
{ MockHost      host;
  PresentScene  scene(host);
  QueuedPresent present(host);
  CHECK(present.invariants_hold());
  CHECK_STATUS(accept(host, present, scene.blt(7u, 60u, 8u)), Status::ok);
  CHECK_EQ(present.ticket(), 7u);
  CHECK_EQ(host.live_guest_mappings(), 3u);
  CHECK(present.invariants_hold());
  CHECK(!scene.copied(60u, 8u));
  CHECK_STATUS(present.move(), Status::ok);
  CHECK(scene.copied(60u, 8u));   /* rows 60 to 67 cross the source's pages */
  CHECK_EQ(scene.destination_pixel(0u, 59u), 0u);
  CHECK_EQ(scene.destination_pixel(0u, 68u), 0u);
  present.release();
  CHECK_EQ(present.ticket(), 0u);
  CHECK_EQ(host.live_guest_mappings(), 0u);
  CHECK(present.invariants_hold()); }

TEST(queued_present, a_fill_maps_only_its_destination_and_writes_its_colour)
{ MockHost      host;
  PresentScene  scene(host);
  QueuedPresent present(host);
  CHECK_STATUS(accept(host, present, scene.fill(2u, 0xFF112233u, 3u, 2u)), Status::ok);
  CHECK_EQ(host.live_guest_mappings(), 1u);
  CHECK_STATUS(present.move(), Status::ok);
  CHECK_EQ(scene.destination_pixel(0u, 3u), 0xFF112233u);
  CHECK_EQ(scene.destination_pixel(PresentScene::width - 1u, 4u), 0xFF112233u);
  CHECK_EQ(scene.destination_pixel(0u, 5u), 0u);
  present.release(); }

TEST(queued_present, the_refusals_after_it_are_counted_until_it_is_released)
{ MockHost      host;
  PresentScene  scene(host);
  QueuedPresent present(host);
  CHECK_STATUS(accept(host, present, scene.blt(4u, 0u, 1u)), Status::ok);
  CHECK_EQ(present.refusals_after(), 0u);
  present.follow_with_refusal();
  present.follow_with_refusal();
  CHECK_EQ(present.refusals_after(), 2u);
  CHECK(present.invariants_hold());
  present.release();
  CHECK_EQ(present.refusals_after(), 0u);
  CHECK(present.invariants_hold()); }

TEST(queued_present, what_it_cannot_take_is_refused_holding_nothing)
{ MockHost      host;
  PresentScene  scene(host);
  QueuedPresent present(host);
  CHECK_STATUS(accept(host, present, scene.image(1u, 0x55u)), Status::not_supported);
  GpuRequestBuilder cut = scene.blt(1u, 0u, 1u);
  cut.truncate(cut.size() - 1u);   /* the region is not all there */
  CHECK_STATUS(accept(host, present, cut), Status::invalid_parameter);
  CHECK_EQ(host.live_guest_mappings(), 0u);
  GpuRequestBuilder no_list = scene.blt(1u, 0u, 1u);
  no_list.truncate(QueuedPresentRequest::encoded_size + 16u);   /* the second source entry is not there */
  CHECK_STATUS(accept(host, present, no_list), Status::invalid_parameter);
  host.fail_allocation_at(1u);
  CHECK_STATUS(accept(host, present, scene.blt(1u, 0u, 1u)), Status::no_memory);
  host.fail_allocation_at(2u);   /* the destination's table, after the source's */
  CHECK_STATUS(accept(host, present, scene.blt(1u, 0u, 1u)), Status::no_memory);
  CHECK_EQ(present.ticket(), 0u);
  CHECK_EQ(host.live_guest_mappings(), 0u);
  CHECK(present.invariants_hold()); }

TEST(queued_present, a_region_outside_a_surface_is_refused_when_the_move_reaches_it)
{ MockHost      host;
  PresentScene  scene(host);
  QueuedPresent present(host);
  CHECK_STATUS(accept(host, present, scene.blt(3u, PresentScene::height - 1u, 2u)), Status::ok);
  CHECK_STATUS(present.move(), Status::invalid_parameter);
  CHECK(scene.copied(PresentScene::height - 1u, 1u));   /* the row inside moved first */
  present.release();
  CHECK_EQ(host.live_guest_mappings(), 0u); }
