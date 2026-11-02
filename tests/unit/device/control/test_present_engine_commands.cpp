#include "device/control/control_request.hpp"
#include "device/control/present_engine_commands.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/present/present_engine.hpp"
#include "device/present/present_frames.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/gpu_request_builder.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"
#include "harness/host/present_scene.hpp"
#include "proto/pages/present_progress.hpp"

#include <cstdint>
#include <cstring>

using av::ControlRequest;
using av::FenceInterrupts;
using av::GpuRequestBuilder;
using av::MockHost;
using av::MockRequest;
using av::PresentEngine;
using av::PresentEngineCommands;
using av::PresentProgress;
using av::PresentScene;
using av::Status;

namespace {

constexpr std::uint32_t register_page = 0x02F6u, queued_present = 0x02F7u;
constexpr std::uint32_t ok_nodata = 0x1100u, err_out_of_memory = 0x1201u, err_invalid_parameter = 0x1205u;

struct Rig {
    MockHost        host;
    FenceInterrupts interrupts;
    PresentScene    scene;
    PresentEngine   engine;
    av::PresentFrames snapshots;

    Rig() : host(), interrupts(host), scene(host), engine(host, interrupts), snapshots()
    {
        static_cast<void>(engine.initialize());
    }
    ~Rig() { engine.detach(); }

    std::uint32_t send(const GpuRequestBuilder &built, std::uint32_t type)
    {
        MockRequest    mock(built.bytes(), built.size());
        ControlRequest request(host, &mock);
        return PresentEngineCommands(host, engine, snapshots).answer(request, type) ? mock.response_type() : 0u;
    }
    std::uint32_t attach(std::uint64_t address)
    {
        GpuRequestBuilder request(register_page);
        request.u64(address);
        return send(request, register_page);
    }
};

}

TEST(present_engine_commands, a_page_of_guest_ram_is_taken_and_presents_are_queued_to_the_engine)
{ Rig rig;
  CHECK_EQ(rig.attach(PresentScene::engine_page), ok_nodata);
  PresentProgress progress;
  CHECK(rig.scene.progress(progress));
  CHECK_EQ(rig.host.live_threads(), 1u);
  CHECK_EQ(rig.send(rig.scene.blt(1u, 0u, 1u), queued_present), ok_nodata);
  CHECK_EQ(rig.send(rig.scene.blt(7u, 0u, 1u), queued_present), err_invalid_parameter);
  CHECK_EQ(rig.attach(0u), ok_nodata);
  CHECK_EQ(rig.host.live_threads(), 0u);
  CHECK_EQ(rig.send(rig.scene.blt(1u, 0u, 1u), queued_present), err_invalid_parameter); }

TEST(present_engine_commands, a_page_that_cannot_be_taken_is_refused_and_said_why)
{ Rig rig;
  CHECK_EQ(rig.attach(PresentScene::engine_page + 8u), err_invalid_parameter);
  CHECK(std::strstr(rig.host.last_log(), "present engine's page was refused") != nullptr);
  CHECK_EQ(rig.attach(0x80000000u), err_invalid_parameter);
  GpuRequestBuilder cut(register_page);
  cut.u32(0u);
  CHECK_EQ(rig.send(cut, register_page), err_invalid_parameter);
  rig.host.fail_thread_start_at(1u);
  CHECK_EQ(rig.attach(PresentScene::engine_page), err_out_of_memory);
  CHECK_EQ(rig.host.live_threads(), 0u);
  CHECK_EQ(rig.host.live_guest_mappings(), 0u); }

TEST(present_engine_commands, every_other_type_is_left_alone)
{ Rig rig;
  GpuRequestBuilder request(0x02F4u);
  request.u64(PresentScene::engine_page);
  CHECK_EQ(rig.send(request, 0x02F4u), 0u);
  CHECK_EQ(rig.send(request, 0x02F2u), 0u);
  CHECK_EQ(rig.host.live_threads(), 0u); }
