#include "harness/framework/test_registry.hpp"
#include "harness/icd/gl_icd_rig.hpp"
#include "icd/opengl/gl_icd_root.hpp"
#include "icd/opengl/guest_gl_context.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/escape/av_context_record.hpp"
#include "proto/submission/command_header.hpp"

#include "gl_protocol_ids.h"
#include "gl_types.h"

#include <cstdint>

using av::GlIcdRig;
using av::MockKmt;
using av::Status;
using Root = av::glicd::GlIcdRoot;

namespace {

using ClearFn = void (APIENTRY *)(GLbitfield);
using GetErrorFn = GLenum (APIENTRY *)();
using ResetStatusFn = GLenum (APIENTRY *)();
using ForceFaultFn = std::int32_t (APIENTRY *)();
using AttribsFn = void *(APIENTRY *)(void *, void *, const std::int32_t *);

constexpr std::uint32_t slot_clear = 203u;
constexpr std::uint32_t slot_get_error = 261u;
constexpr GLbitfield    color_buffer_bit = 0x4000u;
constexpr GLenum        gl_out_of_memory = 0x0505u;
constexpr GLenum        gl_context_lost = 0x0507u;
constexpr GLenum        gl_unknown_context_reset = 0x8255u;
constexpr std::int32_t robust_attributes[] = {0x2094, 0x4, 0x8256, 0x8252, 0};

struct LossRig {
    GlIcdRig rig;

    LossRig()
    {
        rig.load_with_format();
        CHECK(rig.opengl32().set_pixel_format(rig.other_dc(), 1));
    }

    void *made_on(std::uint32_t thread)
    {
        rig.threads().switch_to(thread);
        void *const dc = thread == 1u ? rig.dc() : rig.other_dc();
        void *const context = rig.opengl32().create_context(dc);
        rig.host().gl_make("host gpu", "4.6");
        rig.host().gl_policy(0);
        rig.host().gl_window(1u, 1u);
        return rig.opengl32().make_current(dc, context) ? context : nullptr;
    }

    void *robust_on(std::uint32_t thread)
    {
        rig.threads().switch_to(thread);
        void *const dc = thread == 1u ? rig.dc() : rig.other_dc();
        const auto  create = reinterpret_cast<AttribsFn>(rig.opengl32().driver_proc_address("wglCreateContextAttribsARB"));
        rig.host().gl_make("host gpu", "4.6");
        rig.host().gl_policy(0);
        void *const context = create(dc, nullptr, robust_attributes);
        rig.host().gl_window(1u, 1u);
        return context != nullptr && rig.opengl32().make_current(dc, context) ? context : nullptr;
    }

    void fault_current()
    {
        rig.kmt().withhold_answer_at(1u);
        CHECK_EQ(get_error(), 0u);
    }

    GLenum get_error() { return rig.opengl32().gl<GetErrorFn>(slot_get_error)(); }

    GLenum reset_status()
    {
        return reinterpret_cast<ResetStatusFn>(rig.opengl32().driver_proc_address("glGetGraphicsResetStatus"))();
    }

    std::uint32_t clear_calls()
    {
        flush_current();
        const std::uint32_t before = total_calls();
        rig.opengl32().gl<ClearFn>(slot_clear)(color_buffer_bit);
        flush_current();
        return total_calls() - before;
    }
    void flush_current()
    {
        if (rig.root().current() != nullptr) rig.root().current()->flush();
    }

    std::uint32_t total_calls()
    {
        std::uint32_t total = 0u;
        for (unsigned call = 0u; call <= static_cast<unsigned>(MockKmt::Call::query_video_memory); call++)
            total += rig.kmt().calls(static_cast<MockKmt::Call>(call));
        return total + rig.kmt().commands();
    }
};

}

TEST(gl_device_loss, a_fault_loses_every_context_on_the_device)
{ LossRig loss;
  GlIcdRig &rig = loss.rig;
  void *const first = loss.made_on(1u);
  void *const second = loss.made_on(2u);
  CHECK(first != nullptr && second != nullptr);
  rig.threads().switch_to(1u);
  loss.fault_current();
  CHECK(rig.root().invariants_hold());
  CHECK(rig.root().current()->wire() == nullptr);
  CHECK_EQ(loss.clear_calls(), 0u);
  CHECK_EQ(loss.get_error(), gl_out_of_memory);
  CHECK_EQ(loss.get_error(), 0u);
  CHECK_EQ(loss.reset_status(), 0u);
  CHECK_EQ(rig.log().holding("is lost"), 1u);
  rig.threads().switch_to(2u);
  CHECK_EQ(loss.clear_calls(), 0u);
  CHECK(rig.root().current()->wire() == nullptr);
  CHECK_EQ(loss.get_error(), gl_out_of_memory);
  CHECK_EQ(loss.get_error(), 0u);
  CHECK(rig.root().invariants_hold());
  const std::uint32_t commands = rig.kmt().commands();
  CHECK(rig.opengl32().delete_context(second));
  rig.threads().switch_to(1u);
  CHECK(rig.opengl32().delete_context(first));
  CHECK_EQ(rig.kmt().commands(), commands);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(gl_device_loss, a_robust_context_reports_an_unknown_reset)
{ LossRig loss;
  GlIcdRig &rig = loss.rig;
  CHECK(loss.robust_on(1u) != nullptr);
  av::AvContextRecord record;
  CHECK_STATUS(record.decode(rig.kmt().last_private(), rig.kmt().last_private_size()), Status::ok);
  CHECK_EQ(record.options().flags(), static_cast<std::uint8_t>(av::GlContextOptions::robust_access |
                                                                av::GlContextOptions::lose_context_on_reset));
  rig.host().gl_varint(0u);
  CHECK_EQ(loss.reset_status(), 0u);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  loss.fault_current();
  const std::uint32_t commands = rig.kmt().commands();
  CHECK_EQ(loss.reset_status(), gl_unknown_context_reset);
  CHECK_EQ(loss.reset_status(), gl_unknown_context_reset);
  const char *const spellings[] = {"glGetGraphicsResetStatusARB", "glGetGraphicsResetStatusEXT",
                                   "glGetGraphicsResetStatusKHR"};
  for (const char *const name : spellings)
    CHECK_EQ(reinterpret_cast<ResetStatusFn>(rig.opengl32().driver_proc_address(name))(), gl_unknown_context_reset);
  CHECK_EQ(loss.get_error(), gl_context_lost);
  CHECK_EQ(loss.get_error(), 0u);
  CHECK_EQ(rig.kmt().commands(), commands);
  CHECK(rig.root().invariants_hold()); }

TEST(gl_device_loss, half_of_robustness_reports_like_any_context)
{ const std::int32_t access_only[] = {0x2094, 0x4, 0};
  const std::int32_t reset_only[] = {0x8256, 0x8252, 0};
  const std::int32_t *const halves[] = {access_only, reset_only};
  for (const std::int32_t *const attributes : halves) {
    LossRig loss;
    GlIcdRig &rig = loss.rig;
    const auto create = reinterpret_cast<AttribsFn>(rig.opengl32().driver_proc_address("wglCreateContextAttribsARB"));
    rig.host().gl_make("host gpu", "4.6");
    rig.host().gl_policy(0);
    void *const context = create(rig.dc(), nullptr, attributes);
    rig.host().gl_window(1u, 1u);
    CHECK(rig.opengl32().make_current(rig.dc(), context));
    loss.fault_current();
    CHECK_EQ(loss.reset_status(), 0u);
    CHECK_EQ(loss.get_error(), gl_out_of_memory);
  } }

TEST(gl_device_loss, a_new_context_opens_a_new_device)
{ LossRig loss;
  GlIcdRig &rig = loss.rig;
  void *const lost = loss.made_on(1u);
  loss.fault_current();
  CHECK_EQ(loss.clear_calls(), 0u);
  void *const fresh = loss.made_on(2u);
  CHECK(fresh != nullptr);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_device), 2u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::enumerate_adapters), 1u);
  CHECK(rig.root().current()->wire() != nullptr);
  CHECK_EQ(loss.clear_calls(), 2u);
  rig.host().gl_varint(0u);
  CHECK_EQ(loss.get_error(), 0u);
  CHECK(rig.root().invariants_hold());
  rig.threads().switch_to(1u);
  CHECK(rig.opengl32().delete_context(lost));
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::destroy_device), 1u);
  rig.threads().switch_to(2u);
  CHECK_EQ(loss.clear_calls(), 2u);
  CHECK(rig.opengl32().delete_context(fresh));
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(gl_device_loss, a_lost_context_made_current_again_stays_lost)
{ LossRig loss;
  GlIcdRig &rig = loss.rig;
  void *const context = loss.made_on(1u);
  loss.fault_current();
  CHECK(rig.opengl32().make_current(rig.dc(), nullptr));
  CHECK(rig.opengl32().make_current(rig.dc(), context));
  CHECK(rig.root().current()->wire() == nullptr);
  CHECK_EQ(loss.clear_calls(), 0u);
  CHECK(rig.opengl32().delete_context(context));
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(gl_device_loss, the_probe_forces_a_fault_on_the_current_context)
{ LossRig loss;
  GlIcdRig &rig = loss.rig;
  const auto force = reinterpret_cast<ForceFaultFn>(rig.opengl32().driver_proc_address("avForceContextFault"));
  CHECK(force != nullptr);
  rig.threads().switch_to(3u);
  CHECK_EQ(force(), 0);
  CHECK_EQ(rig.threads().last_error(), Root::error_invalid_operation);
  CHECK_EQ(loss.reset_status(), 0u);
  CHECK(loss.made_on(1u) != nullptr);
  loss.flush_current();
  const std::uint32_t commands = rig.kmt().commands();
  rig.kmt().withhold_answer_at(1u);
  CHECK_EQ(force(), 1);
  CHECK_EQ(rig.kmt().commands(), commands + 1u);
  CHECK_EQ(rig.kmt().command_id(commands), av::glproto::kCmd_glGetString);
  CHECK_EQ(rig.kmt().command_size(commands), static_cast<std::uint32_t>(av::CommandHeader::encoded_size));
  CHECK(rig.root().current()->wire() == nullptr);
  CHECK_EQ(force(), 1);
  CHECK_EQ(rig.kmt().commands(), commands + 1u);
  CHECK(rig.root().invariants_hold()); }

TEST(gl_device_loss, a_host_that_answers_the_forced_fault_leaves_the_context_working)
{ LossRig loss;
  GlIcdRig &rig = loss.rig;
  CHECK(loss.made_on(1u) != nullptr);
  const auto force = reinterpret_cast<ForceFaultFn>(rig.opengl32().driver_proc_address("avForceContextFault"));
  rig.host().gl_string("answered");
  CHECK_EQ(force(), 0);
  CHECK_EQ(rig.log().holding("the host answered the forced fault"), 1u);
  CHECK(rig.root().current()->wire() != nullptr);
  CHECK_EQ(loss.clear_calls(), 2u); }
