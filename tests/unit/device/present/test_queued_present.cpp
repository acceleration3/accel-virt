#include "device/control/control_request.hpp"
#include "device/present/present_frames.hpp"
#include "device/present/queued_present.hpp"
#include "harness/context/recording_context_plane.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"
#include "harness/host/present_scene.hpp"
#include "proto/resources/surface_point.hpp"
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

const av::PresentFrames none;

Status accept(MockHost &host, QueuedPresent &present, const GpuRequestBuilder &builder,
              const av::PresentFrames &snapshots = none)
{
    MockRequest          mock(builder.bytes(), builder.size());
    ControlRequest       request(host, &mock);
    WireReader           reader(builder.bytes(), builder.size());
    QueuedPresentRequest decoded;
    const Status         status = decoded.decode(reader);
    return status == Status::ok ? present.accept(request, decoded, snapshots) : status;
}

}

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
  CHECK_STATUS(present.move(none), Status::ok);
  CHECK(scene.copied(60u, 8u));
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
  CHECK_STATUS(present.move(none), Status::ok);
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
  cut.truncate(cut.size() - 1u);
  CHECK_STATUS(accept(host, present, cut), Status::invalid_parameter);
  CHECK_EQ(host.live_guest_mappings(), 0u);
  GpuRequestBuilder no_list = scene.blt(1u, 0u, 1u);
  no_list.truncate(QueuedPresentRequest::encoded_size + 16u);
  CHECK_STATUS(accept(host, present, no_list), Status::invalid_parameter);
  host.fail_allocation_at(1u);
  CHECK_STATUS(accept(host, present, scene.blt(1u, 0u, 1u)), Status::no_memory);
  host.fail_allocation_at(2u);
  CHECK_STATUS(accept(host, present, scene.blt(1u, 0u, 1u)), Status::no_memory);
  CHECK_EQ(present.ticket(), 0u);
  CHECK_EQ(host.live_guest_mappings(), 0u);
  CHECK(present.invariants_hold()); }

TEST(queued_present, a_region_outside_a_surface_is_refused_when_the_move_reaches_it)
{ MockHost      host;
  PresentScene  scene(host);
  QueuedPresent present(host);
  CHECK_STATUS(accept(host, present, scene.blt(3u, PresentScene::height - 1u, 2u)), Status::ok);
  CHECK_STATUS(present.move(none), Status::invalid_parameter);
  CHECK(scene.copied(PresentScene::height - 1u, 1u));
  present.release();
  CHECK_EQ(host.live_guest_mappings(), 0u); }

TEST(queued_present, an_image_source_maps_its_destination_and_moves_out_of_its_snapshot)
{ MockHost                  host;
  PresentScene              scene(host);
  av::RecordingContextPlane plane;
  constexpr std::uint8_t    gpu[av::PresentFrames::uuid_bytes] = {9};
  const av::PresentFrames snapshots(plane, gpu);
  static std::uint32_t      pixels[PresentScene::width];
  for (std::uint32_t x = 0u; x < PresentScene::width; x++) pixels[x] = 0x0000FF00u | x;
  plane.serve_frame(av::SurfacePoint(0x61u, 1u), AvFrameView{reinterpret_cast<const std::uint8_t *>(pixels), sizeof pixels,
                                              PresentScene::width, 1u, 0u});
  QueuedPresent present(host);
  CHECK_STATUS(accept(host, present, scene.image(4u, 0x61u), snapshots), Status::ok);
  CHECK_EQ(host.live_guest_mappings(), 1u);
  CHECK_STATUS(present.move(snapshots), Status::ok);
  CHECK_EQ(scene.destination_pixel(3u, 0u), 0x0000FF03u);
  CHECK_EQ(plane.unmaps(), 1u);
  CHECK_STATUS(present.move(snapshots), Status::not_found);
  present.release();
  QueuedPresent outside(host);
  static std::uint32_t row[4];
  plane.serve_frame(av::SurfacePoint(0x62u, 1u), AvFrameView{reinterpret_cast<const std::uint8_t *>(row), sizeof row, 4u, 1u, 0u});
  CHECK_STATUS(accept(host, outside, scene.image(5u, 0x62u), snapshots), Status::ok);
  CHECK_STATUS(outside.move(snapshots), Status::invalid_parameter);
  CHECK_EQ(plane.unmaps(), 2u);
  outside.release();
  CHECK_EQ(host.live_guest_mappings(), 0u); }
