#include "executor/opengl/gl_fence_deferral.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/opengl/gl_protocol_ids.h"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <initializer_list>

using av::GlFenceDeferral;

namespace {

struct Payload {
    std::uint8_t   bytes[32] = {};
    av::WireWriter writer{bytes, sizeof bytes};
    Payload(std::initializer_list<std::uint64_t> values) { for (const std::uint64_t value : values) writer.write_varint(value); }
    av::WireReader reader() const { return av::WireReader(bytes, writer.offset()); }
};

constexpr std::uint64_t gl_blend = 0x0BE2u, multiply = 0x9294u, hsl_luminosity = 0x92B0u, func_add = 0x8006u,
                        samples_passed = 0x8914u;

void run(GlFenceDeferral &state, std::uint32_t command, std::initializer_list<std::uint64_t> values)
{ const Payload payload(values);
  state.ran(command, payload.reader()); }

}

TEST(gl_fence_deferral, advanced_blending_defers_only_while_blend_is_on_and_the_equation_is_advanced)
{ GlFenceDeferral state;
  CHECK(!state.defers());
  run(state, av::glproto::kCmd_glBlendEquation, {multiply});
  CHECK(!state.defers());
  run(state, av::glproto::kCmd_glEnable, {gl_blend});
  CHECK(state.defers());
  CHECK(state.advanced_blending());
  run(state, av::glproto::kCmd_glBlendEquation, {func_add});
  CHECK(!state.defers());
  run(state, av::glproto::kCmd_glBlendEquationSeparate, {hsl_luminosity, func_add});
  CHECK(state.defers());
  run(state, av::glproto::kCmd_glDisable, {gl_blend});
  CHECK(!state.defers());
  CHECK(state.invariants_hold()); }

TEST(gl_fence_deferral, a_draw_buffer_with_an_advanced_equation_and_blending_defers_by_itself)
{ GlFenceDeferral state;
  run(state, av::glproto::kCmd_glBlendEquationi, {1u, multiply});
  run(state, av::glproto::kCmd_glEnablei, {gl_blend, 0u});
  CHECK(!state.defers());
  run(state, av::glproto::kCmd_glEnablei, {gl_blend, 1u});
  CHECK(state.defers());
  run(state, av::glproto::kCmd_glDisablei, {gl_blend, 1u});
  CHECK(!state.defers());
  run(state, av::glproto::kCmd_glEnablei, {gl_blend, 9u});
  run(state, av::glproto::kCmd_glBlendEquationSeparatei, {1u, multiply, func_add});
  run(state, av::glproto::kCmd_glEnable, {0x0B71u});
  CHECK(!state.defers() || state.advanced_blending()); }

TEST(gl_fence_deferral, an_open_occlusion_query_defers_as_well_and_both_reasons_must_end)
{ GlFenceDeferral state;
  run(state, av::glproto::kCmd_glBeginQuery, {samples_passed, 1u});
  CHECK(state.defers());
  CHECK(state.occlusion_open());
  run(state, av::glproto::kCmd_glBlendEquation, {multiply});
  run(state, av::glproto::kCmd_glEnable, {gl_blend});
  run(state, av::glproto::kCmd_glEndQuery, {samples_passed});
  CHECK(state.defers());
  run(state, av::glproto::kCmd_glDisable, {gl_blend});
  CHECK(!state.defers()); }

TEST(gl_fence_deferral, commands_that_wait_for_or_hand_over_the_gpu_demand_the_owed_fence)
{ GlFenceDeferral state;
  for (const std::uint32_t command :
       {av::glproto::kCmd_glFinish, av::glproto::kCmd_glFlush, av::glproto::kCmd_glClientWaitSync,
        av::glproto::kCmd_glWaitSync, av::glproto::kCmd_glFenceSync, av::glproto::kCmd_glSignalSemaphoreEXT,
        av::glproto::kCmd_glReadPixels, av::glproto::kCmd_glGetTextureSubImage, av::glproto::kCmd_glGetBufferSubData})
    CHECK(state.demands(command));
  CHECK(!state.demands(av::glproto::kCmd_glDrawArrays));
  CHECK(!state.demands(av::glproto::kCmd_glBufferData)); }

TEST(gl_fence_deferral, a_deferral_that_has_run_for_64_batches_stops_so_a_waiting_guest_is_never_starved)
{ GlFenceDeferral state;
  run(state, av::glproto::kCmd_glBlendEquation, {multiply});
  run(state, av::glproto::kCmd_glEnable, {gl_blend});
  for (std::uint32_t batch = 0u; batch < GlFenceDeferral::most_owed; batch++) {
    CHECK(state.defers());
    state.owe();
  }
  CHECK(!state.defers());
  CHECK_EQ(state.owed(), GlFenceDeferral::most_owed);
  state.paid();
  CHECK(state.defers());
  CHECK_EQ(state.owed(), 0u);
  state.clear();
  CHECK(!state.defers()); }

TEST(gl_fence_deferral, truncated_payloads_change_nothing)
{ GlFenceDeferral state;
  const std::uint8_t one = 1u;
  state.ran(av::glproto::kCmd_glEnablei, av::WireReader(&one, 1u));
  state.ran(av::glproto::kCmd_glBlendEquationSeparatei, av::WireReader(&one, 1u));
  state.ran(av::glproto::kCmd_glEnable, av::WireReader(nullptr, 0u));
  CHECK(!state.defers()); }
