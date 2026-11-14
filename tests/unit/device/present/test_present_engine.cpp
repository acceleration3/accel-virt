#include "device/abi/entry_points.hpp"
#include "device/control/control_request.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/present/present_engine.hpp"
#include "device/present/present_frames.hpp"
#include "harness/context/recording_context_plane.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"
#include "harness/host/present_scene.hpp"
#include "proto/pages/present_progress.hpp"
#include "proto/dma/present_region.hpp"
#include "proto/resources/surface_pair.hpp"
#include "proto/resources/surface_point.hpp"

#include <cstdint>
#include <time.h>

namespace av {

struct PresentEngineProbe {
    static void   pause(PresentEngine &engine) { engine.stop_thread(); }
    static Status resume(PresentEngine &engine, Host &host)
    {
        engine.stopping_ = false;
        return host.start_thread(&AvPresentEngineRun, &engine, engine.thread_);
    }
    static void set_first(PresentEngine &engine, std::uint32_t first) { engine.first_ = first; }
    static void set_moved(PresentEngine &engine, std::uint32_t moved) { engine.moved_ = moved; }
    static void set_held(PresentEngine &engine, std::uint32_t held) { engine.held_ = held; }
    static void set_counted(PresentEngine &engine, std::uint64_t counted) { engine.counted_ = counted; }
    static void set_completed(PresentEngine &engine, std::uint64_t completed) { engine.completed_ = completed; }
    static void set_refused(PresentEngine &engine, std::uint64_t refused) { engine.refused_ = refused; }
    static void set_thread(PresentEngine &engine, void *thread) { engine.thread_ = thread; }
    static void *wake(PresentEngine &engine) { return engine.wake_; }
    static void *lock(PresentEngine &engine) { return engine.lock_; }
    static void set_wake(PresentEngine &engine, void *wake) { engine.wake_ = wake; }
    static void set_lock(PresentEngine &engine, void *lock) { engine.lock_ = lock; }
    static void unconstruct(PresentEngine &engine, std::uint32_t slot) { engine.slots_[slot].destroy(); }
    static Status construct(PresentEngine &engine, std::uint32_t slot)
    {
        return engine.slots_[slot].construct(engine.host_);
    }
    static QueuedPresent &slot(PresentEngine &engine, std::uint32_t slot) { return engine.slots_[slot].object(); }
};

}

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

}

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
  CHECK_EQ(rig.send(rig.scene.blt(1u, 0u, 1u)), ok_nodata);
  PresentProgress progress;
  CHECK(rig.reaches(1u, progress));
  CHECK_EQ(progress.refused(), 0u); }

TEST(present_engine, a_refusal_with_nothing_outstanding_completes_at_once)
{ Rig rig;
  CHECK_STATUS(rig.engine.attach(PresentScene::engine_page, rig.snapshots), Status::ok);
  CHECK_EQ(rig.send(rig.scene.blt(5u, 0u, 1u)), err_invalid_parameter);
  PresentProgress progress;
  CHECK(rig.scene.progress(progress));
  CHECK_EQ(progress.completed(), 1u);
  CHECK_EQ(progress.refused(), 1u);
  CHECK_EQ(rig.present_lines(), 1u);
  CHECK_EQ(rig.send(rig.scene.image(2u, 0x77u)), err_invalid_parameter);
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
  CHECK_EQ(progress.completed(), 0u);
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
      engine.detach();
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
  CHECK_EQ(host.live_threads(), 0u);
  CHECK_EQ(host.live_guest_mappings(), 0u);
  CHECK_EQ(host.live_events(), 0u); }

TEST(present_engine, an_image_source_is_moved_out_of_its_snapshot_and_the_snapshot_let_go)
{ Rig rig;
  constexpr std::uint32_t pitch = 20u * 4u;
  static std::uint32_t    pixels[20u * 2u];
  for (std::uint32_t at = 0u; at < 40u; at++) pixels[at] = 0xA0000000u | at;
  const AvFrameView view = {reinterpret_cast<const std::uint8_t *>(pixels), pitch, 16u, 2u, 0u};
  CHECK_STATUS(rig.engine.attach(PresentScene::engine_page, rig.snapshots), Status::ok);
  CHECK_EQ(rig.send(rig.scene.image(1u, 0x51u)), 0x1205u);
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
  CHECK_EQ(rig.send(rig.scene.image(2u, 0x53u)), ok_nodata);
  CHECK(rig.reaches(2u, progress));
  CHECK_EQ(progress.refused(), 1u);
  CHECK(rig.engine.invariants_hold()); }

TEST(present_frames, a_plane_that_reads_no_frames_or_none_at_all_gives_none)
{ av::PresentFrames none;
  av::SurfacePoint  frame(7u, 1u);
  AvFrameView       view = {};
  CHECK(!none.map(frame, view));
  none.unmap(frame);
  av::RecordingContextPlane plane;
  const av::PresentFrames   blind(plane, gpu);
  CHECK(!blind.readable());
  CHECK(!blind.map(frame, view));
  blind.unmap(frame); }

TEST(present_engine, an_engine_never_initialized_takes_nothing_and_a_lost_wake_event_takes_no_page)
{ MockHost        host;
  FenceInterrupts interrupts(host);
  PresentScene    scene(host);
  av::RecordingContextPlane plane;
  av::PresentFrames snapshots(plane, gpu);
  PresentEngine   idle(host, interrupts);
  const GpuRequestBuilder request = scene.blt(1u, 0u, 1u);
  MockRequest             mock(request.bytes(), request.size());
  ControlRequest          control(host, &mock);
  CHECK_EQ(idle.queue(control), err_invalid_parameter);
  CHECK_STATUS(idle.attach(PresentScene::engine_page, snapshots), Status::invalid_state);
  Rig rig;
  void *const wake = PresentEngineProbe::wake(rig.engine);
  PresentEngineProbe::set_wake(rig.engine, nullptr);
  CHECK_STATUS(rig.engine.attach(PresentScene::engine_page, rig.snapshots), Status::invalid_state);
  PresentEngineProbe::set_wake(rig.engine, wake);
  CHECK_STATUS(rig.engine.attach(PresentScene::engine_page, rig.snapshots), Status::ok); }

TEST(present_engine, the_invariant_catches_a_ring_of_slots_a_tally_or_a_thread_that_cannot_hold)
{ Rig rig;
  CHECK(rig.engine.invariants_hold());
  PresentEngineProbe::set_first(rig.engine, PresentEngine::depth);
  CHECK(!rig.engine.invariants_hold());
  PresentEngineProbe::set_first(rig.engine, 0u);
  PresentEngineProbe::set_moved(rig.engine, 1u);
  CHECK(!rig.engine.invariants_hold());
  PresentEngineProbe::set_moved(rig.engine, 0u);
  PresentEngineProbe::set_held(rig.engine, PresentEngine::depth + 1u);
  CHECK(!rig.engine.invariants_hold());
  PresentEngineProbe::set_held(rig.engine, 0u);
  PresentEngineProbe::set_completed(rig.engine, 1u);
  PresentEngineProbe::set_refused(rig.engine, 2u);
  CHECK(!rig.engine.invariants_hold());
  PresentEngineProbe::set_refused(rig.engine, 0u);
  CHECK(!rig.engine.invariants_hold());
  PresentEngineProbe::set_counted(rig.engine, 1u);
  CHECK(rig.engine.invariants_hold());
  PresentEngineProbe::set_completed(rig.engine, 0u);
  PresentEngineProbe::set_counted(rig.engine, 0u);
  PresentEngineProbe::unconstruct(rig.engine, 3u);
  CHECK(!rig.engine.invariants_hold());
  CHECK_STATUS(PresentEngineProbe::construct(rig.engine, 3u), Status::ok);
  CHECK(rig.engine.invariants_hold());
  PresentEngineProbe::slot(rig.engine, 0u).follow_with_refusal();
  CHECK(!rig.engine.invariants_hold());
  PresentEngineProbe::slot(rig.engine, 0u).release();
  void *const lock = PresentEngineProbe::lock(rig.engine);
  void *const wake = PresentEngineProbe::wake(rig.engine);
  PresentEngineProbe::set_thread(rig.engine, &rig);
  PresentEngineProbe::set_lock(rig.engine, nullptr);
  CHECK(!rig.engine.invariants_hold());
  PresentEngineProbe::set_lock(rig.engine, lock);
  PresentEngineProbe::set_wake(rig.engine, nullptr);
  CHECK(!rig.engine.invariants_hold());
  PresentEngineProbe::set_wake(rig.engine, wake);
  CHECK(!rig.engine.invariants_hold());
  PresentEngineProbe::set_thread(rig.engine, nullptr);
  CHECK(rig.engine.invariants_hold()); }

TEST(present_engine, a_surface_copy_completes_its_ticket_after_the_plane_copied_and_an_unknown_key_completes_it_refused)
{ Rig rig;
  CHECK_STATUS(rig.engine.attach(PresentScene::engine_page, rig.snapshots), Status::ok);
  CHECK_EQ(rig.send(rig.scene.surfaces(1u, 0x71u, 0x72u)), 0x1205u);
  rig.plane.copy_through(false);
  CHECK_STATUS(rig.engine.attach(PresentScene::engine_page, rig.snapshots), Status::ok);
  CHECK_EQ(rig.send(rig.scene.surfaces(1u, 0x71u, 0x72u, 3u)), ok_nodata);
  PresentProgress progress;
  CHECK(rig.reaches(1u, progress));
  CHECK_EQ(progress.refused(), 0u);
  CHECK(rig.plane.copied() == av::SurfacePair(0x71u, 0x72u));
  CHECK_EQ(rig.plane.copied_regions(), 3u);
  CHECK_EQ(rig.host.live_guest_mappings(), 1u);
  rig.plane.copy_through(true);
  CHECK_EQ(rig.send(rig.scene.surfaces(2u, 0x73u, 0x72u)), ok_nodata);
  CHECK(rig.reaches(2u, progress));
  CHECK_EQ(progress.refused(), 1u);
  CHECK_EQ(rig.send(rig.scene.blt(3u, 0u, 4u)), ok_nodata);
  CHECK(rig.reaches(3u, progress));
  CHECK_EQ(progress.refused(), 1u);
  CHECK(rig.scene.copied(0u, 4u));
  CHECK(rig.engine.invariants_hold()); }

TEST(present_frames, copies_only_through_a_plane_that_copies_and_only_what_it_can_carry)
{ av::PresentFrames none;
  const av::PresentRegion region(0u, 0u, 0u, 0u, 4u, 4u);
  CHECK(!none.copies());
  CHECK(!none.copy(av::SurfacePair(1u, 2u), &region, 1u));
  av::RecordingContextPlane plane;
  const av::PresentFrames   blind(plane, gpu);
  CHECK(!blind.copies());
  CHECK(!blind.copy(av::SurfacePair(1u, 2u), &region, 1u));
  plane.copy_through(false);
  const av::PresentFrames sighted(plane, gpu);
  CHECK(sighted.copies());
  CHECK(!sighted.copy(av::SurfacePair(1u, 2u), nullptr, 1u));
  CHECK(!sighted.copy(av::SurfacePair(1u, 2u), &region, 0u));
  CHECK(!sighted.copy(av::SurfacePair(1u, 2u), &region, av::PresentFrames::copy_regions + 1u));
  CHECK_EQ(plane.copies_asked(), 0u);
  CHECK(sighted.copy(av::SurfacePair(1u, 2u), &region, 1u));
  CHECK_EQ(plane.copy_first().width, 4u);
  CHECK(!sighted.copy(av::SurfacePair(0u, 2u), &region, 1u)); }

TEST(present_engine, a_texture_source_completes_after_the_host_texture_was_read_and_an_unknown_key_completes_refused)
{ Rig rig;
  static std::uint32_t pixels[PresentScene::width];
  for (std::uint32_t x = 0u; x < PresentScene::width; x++) pixels[x] = 0x00FF0000u | x;
  rig.plane.serve_texture(0x81u, AvFrameView{reinterpret_cast<const std::uint8_t *>(pixels), sizeof pixels,
                                             PresentScene::width, 1u, 0u});
  CHECK_STATUS(rig.engine.attach(PresentScene::engine_page, rig.snapshots), Status::ok);
  CHECK_EQ(rig.send(rig.scene.texture(1u, 0x81u)), ok_nodata);
  PresentProgress progress;
  CHECK(rig.reaches(1u, progress));
  CHECK_EQ(progress.refused(), 0u);
  CHECK_EQ(rig.scene.destination_pixel(5u, 0u), 0x00FF0005u);
  CHECK_EQ(rig.plane.texture_unmaps(), 1u);
  CHECK_EQ(rig.send(rig.scene.texture(2u, 0x99u)), ok_nodata);
  CHECK(rig.reaches(2u, progress));
  CHECK_EQ(progress.refused(), 1u);
  CHECK(rig.engine.invariants_hold()); }

TEST(present_frames, reads_a_texture_only_through_a_plane_that_has_one)
{ av::PresentFrames none;
  AvFrameView       view = {};
  CHECK(!none.map_surface(7u, view));
  none.unmap_surface(7u);
  av::RecordingContextPlane plane;
  const av::PresentFrames   blind(plane, gpu);
  CHECK(!blind.map_surface(7u, view));
  plane.serve_texture(7u, AvFrameView{nullptr, 4u, 1u, 1u, 0u});
  const av::PresentFrames sighted(plane, gpu);
  CHECK(sighted.map_surface(7u, view));
  CHECK(!sighted.map_surface(8u, view));
  sighted.unmap_surface(7u);
  CHECK_EQ(plane.texture_unmaps(), 1u); }
