#include "guest/probe/d3d12_work.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/scenario_steps.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_d3d12.hpp"
#include "harness/guest/scripted_image_sink.hpp"

#include <cstdint>
#include <cstring>
#include <initializer_list>

namespace av {

struct AppPointerCheckProbe {
    static std::uint8_t *pattern(AppPointerCheck &check) { return check.pattern_; }
};

struct D3d12WorkProbe {
    static std::uint8_t *frame(D3d12Work &work) { return work.frame_; }
    static void          read_without_a_device(D3d12Work &work) { work.read_ = true; }
    static void          adapter_without_a_device(D3d12Work &work) { work.adapter_ = 0x99u; }
    static void          level_without_a_device(D3d12Work &work) { work.feature_level_ = 0xC000u; }
    static void          library_without_a_device(D3d12Work &work) { work.serialized_library_ = 8u; }
    static std::uint32_t coloured(const D3d12Work &work) { return work.coloured(); }
    static AppPointerCheck &existing(D3d12Work &work) { return work.existing_; }
};

}

using av::D3d12Work;
using av::D3d12WorkProbe;
using av::GuestReport;
using av::MockKmt;
using av::ScenarioSteps;
using av::ScriptedD3d12;
using av::ScriptedImageSink;
using Call = av::ScriptedD3d12::Call;

namespace {

class D3d12Rig {
public:
    D3d12Rig() : report_(report_memory_, "avd3d12probe"), steps_(kmt_, report_, "d3d12") {}
    D3d12Rig(const D3d12Rig &) = delete;
    D3d12Rig &operator=(const D3d12Rig &) = delete;

    ScriptedD3d12     &d3d12() { return d3d12_; }
    ScriptedImageSink &images() { return images_; }
    GuestReport       &report() { return report_; }

    const char *run()
    {
        { D3d12Work work(d3d12_, images_);
          CHECK(work.invariants_hold());
          work.run(steps_);
          CHECK(work.invariants_hold());
          adapter_ = work.adapter();
          feature_level_ = work.feature_level();
          serialized_library_ = work.serialized_library(); }
        CHECK(!d3d12_.live());
        report_.finish();
        return report_.text();
    }

    std::uint64_t adapter() const { return adapter_; }
    std::uint32_t feature_level() const { return feature_level_; }
    std::uint32_t serialized_library() const { return serialized_library_; }

private:
    ScriptedD3d12     d3d12_;
    ScriptedImageSink images_;
    MockKmt           kmt_;
    av::MockPlatform report_memory_;
    GuestReport       report_{report_memory_};
    ScenarioSteps     steps_;
    std::uint64_t     adapter_ = 0u;
    std::uint32_t     feature_level_ = 0u;
    std::uint32_t     serialized_library_ = 0u;
};

}

TEST(d3d12_work, every_step_passes_and_the_target_is_the_colour_the_list_cleared_it_to)
{ D3d12Rig    rig;
  const char *report = rig.run();
  CHECK(std::strstr(report, "ok d3d12.device") != nullptr);
  CHECK(std::strstr(report, "ok d3d12.feature_level 49152") != nullptr);
  CHECK(std::strstr(report, "ok d3d12.adapter_luid") != nullptr);
  CHECK(std::strstr(report, "ok d3d12.pipeline_library") != nullptr);
  CHECK(std::strstr(report, "ok d3d12.queue") != nullptr);
  CHECK(std::strstr(report, "ok d3d12.target") != nullptr);
  CHECK(std::strstr(report, "ok d3d12.clear") != nullptr);
  CHECK(std::strstr(report, "ok d3d12.fence") != nullptr);
  CHECK(std::strstr(report, "ok d3d12.readback") != nullptr);
  CHECK(std::strstr(report, "ok d3d12.pixels 4096") != nullptr);
  CHECK(std::strstr(report, "ok d3d12.existing_heap") != nullptr);
  CHECK(std::strstr(report, "ok d3d12.existing_alias 1") != nullptr);
  CHECK(std::strstr(report, "ok d3d12.existing_copy 65536") != nullptr);
  CHECK(std::strstr(report, "ok d3d12.write_watch") != nullptr);
  CHECK(std::strstr(report, "ok d3d12.write_watch_pages 2") != nullptr);
  CHECK(std::strstr(report, "avd3d12probe: 15 passed, 0 failed") != nullptr);
  CHECK(std::strstr(report, "ok d3d12.pipeline_library") < std::strstr(report, "ok d3d12.queue"));
  CHECK_EQ(rig.serialized_library(), ScriptedD3d12::library_bytes);
  CHECK_EQ(rig.adapter(), ScriptedD3d12::adapter);
  CHECK_EQ(rig.feature_level(), ScriptedD3d12::feature_level_12_0);
  CHECK_EQ(rig.d3d12().width(), D3d12Work::extent);
  CHECK_EQ(rig.d3d12().calls(Call::release), 1u);
  CHECK(rig.images().size("d3d12-clear") > 0u); }

TEST(d3d12_work, a_target_of_the_wrong_colour_fails_with_the_count)
{ for (unsigned channel = 0u; channel < 3u; channel++) {
      D3d12Rig rig;
      rig.d3d12().miscolour(channel);
      const char *report = rig.run();
      CHECK(std::strstr(report, "FAIL d3d12.pixels") != nullptr);
      CHECK(std::strstr(report, " 4095\n") != nullptr);
      CHECK(std::strstr(report, "avd3d12probe: 14 passed, 1 failed") != nullptr);
  }
  D3d12Rig alpha;
  alpha.d3d12().miscolour(3u);
  CHECK(std::strstr(alpha.run(), "ok d3d12.pixels 4096") != nullptr); }

TEST(d3d12_work, application_memory_that_is_not_the_applications_fails_each_step)
{ { D3d12Rig rig;
    rig.d3d12().map_elsewhere();
    const char *report = rig.run();
    CHECK(std::strstr(report, "FAIL d3d12.existing_alias") != nullptr);
    CHECK(std::strstr(report, "ok d3d12.existing_copy 65536") != nullptr);
    CHECK(std::strstr(report, "avd3d12probe: 14 passed, 1 failed") != nullptr); }
  { D3d12Rig rig;
    rig.d3d12().drop_a_byte();
    const char *report = rig.run();
    CHECK(std::strstr(report, "FAIL d3d12.existing_copy") != nullptr);
    CHECK(std::strstr(report, " 65535\n") != nullptr); }
  for (const std::uint32_t pages : {0u, 1u, 3u, 0xFFFFFFFFu}) {
      D3d12Rig rig;
      rig.d3d12().watch(pages);
      const char *report = rig.run();
      CHECK(std::strstr(report, "FAIL d3d12.write_watch_pages") != nullptr);
      CHECK(std::strstr(report, "avd3d12probe: 14 passed, 1 failed") != nullptr);
  } }

TEST(d3d12_work, a_refused_existing_heap_or_write_watch_is_the_one_failure)
{ constexpr std::int32_t unsupported = static_cast<std::int32_t>(0x887A0004);
  { D3d12Rig rig;
    rig.d3d12().fail(Call::existing_heap, 1u, unsupported);
    const char *report = rig.run();
    CHECK(std::strstr(report, "FAIL d3d12.existing_heap") != nullptr);
    CHECK(std::strstr(report, "existing_alias") == nullptr);
    CHECK(std::strstr(report, "existing_copy") == nullptr);
    CHECK(std::strstr(report, "ok d3d12.write_watch_pages 2") != nullptr);
    CHECK(std::strstr(report, "avd3d12probe: 12 passed, 1 failed") != nullptr); }
  D3d12Rig rig;
  rig.d3d12().fail(Call::write_watch, 1u, unsupported);
  const char *report = rig.run();
  CHECK(std::strstr(report, "FAIL d3d12.write_watch") != nullptr);
  CHECK(std::strstr(report, "write_watch_pages") == nullptr);
  CHECK(std::strstr(report, "avd3d12probe: 13 passed, 1 failed") != nullptr); }

TEST(d3d12_work, a_refusal_at_each_call_stops_the_run_there)
{ constexpr std::int32_t e_fail = static_cast<std::int32_t>(0x80004005);
  struct Refusal {
      Call          call;
      const char   *step;
      std::uint32_t passed;
  };
  const Refusal refusals[] = {
      {Call::create_device, "FAIL d3d12.device", 0u},
      {Call::adapter_luid, "FAIL d3d12.adapter_luid", 2u},
      {Call::pipeline_library, "FAIL d3d12.pipeline_library", 3u},
      {Call::create_queue, "FAIL d3d12.queue", 4u},
      {Call::create_render_target, "FAIL d3d12.target", 5u},
      {Call::clear_and_copy, "FAIL d3d12.clear", 6u},
      {Call::wait, "FAIL d3d12.fence", 7u},
      {Call::read_back, "FAIL d3d12.readback", 8u},
  };
  for (const Refusal &refusal : refusals) {
      D3d12Rig rig;
      rig.d3d12().fail(refusal.call, 1u, e_fail);
      const char *report = rig.run();
      CHECK(std::strstr(report, refusal.step) != nullptr);
      CHECK(std::strstr(report, "2147500037") != nullptr);
      CHECK_EQ(rig.report().passed(), refusal.passed);
      CHECK_EQ(rig.report().failed(), 1u);
      const bool made = refusal.call != Call::create_device;
      CHECK_EQ(rig.d3d12().calls(Call::release), made ? 1u : 0u);
      CHECK_EQ(rig.d3d12().calls(Call::create_queue), refusal.call >= Call::create_queue ? 1u : 0u);
      CHECK_EQ(rig.d3d12().calls(Call::read_back), refusal.call == Call::read_back ? 1u : 0u);
  }
}

TEST(d3d12_work, rounding_is_within_a_step_and_an_impossible_state_is_named)
{ { ScriptedD3d12     d3d12;
    ScriptedImageSink images;
    MockKmt           kmt;
    av::MockPlatform report_memory;
    GuestReport       report(report_memory, "avd3d12probe");
    ScenarioSteps     steps(kmt, report, "d3d12");
    D3d12Work         work(d3d12, images);
    work.run(steps);
    std::uint8_t *const frame = D3d12WorkProbe::frame(work);
    for (std::uint32_t pixel = 0u; pixel < D3d12Work::pixels; pixel++)
        for (std::uint32_t channel = 0u; channel < 3u; channel++) {
            std::uint8_t &byte = frame[pixel * D3d12Work::pixel_bytes + channel];
            byte = static_cast<std::uint8_t>(pixel % 2u == 0u ? byte + 1u : byte - 1u);
        }
    CHECK_EQ(D3d12WorkProbe::coloured(work), D3d12Work::pixels);
    frame[1] = static_cast<std::uint8_t>(frame[1] + 2u);
    CHECK_EQ(D3d12WorkProbe::coloured(work), D3d12Work::pixels - 1u);
    frame[1] = static_cast<std::uint8_t>(frame[1] - 5u);
    CHECK_EQ(D3d12WorkProbe::coloured(work), D3d12Work::pixels - 1u); }

  ScriptedD3d12     d3d12;
  ScriptedImageSink images;
  D3d12Work         work(d3d12, images);
  CHECK(work.invariants_hold());
  D3d12WorkProbe::read_without_a_device(work);
  CHECK(!work.invariants_hold());
  D3d12Work adapter(d3d12, images);
  D3d12WorkProbe::adapter_without_a_device(adapter);
  CHECK(!adapter.invariants_hold());
  D3d12Work level(d3d12, images);
  D3d12WorkProbe::level_without_a_device(level);
  CHECK(!level.invariants_hold());
  D3d12Work library(d3d12, images);
  D3d12WorkProbe::library_without_a_device(library);
  CHECK(!library.invariants_hold());
  D3d12Work pointer(d3d12, images);
  av::AppPointerCheckProbe::pattern(D3d12WorkProbe::existing(pointer))[0] ^= 1u;
  CHECK(!pointer.invariants_hold()); }
