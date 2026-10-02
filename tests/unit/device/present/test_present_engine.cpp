/* The present engine on its own thread over the mock's guest RAM: presents
 * taken in order are moved in order and complete in the page as they do, the
 * present line raised; a refused ticket completes in its turn - with the
 * present before it, or at once with none outstanding - and is counted; a
 * present past the engine's depth, a ticket out of count, an image source and
 * a region outside a surface are refused. Nothing is counted without a page;
 * registering one again or resetting forgets every present and holds
 * nothing. */
#include "device/abi/entry_points.hpp"
#include "device/control/control_request.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/present/present_engine.hpp"
#include "harness/context/recording_context_plane.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"
#include "harness/host/present_scene.hpp"
#include "proto/pages/present_progress.hpp"
#include "proto/resources/surface_point.hpp"

#include <cstdint>
#include <time.h>

namespace av {

/* The engine's thread stopped and started again, so a test can queue past
 * what the thread would already have moved. */
struct PresentEngineProbe {
    static void   pause(PresentEngine &engine) { engine.stop_thread(); }
    static Status resume(PresentEngine &engine, Host &host)
    {
        engine.stopping_ = false;
        return host.start_thread(&AvPresentEngineRun, &engine, engine.thread_);
    }
};

}  // namespace av

using av::ControlRequest;
using av::FenceInterrupts;
using av::GpuRequestBuilder;
using av::MockHost;
using av::MockRequest;
using av::PresentEngine;
using av::PresentEngineProbe;
using av::PresentProgress;
using av::PresentScene;
using av::Status;

namespace {

constexpr std::uint32_t ok_nodata = 0x1100u, err_out_of_memory = 0x1201u, err_invalid_parameter = 0x1205u;

constexpr std::uint8_t gpu[av::PresentFrames::uuid_bytes] = {7};

struct Rig {
    MockHost                  host;
    FenceInterrupts           interrupts;
    PresentScene              scene;
    av::RecordingContextPlane plane;
    av::PresentFrames      snapshots;
    PresentEngine             engine;

    Rig() : host(), interrupts(host), scene(host), plane(), snapshots(plane, gpu), engine(host, interrupts)
    {
        CHECK_STATUS(interrupts.initialize(), Status::ok);
        CHECK_STATUS(interrupts.connect(), Status::ok);
        CHECK_STATUS(engine.initialize(), Status::ok);
    }
    ~Rig()
    {
        engine.detach();
        interrupts.disconnect();
    }

    std::uint32_t send(const GpuRequestBuilder &builder)
    {
        MockRequest    mock(builder.bytes(), builder.size());
        ControlRequest request(host, &mock);
        return engine.queue(request);
    }
    /* Waits up to two seconds for the page to say `completed`. */
    bool reaches(std::uint64_t completed, PresentProgress &progress)
    {
        const timespec pause = {0, 100000};
        for (std::uint32_t waited = 0u; waited < 20000u; waited++) {
            if (scene.progress(progress) && progress.completed() >= completed) return true;
            nanosleep(&pause, nullptr);
        }
        return false;
    }
    std::uint32_t present_lines() { return host.raised(FenceInterrupts::present_line); }
};

}  // namespace

TEST(present_engine, presents_are_moved_in_order_and_complete_in_the_page)
{ Rig rig;
  CHECK_STATUS(rig.engine.attach(PresentScene::engine_page, rig.snapshots), Status::ok);
  CHECK_EQ(rig.host.live_threads(), 1u);
  PresentProgress progress;
  CHECK(rig.scene.progress(progress));
  CHECK_EQ(progress.completed(), 0u);
  CHECK_EQ(rig.send(rig.scene.blt(1u, 0u, 4u)), ok_nodata);
  CHECK_EQ(rig.send(rig.scene.fill(2u, 0xFF0000FFu, 4u, 1u)), ok_nodata);
  CHECK_EQ(rig.send(rig.scene.blt(3u, 60u, 8u)), ok_nodata);
  CHECK(rig.reaches(3u, progress));
  CHECK_EQ(progress.completed(), 3u);
  CHECK_EQ(progress.refused(), 0u);
  CHECK(rig.scene.copied(0u, 4u));
  CHECK_EQ(rig.scene.destination_pixel(0u, 4u), 0xFF0000FFu);
  CHECK(rig.scene.copied(60u, 8u));
  CHECK(rig.present_lines() >= 1u);
  CHECK(rig.present_lines() <= 3u);
  CHECK(rig.engine.invariants_hold());
  /* The moved presents' mappings go when the next is taken. */
  CHECK_EQ(rig.send(rig.scene.blt(4u, 8u, 1u)), ok_nodata);
  CHECK(rig.reaches(4u, progress));
  CHECK(rig.host.live_guest_mappings() <= 1u + 3u);
  rig.engine.detach();
  CHECK_EQ(rig.host.live_threads(), 0u);
  CHECK_EQ(rig.host.live_guest_mappings(), 0u);
  CHECK(rig.engine.invariants_hold()); }

TEST(present_engine, nothing_is_taken_or_counted_without_a_page)
{ Rig rig;
  CHECK_EQ(rig.send(rig.scene.blt(1u, 0u, 1u)), err_invalid_parameter);
  CHECK_STATUS(rig.engine.attach(PresentScene::engine_page, rig.snapshots), Status::ok);
  CHECK_EQ(rig.send(rig.scene.blt(1u, 0u, 1u)), ok_nodata);   /* still the first */
  PresentProgress progress;
  CHECK(rig.reaches(1u, progress));
  CHECK_EQ(progress.refused(), 0u); }

TEST(present_engine, a_refusal_with_nothing_outstanding_completes_at_once)
{ Rig rig;
  CHECK_STATUS(rig.engine.attach(PresentScene::engine_page, rig.snapshots), Status::ok);
  CHECK_EQ(rig.send(rig.scene.blt(5u, 0u, 1u)), err_invalid_parameter);   /* the first is ticket 1 */
  PresentProgress progress;
  CHECK(rig.scene.progress(progress));
  CHECK_EQ(progress.completed(), 1u);
  CHECK_EQ(progress.refused(), 1u);
  CHECK_EQ(rig.present_lines(), 1u);
  CHECK_EQ(rig.send(rig.scene.image(2u, 0x77u)), err_invalid_parameter);   /* nothing reads snapshots here */
  CHECK(rig.scene.progress(progress));
  CHECK_EQ(progress.completed(), 2u);
  CHECK_EQ(progress.refused(), 2u);
  CHECK_EQ(rig.scene.destination_pixel(0u, 0u), 0u);
  CHECK_EQ(rig.send(rig.scene.blt(3u, 0u, 1u)), ok_nodata);
  CHECK(rig.reaches(3u, progress));
  CHECK_EQ(progress.refused(), 2u);
  CHECK(rig.engine.invariants_hold()); }

TEST(present_engine, a_refusal_behind_an_outstanding_present_completes_with_it)
{ Rig rig;
  CHECK_STATUS(rig.engine.attach(PresentScene::engine_page, rig.snapshots), Status::ok);
  PresentEngineProbe::pause(rig.engine);
  CHECK_EQ(rig.send(rig.scene.blt(1u, 0u, 1u)), ok_nodata);
  CHECK_EQ(rig.send(rig.scene.blt(9u, 1u, 1u)), err_invalid_parameter);
  CHECK_EQ(rig.send(rig.scene.blt(3u, 2u, 1u)), ok_nodata);
  rig.host.fail_allocation_at(1u);
  CHECK_EQ(rig.send(rig.scene.blt(4u, 3u, 1u)), err_out_of_memory);
  PresentProgress progress;
  CHECK(rig.scene.progress(progress));
  CHECK_EQ(progress.completed(), 0u);   /* ticket 2 waits for ticket 1 */
  CHECK(rig.engine.invariants_hold());
  CHECK_STATUS(PresentEngineProbe::resume(rig.engine, rig.host), Status::ok);
  CHECK(rig.reaches(4u, progress));
  CHECK_EQ(progress.completed(), 4u);
  CHECK_EQ(progress.refused(), 2u);
  CHECK(rig.scene.copied(0u, 1u));
  CHECK_EQ(rig.scene.destination_pixel(0u, 1u), 0u);
  CHECK(rig.scene.copied(2u, 1u));
  CHECK_EQ(rig.scene.destination_pixel(0u, 3u), 0u); }

TEST(present_engine, a_present_past_the_depth_is_refused_and_completes_in_its_turn)
{ Rig rig;
  CHECK_STATUS(rig.engine.attach(PresentScene::engine_page, rig.snapshots), Status::ok);
  PresentEngineProbe::pause(rig.engine);
  for (std::uint64_t ticket = 1u; ticket <= PresentEngine::depth; ticket++)
      CHECK_EQ(rig.send(rig.scene.blt(ticket, static_cast<std::uint32_t>(ticket), 1u)), ok_nodata);
  CHECK_EQ(rig.send(rig.scene.blt(PresentEngine::depth + 1u, 0u, 1u)), err_invalid_parameter);
  CHECK(rig.engine.invariants_hold());
  CHECK_STATUS(PresentEngineProbe::resume(rig.engine, rig.host), Status::ok);
  PresentProgress progress;
  CHECK(rig.reaches(PresentEngine::depth + 1u, progress));
  CHECK_EQ(progress.refused(), 1u);
  CHECK(rig.scene.copied(1u, PresentEngine::depth));
  CHECK_EQ(rig.scene.destination_pixel(0u, 0u), 0u);
  /* Room again once the moved presents are let go. */
  CHECK_EQ(rig.send(rig.scene.blt(PresentEngine::depth + 2u, 0u, 1u)), ok_nodata);
  CHECK(rig.reaches(PresentEngine::depth + 2u, progress));
  CHECK(rig.scene.copied(0u, 1u)); }

TEST(present_engine, a_region_outside_a_surface_is_counted_refused_though_it_was_taken)
{ Rig rig;
  CHECK_STATUS(rig.engine.attach(PresentScene::engine_page, rig.snapshots), Status::ok);
  CHECK_EQ(rig.send(rig.scene.blt(1u, PresentScene::height - 1u, 2u)), ok_nodata);
  PresentProgress progress;
  CHECK(rig.reaches(1u, progress));
  CHECK_EQ(progress.refused(), 1u); }

TEST(present_engine, a_page_registered_again_counts_from_one_and_a_reset_holds_nothing)
{ Rig rig;
  CHECK_STATUS(rig.engine.attach(PresentScene::engine_page, rig.snapshots), Status::ok);
  CHECK_EQ(rig.send(rig.scene.blt(1u, 0u, 1u)), ok_nodata);
  PresentProgress progress;
  CHECK(rig.reaches(1u, progress));
  CHECK_STATUS(rig.engine.attach(PresentScene::engine_page, rig.snapshots), Status::ok);
  CHECK(rig.scene.progress(progress));
  CHECK_EQ(progress.completed(), 0u);
  CHECK_EQ(rig.host.live_threads(), 1u);
  CHECK_EQ(rig.send(rig.scene.blt(1u, 5u, 1u)), ok_nodata);
  CHECK(rig.reaches(1u, progress));
  CHECK(rig.scene.copied(5u, 1u));
  CHECK_STATUS(rig.engine.attach(0u, rig.snapshots), Status::ok);
  CHECK_EQ(rig.host.live_threads(), 0u);
  CHECK_EQ(rig.host.live_guest_mappings(), 0u);
  CHECK_EQ(rig.send(rig.scene.blt(2u, 0u, 1u)), err_invalid_parameter);
  CHECK(rig.engine.invariants_hold()); }

TEST(present_engine, an_engine_that_cannot_start_holds_nothing)
{ MockHost        host;
  FenceInterrupts interrupts(host);
  PresentScene    scene(host);
  const av::PresentFrames none;
  {
      PresentEngine engine(host, interrupts);
      CHECK_STATUS(engine.attach(PresentScene::engine_page, none), Status::invalid_state);
      engine.detach();   /* nothing to stop */
      host.fail_mutex_at(1u);
      CHECK_STATUS(engine.initialize(), Status::no_memory);
      host.fail_event_at(1u);
      CHECK_STATUS(engine.initialize(), Status::no_memory);
      CHECK_EQ(host.live_events(), 0u);
      CHECK_STATUS(engine.initialize(), Status::ok);
      CHECK_STATUS(engine.initialize(), Status::invalid_state);
      CHECK_STATUS(engine.attach(0x80000000u, none), Status::not_found);
      host.fail_thread_start_at(1u);
      CHECK_STATUS(engine.attach(PresentScene::engine_page, none), Status::no_memory);
      CHECK_EQ(host.live_guest_mappings(), 0u);
      CHECK_EQ(host.live_threads(), 0u);
      CHECK(engine.invariants_hold());
      CHECK_STATUS(engine.attach(PresentScene::engine_page, none), Status::ok);
  }
  CHECK_EQ(host.live_threads(), 0u);   /* destroyed running, its thread stopped */
  CHECK_EQ(host.live_guest_mappings(), 0u);
  CHECK_EQ(host.live_events(), 0u); }

/* An image source is moved out of the snapshot its key names - rows its
 * pitch apart, padding and all - which is let go after; one whose snapshot
 * never comes is counted refused, and one naming a key where nothing reads
 * snapshots is refused at once. */
TEST(present_engine, an_image_source_is_moved_out_of_its_snapshot_and_the_snapshot_let_go)
{ Rig rig;
  constexpr std::uint32_t pitch = 20u * 4u;   /* 16 pixels and 4 of padding a row */
  static std::uint32_t    pixels[20u * 2u];
  for (std::uint32_t at = 0u; at < 40u; at++) pixels[at] = 0xA0000000u | at;
  const AvFrameView view = {reinterpret_cast<const std::uint8_t *>(pixels), pitch, 16u, 2u, 0u};
  CHECK_STATUS(rig.engine.attach(PresentScene::engine_page, rig.snapshots), Status::ok);
  CHECK_EQ(rig.send(rig.scene.image(1u, 0x51u)), 0x1205u);   /* nothing reads snapshots yet */
  rig.plane.serve_frame(av::SurfacePoint(0x52u, 1u), view);
  CHECK_STATUS(rig.engine.attach(PresentScene::engine_page, rig.snapshots), Status::ok);
  CHECK_EQ(rig.send(rig.scene.image(1u, 0x52u)), ok_nodata);
  PresentProgress progress;
  CHECK(rig.reaches(1u, progress));
  CHECK_EQ(progress.refused(), 0u);
  for (std::uint32_t x = 0u; x < 16u; x++) CHECK_EQ(rig.scene.destination_pixel(x, 0u), 0xA0000000u | x);
  CHECK_EQ(rig.scene.destination_pixel(0u, 1u), 0u);
  CHECK(!rig.plane.snapshot_mapped());
  CHECK_EQ(rig.plane.unmaps(), 1u);
  CHECK_EQ(rig.send(rig.scene.image(2u, 0x53u)), ok_nodata);   /* a key no session took */
  CHECK(rig.reaches(2u, progress));
  CHECK_EQ(progress.refused(), 1u);
  CHECK(rig.engine.invariants_hold()); }
