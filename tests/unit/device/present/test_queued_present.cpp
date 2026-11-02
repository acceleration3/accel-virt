#include "device/control/control_request.hpp"
#include "device/memory/guest_ram_lending.hpp"
#include "device/memory/guest_ram_lendings.hpp"
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
#include <sys/mman.h>
#include <unistd.h>

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
              const av::PresentFrames &snapshots = none, av::GuestRamLendings *lent = nullptr)
{
    MockRequest          mock(builder.bytes(), builder.size());
    ControlRequest       request(host, &mock);
    WireReader           reader(builder.bytes(), builder.size());
    QueuedPresentRequest decoded;
    av::GuestRamLendings none_lent(host);
    const Status         status = decoded.decode(reader);
    return status == Status::ok ? present.accept(request, decoded, snapshots, lent != nullptr ? *lent : none_lent)
                                : status;
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


namespace {
av::GuestRamLending *lend_destination(MockHost &host, av::GuestRamLendings &lendings, av::RecordingContextPlane &lender)
{
    const int memory = ::memfd_create("lent", MFD_CLOEXEC);
    CHECK(memory >= 0 && ::ftruncate(memory, static_cast<off_t>(2u * PresentScene::page_bytes)) == 0);
    av::GuestRamLending *const lending = lendings.lend(host, 2u * PresentScene::page_bytes, memory, lender, 3u);
    CHECK(lending != nullptr);
    GpuRequestBuilder list(0u);
    list.u64(PresentScene::ram + 8u * PresentScene::page_bytes)
        .u32(static_cast<std::uint32_t>(2u * PresentScene::page_bytes))
        .u32(0u);
    MockRequest    mock(list.bytes(), list.size());
    ControlRequest request(host, &mock);
    CHECK_STATUS(lending->lay(host, request, 24u, 1u), Status::ok);
    return lending;
}

std::uint32_t pixels[PresentScene::width];

AvFrameView snapshot_view()
{
    for (std::uint32_t x = 0u; x < PresentScene::width; x++) pixels[x] = 0x0000FF00u | x;
    return AvFrameView{reinterpret_cast<const std::uint8_t *>(pixels), sizeof pixels, PresentScene::width, 1u, 0u};
}

}

TEST(queued_present, a_snapshot_over_lent_pages_is_copied_by_the_gpu_or_moved_by_the_cpu)
{ MockHost                  host;
  PresentScene              scene(host);
  av::RecordingContextPlane plane;
  av::RecordingContextPlane lender;
  constexpr std::uint8_t    gpu[av::PresentFrames::uuid_bytes] = {9};
  const av::PresentFrames   snapshots(plane, gpu);
  av::GuestRamLendings      lendings(host);
  av::GuestRamLending *const lending = lend_destination(host, lendings, lender);
  plane.serve_frame(av::SurfacePoint(0x61u, 1u), snapshot_view());
  plane.answer_copies_with(Status::ok);
  QueuedPresent present(host);
  CHECK_STATUS(accept(host, present, scene.image(4u, 0x61u), snapshots, &lendings), Status::ok);
  CHECK(present.invariants_hold());
  CHECK_STATUS(present.move(snapshots), Status::ok);
  CHECK_EQ(plane.copies(), 1u);
  CHECK_EQ(plane.last_copy().pitch, PresentScene::pitch);
  CHECK_EQ(plane.last_copy().lending, lending->id());
  CHECK_EQ(plane.last_copy().oldest_live, lending->id());
  CHECK_EQ(plane.last_copy().offset, 0u);
  CHECK_EQ(plane.last_copy().size, 2u * PresentScene::page_bytes);
  CHECK_EQ(plane.last_copy().count, 1u);
  CHECK_EQ(plane.last_copy().regions[0].width, PresentScene::width);
  CHECK_EQ(plane.unmaps(), 1u);
  CHECK_EQ(scene.destination_pixel(3u, 0u), 0u);
  CHECK(present.fallback() == QueuedPresent::Fallback::none);
  present.release();

  plane.serve_frame(av::SurfacePoint(0x62u, 1u), snapshot_view());
  plane.answer_copies_with(Status::not_supported);
  QueuedPresent declined(host);
  CHECK_STATUS(accept(host, declined, scene.image(5u, 0x62u), snapshots, &lendings), Status::ok);
  CHECK_STATUS(declined.move(snapshots), Status::ok);
  CHECK_EQ(plane.copies(), 2u);
  CHECK_EQ(scene.destination_pixel(3u, 0u), 0x0000FF03u);
  CHECK(declined.fallback() == QueuedPresent::Fallback::executor_declined);
  declined.release();

  QueuedPresent unlent(host);
  plane.serve_frame(av::SurfacePoint(0x63u, 1u), snapshot_view());
  CHECK_STATUS(accept(host, unlent, scene.image(6u, 0x63u), snapshots), Status::ok);
  CHECK_STATUS(unlent.move(snapshots), Status::ok);
  CHECK_EQ(plane.copies(), 2u);
  CHECK(unlent.fallback() == QueuedPresent::Fallback::not_lent);
  unlent.release();
  lendings.leave(host, *lending, lender, 3u);
  CHECK_EQ(host.live_guest_mappings(), 0u); }

TEST(queued_present, regions_that_would_wrap_the_destination_are_never_sent_to_the_gpu)
{ MockHost                  host;
  PresentScene              scene(host);
  av::RecordingContextPlane plane;
  av::RecordingContextPlane lender;
  constexpr std::uint8_t    gpu[av::PresentFrames::uuid_bytes] = {9};
  const av::PresentFrames   snapshots(plane, gpu);
  av::GuestRamLendings      lendings(host);
  av::GuestRamLending *const lending = lend_destination(host, lendings, lender);
  plane.answer_copies_with(Status::ok);
  GpuRequestBuilder wrapped(QueuedPresentRequest::type);
  wrapped.u64(7u).u32(1u).u32(1u).u64(0x64u);
  wrapped.u32(1u).u32(0u).u32(0u).u32(1u).u32(0u).u32(1u).u32(0u).u32(0xFFFFFFFCu);
  wrapped.u64(PresentScene::ram + 8u * PresentScene::page_bytes).u32(static_cast<std::uint32_t>(2u * PresentScene::page_bytes)).u32(0u);
  wrapped.u32(0u).u32(0u).u32(0u).u32(0xFFFFFFFFu).u32(4u).u32(6u);
  plane.serve_frame(av::SurfacePoint(0x64u, 1u), snapshot_view());
  QueuedPresent present(host);
  CHECK_STATUS(accept(host, present, wrapped, snapshots, &lendings), Status::ok);
  CHECK_STATUS(present.move(snapshots), Status::invalid_parameter);
  CHECK_EQ(plane.copies(), 0u);
  CHECK(present.fallback() == QueuedPresent::Fallback::unplaceable);
  present.release();
  GpuRequestBuilder low(QueuedPresentRequest::type);
  low.u64(8u).u32(1u).u32(1u).u64(0x65u);
  low.u32(1u).u32(0u).u32(0u).u32(1u).u32(0u).u32(1u).u32(0u).u32(PresentScene::pitch);
  low.u64(PresentScene::ram + 8u * PresentScene::page_bytes).u32(static_cast<std::uint32_t>(2u * PresentScene::page_bytes)).u32(0u);
  low.u32(0u).u32(0u).u32(0u).u32(PresentScene::height * 2u).u32(4u).u32(1u);
  plane.serve_frame(av::SurfacePoint(0x65u, 1u), snapshot_view());
  QueuedPresent beyond(host);
  CHECK_STATUS(accept(host, beyond, low, snapshots, &lendings), Status::ok);
  CHECK_STATUS(beyond.move(snapshots), Status::invalid_parameter);
  CHECK_EQ(plane.copies(), 0u);
  beyond.release();
  lendings.leave(host, *lending, lender, 3u);
  CHECK_EQ(host.live_guest_mappings(), 0u); }

TEST(queued_present, a_lending_given_back_before_the_move_is_not_copied_into)
{ MockHost                  host;
  PresentScene              scene(host);
  av::RecordingContextPlane plane;
  av::RecordingContextPlane lender;
  constexpr std::uint8_t    gpu[av::PresentFrames::uuid_bytes] = {9};
  const av::PresentFrames   snapshots(plane, gpu);
  av::GuestRamLendings      lendings(host);
  av::GuestRamLending *const lending = lend_destination(host, lendings, lender);
  plane.answer_copies_with(Status::ok);
  plane.serve_frame(av::SurfacePoint(0x66u, 1u), snapshot_view());
  QueuedPresent present(host);
  CHECK_STATUS(accept(host, present, scene.image(9u, 0x66u), snapshots, &lendings), Status::ok);
  lendings.leave(host, *lending, lender, 3u);
  CHECK_EQ(lendings.live(), 0u);
  CHECK_STATUS(present.move(snapshots), Status::ok);
  CHECK_EQ(plane.copies(), 0u);
  CHECK(present.fallback() == QueuedPresent::Fallback::lending_gone);
  CHECK_EQ(scene.destination_pixel(3u, 0u), 0x0000FF03u);
  present.release();
  CHECK_EQ(host.live_guest_mappings(), 0u); }
