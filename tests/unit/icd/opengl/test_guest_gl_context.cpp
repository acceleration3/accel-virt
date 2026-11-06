#include "core/trace/state_writer.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "harness/icd/context_link_probe.hpp"
#include "harness/icd/host_channel_probe.hpp"
#include "icd/opengl/gl_external_objects.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "harness/icd/gl_context_rig.hpp"
#include "icd/opengl/granted_profile.hpp"
#include "icd/opengl/guest_gl_context.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/escape/av_context_record.hpp"
#include "proto/generated/opengl/gl_protocol_ids.h"
#include "proto/submission/command_header.hpp"

#include "gl_dispatch_client.h"

#include <cstdio>
#include <cstring>

using av::GlContextOptions;
using av::GlContextRig;
using av::MockKmt;
using av::Status;
using av::glicd::GuestGlContext;
using State = GuestGlContext::State;

namespace av::icd {

struct LossLatchProbe {
    static void set_word(LossLatch &latch, std::uint32_t word) { latch.word_ = word; }
};

struct PresentPacingProbe {
    static void set_blanks(PresentPacing &pacing, std::uint32_t blanks) { pacing.blanks_ = blanks; }
};

}

namespace av::glicd {

struct PixelStagingBufferProbe {
    static void stage_with_no_name(PixelStagingBuffer &staging) { staging.staged_ = PixelStagingBuffer::Staged::download; }
};

struct SlicedBuffersProbe {
    static void set_group(SlicedBuffers &slices, std::uint32_t group) { slices.group_ = group; }
};

struct SliceGroupsProbe {
    static void plant_member(SliceGroups &groups, std::uint64_t key, const SliceGroups::Member &member)
    {
        std::uint32_t slot = 0u;
        static_cast<void>(groups.members_.insert(key, member, slot));
    }
    static std::uint64_t next_member(SliceGroups &groups) { return groups.next_member_; }
};

struct GuestGlContextProbe {
    using LossLatchProbe = av::icd::LossLatchProbe;
    using PresentPacingProbe = av::icd::PresentPacingProbe;
    static void claim_made(GuestGlContext &context) { context.state_ = GuestGlContext::State::made; }
    static av::icd::HostChannel &channel(GuestGlContext &context) { return context.channel_; }
    static void unterminate(GuestGlContext &context) { std::memset(context.strings_[2], 'x', GuestGlContext::string_bytes); }
    static av::icd::DeviceLink *disconnect(GuestGlContext &context)
    {
        av::icd::DeviceLink *&link = av::icd::ContextLinkProbe(context.link_).link();
        av::icd::DeviceLink *const held = link;
        link = nullptr;
        return held;
    }
    static void reconnect(GuestGlContext &context, av::icd::DeviceLink *held)
    {
        av::icd::ContextLinkProbe(context.link_).link() = held;
    }
    static void report_loss(GuestGlContext &context) { context.loss_reported_ = true; }
    static void lose(GuestGlContext &context) { context.loss_.lose(av::FaultCode::device_lost); }
    static void mislose(GuestGlContext &context) { LossLatchProbe::set_word(context.loss_, 1u); }
    static void set_state(GuestGlContext &context, GuestGlContext::State state) { context.state_ = state; }
    static void name_staging(GuestGlContext &context) { PixelStagingBufferProbe::stage_with_no_name(context.staging_); }
    static void crowd_pacing(GuestGlContext &context) { PresentPacingProbe::set_blanks(context.pacing_, 1000u); }
    static void orphan_slices(GuestGlContext &context) { SlicedBuffersProbe::set_group(context.slices_, 0u); }
};

struct ClientArrayStateProbe {
    static void name_default(ClientArrayState &state)
    {
        uint32_t slot = 0u;
        static_cast<void>(state.elements_.insert(0u, 0u, slot));
    }
    static void unname_default(ClientArrayState &state) { static_cast<void>(state.elements_.erase(0u)); }
};

struct UniformShapesProbe {
    static void plant_unshaped(UniformShapes &shapes)
    {
        uint32_t slot = 0u;
        static_cast<void>(shapes.values_.insert(uint64_t{9u} << 32u, 0u, slot));
    }
    static void pull_unshaped(UniformShapes &shapes) { static_cast<void>(shapes.values_.erase(uint64_t{9u} << 32u)); }
};

struct WindowFramebuffersProbe {
    static void point_at(WindowFramebuffers &windows, BackBufferPresenter *presenter) { windows.current_ = presenter; }
};

struct MappedBuffersProbe {
    static void miscount(MappedBuffers &mappings, std::uint32_t writers) { mappings.persistent_writers_ = writers; }
};

struct BufferShadowsProbe {
    static void miscount(BufferShadows &shadows, std::uint64_t bytes) { shadows.held_bytes_ = bytes; }
};

struct QueryShapesProbe {
    static void overflow(QueryShapes &shapes, uint32_t length) { shapes.lengths_[1] = length; }
};

}

namespace {

constexpr const char *renderer = "av host renderer";

class Context {
public:
    explicit Context(GlContextRig &rig, const GlContextOptions &options = GlContextOptions())
        : context_(rig.connection(), rig.platform(), rig.send_lock(), rig.groups(), rig.lines(), rig.extensions(), rig.handles(), 7,
                   options)
    {}
    GuestGlContext &operator*() { return context_; }
    GuestGlContext *operator->() { return &context_; }

private:
    GuestGlContext context_;
};

std::uint32_t warmed(GlContextRig &rig)
{
    {
        Context context(rig);
        rig.host().gl_made(renderer, "4.6");
        CHECK_STATUS(context->make(), Status::ok);
    }
    CHECK_EQ(rig.kmt().open_objects(), 0u);
    return rig.platform().live_allocations();
}

}

TEST(guest_gl_context, nothing_is_on_the_host_until_made)
{ GlContextRig rig;
  Context context(rig);
  CHECK(context->invariants_hold());
  CHECK(context->state() == State::created);
  CHECK(context->wire() == nullptr);
  CHECK_EQ(context->share_name(), 0u);
  CHECK(context->string(GuestGlContext::gl_renderer) == nullptr);
  CHECK_EQ(context->pixel_format(), 7);
  CHECK_EQ(context->owner(), 0u);
  CHECK_EQ(context->guest_device(), 0u);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.connection().users(), 0u); }

TEST(guest_gl_context, make_opens_an_opengl_context_with_its_options_and_learns_the_host)
{ GlContextRig rig;
  const GlContextOptions options(4u, 5u, GlContextOptions::Profile::core, GlContextOptions::debug, 0u);
  Context context(rig, options);
  rig.host().gl_made(renderer, "4.6.0 NVIDIA 580");
  CHECK_STATUS(context->make(), Status::ok);
  CHECK(context->invariants_hold());
  CHECK(context->state() == State::made);
  CHECK(context->wire() != nullptr);
  CHECK(context->share_name() != 0u);
  CHECK_EQ(rig.connection().users(), 1u);
  CHECK(std::strcmp(context->string(GuestGlContext::gl_vendor), "av host vendor") == 0);
  CHECK(std::strcmp(context->string(GuestGlContext::gl_renderer), renderer) == 0);
  CHECK(std::strcmp(context->string(GuestGlContext::gl_version), "4.6.0 NVIDIA 580") == 0);
  CHECK(std::strcmp(context->string(GuestGlContext::gl_shading_language_version), "4.60") == 0);
  CHECK(context->string(0x1F03u) == nullptr);
  CHECK_EQ(context->queries().values(0x86A3u), 3u);
  CHECK_EQ(context->queries().values(0x87FFu), 1u);
  CHECK_EQ(context->queries().values(0x8DF8u), 0u);
  av::AvContextRecord record;
  CHECK_STATUS(record.decode(rig.kmt().last_private(), rig.kmt().last_private_size()), Status::ok);
  CHECK_EQ(record.options().major(), 4u);
  CHECK_EQ(record.options().minor(), 5u);
  CHECK_EQ(record.options().flags(), GlContextOptions::debug);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.log().lines() == 0u);
  CHECK_STATUS(context->make(), Status::invalid_state);
  CHECK(context->state() == State::made); }

TEST(guest_gl_context, a_make_learns_the_profile_the_host_granted)
{ GlContextRig rig;
  Context context(rig);
  context->asking(av::glicd::GrantedProfile::Rung::compatibility_46);
  rig.host().gl_make_profile(renderer, "4.6.0 NVIDIA 580", 4, 6, 2);
  rig.host().gl_slices();
  CHECK_STATUS(context->make(), Status::ok);
  CHECK(context->invariants_hold());
  CHECK(context->profile().rung() == av::glicd::GrantedProfile::Rung::compatibility_46);
  CHECK_EQ(context->profile().major(), 4u);
  CHECK_EQ(context->profile().minor(), 6u);
  CHECK_EQ(context->profile().mask(), 2u);
  CHECK(context->profile().legacy_calls_live()); }

TEST(guest_gl_context, a_core_or_silent_host_leaves_the_legacy_calls_shut)
{ GlContextRig core;
  Context core_context(core);
  core_context->asking(av::glicd::GrantedProfile::Rung::core_46);
  core.host().gl_make_profile(renderer, "4.6 (Core Profile) Mesa", 4, 6, 1);
  core.host().gl_slices();
  CHECK_STATUS(core_context->make(), Status::ok);
  CHECK_EQ(core_context->profile().mask(), 1u);
  CHECK(!core_context->profile().legacy_calls_live());

  GlContextRig silent;
  Context silent_context(silent);
  silent.host().gl_make_profile(renderer, "1.1", -1, -1, 0);
  silent.host().gl_slices();
  CHECK_STATUS(silent_context->make(), Status::ok);
  CHECK(silent_context->invariants_hold());
  CHECK(!silent_context->profile().known());
  CHECK(!silent_context->profile().legacy_calls_live()); }

TEST(guest_gl_context, a_legacy_context_asks_for_compatibility_first)
{ GlContextRig rig;
  Context context(rig);
  rig.host().gl_make_profile(renderer, "4.6 (Compatibility Profile) Mesa", 4, 6, 2);
  rig.host().gl_slices();
  CHECK_STATUS(context->make(), Status::ok);
  CHECK(context->profile().rung() == av::glicd::GrantedProfile::Rung::compatibility_46);
  av::AvContextRecord record;
  CHECK_STATUS(record.decode(rig.kmt().last_private(), rig.kmt().last_private_size()), Status::ok);
  CHECK_EQ(record.options().major(), 4u);
  CHECK_EQ(record.options().minor(), 6u);
  CHECK(record.options().profile() == GlContextOptions::Profile::compatibility);
  CHECK(context->profile().legacy_calls_live()); }

TEST(guest_gl_context, a_host_with_no_compatibility_profile_gets_the_next_rung)
{ GlContextRig rig;
  Context context(rig);
  rig.kmt().fail_at(MockKmt::Call::create_context_virtual, 1u, MockKmt::invalid_parameter);
  rig.host().gl_make_profile(renderer, "1.4 Mesa", 1, 4, 0);
  rig.host().gl_slices();
  CHECK_STATUS(context->make(), Status::ok);
  CHECK(context->profile().rung() == av::glicd::GrantedProfile::Rung::host_default);
  av::AvContextRecord record;
  CHECK_STATUS(record.decode(rig.kmt().last_private(), rig.kmt().last_private_size()), Status::ok);
  CHECK_EQ(record.options().major(), 0u);
  CHECK(record.options().profile() == GlContextOptions::Profile::none);
  CHECK(context->profile().legacy_calls_live()); }

TEST(guest_gl_context, a_host_with_neither_falls_to_core_and_says_so)
{ GlContextRig rig;
  Context context(rig);
  rig.kmt().fail_first(MockKmt::Call::create_context_virtual, 2u, MockKmt::invalid_parameter);
  rig.host().gl_make_profile(renderer, "4.6 (Core Profile) Mesa", 4, 6, 1);
  rig.host().gl_slices();
  CHECK_STATUS(context->make(), Status::ok);
  CHECK(context->profile().rung() == av::glicd::GrantedProfile::Rung::core_46);
  CHECK(!context->profile().legacy_calls_live());
  CHECK(context->invariants_hold()); }

TEST(guest_gl_context, a_host_that_refuses_every_rung_holds_nothing)
{ GlContextRig rig;
  Context context(rig);
  rig.kmt().fail_first(MockKmt::Call::create_context_virtual, 3u, MockKmt::invalid_parameter);
  CHECK_STATUS(context->make(), Status::invalid_parameter);
  CHECK(context->state() == State::created);
  CHECK(context->invariants_hold());
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.platform().live_allocations(), 0u); }

TEST(guest_gl_context, a_refusal_that_is_not_the_profiles_stops_the_ladder)
{ GlContextRig rig;
  Context context(rig);
  av::icd::DeviceLink *link = nullptr;
  CHECK_STATUS(rig.connection().acquire(link), Status::ok);
  rig.platform().fail_allocation_at(1u);
  CHECK_STATUS(context->make(), Status::no_memory);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_context_virtual), 0u);
  CHECK(context->state() == State::created);
  rig.connection().release(*link); }

TEST(guest_gl_context, a_link_that_cannot_be_had_is_not_supported_with_nothing_held)
{ GlContextRig rig;
  Context context(rig);
  rig.platform().fail_allocation_at(1u);
  CHECK_STATUS(context->make(), Status::not_supported);
  CHECK(context->state() == State::created);
  CHECK(context->invariants_hold());
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.connection().users(), 0u);
  CHECK_EQ(rig.platform().live_allocations(), 0u); }

TEST(guest_gl_context, a_context_created_with_attributes_asks_once)
{ GlContextRig rig;
  const GlContextOptions options(4u, 5u, GlContextOptions::Profile::core, 0u, 0u);
  Context context(rig, options);
  rig.kmt().fail_at(MockKmt::Call::create_context_virtual, 1u, MockKmt::invalid_parameter);
  CHECK_STATUS(context->make(), Status::invalid_parameter);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_context_virtual), 1u);
  CHECK(context->profile().rung() == av::glicd::GrantedProfile::Rung::none); }

TEST(guest_gl_context, it_remembers_the_dc_it_was_made_current_on)
{ GlContextRig rig;
  Context context(rig);
  int dc = 0;
  CHECK(context->current_dc() == nullptr);
  context->set_current_dc(&dc);
  CHECK(context->current_dc() == &dc);
  context->set_current_dc(nullptr);
  CHECK(context->current_dc() == nullptr);
  CHECK(context->invariants_hold()); }

TEST(guest_gl_context, an_unmade_context_paces_nothing)
{ GlContextRig rig;
  Context context(rig);
  const std::uint32_t blanks = rig.kmt().calls(MockKmt::Call::wait_for_vertical_blank);
  context->pacing().after_present();
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_vertical_blank), blanks); }

TEST(guest_gl_context, a_destroyed_context_gives_back_everything)
{ GlContextRig rig;
  const std::uint32_t baseline = warmed(rig);
  {
    Context context(rig);
    rig.host().gl_made(renderer, "4.6");
    CHECK_STATUS(context->make(), Status::ok);
    CHECK(rig.kmt().open_objects() > GlContextRig::link_objects);
  }
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.connection().users(), 0u);
  CHECK_EQ(rig.platform().live_allocations(), baseline);
  CHECK_EQ(rig.kmt().window_chunks(), 0u); }

TEST(guest_gl_context, no_avgpu_device_is_not_supported)
{ GlContextRig rig;
  rig.kmt().fail_at(MockKmt::Call::create_device, 1u, MockKmt::invalid_parameter);
  Context context(rig);
  CHECK_STATUS(context->make(), Status::not_supported);
  CHECK(context->state() == State::created);
  CHECK(context->invariants_hold());
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(guest_gl_context, staging_that_cannot_be_had_is_no_memory_with_nothing_held)
{ GlContextRig rig;
  Context context(rig);
  const std::uint32_t baseline = warmed(rig);
  av::icd::DeviceLink *link = nullptr;
  CHECK_STATUS(rig.connection().acquire(link), Status::ok);
  const std::uint32_t objects = rig.kmt().open_objects();
  rig.platform().fail_allocation_at(1u);
  CHECK_STATUS(context->make(), Status::no_memory);
  CHECK(context->invariants_hold());
  CHECK_EQ(rig.kmt().open_objects(), objects);
  CHECK_EQ(rig.connection().users(), 1u);
  rig.connection().release(*link);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.connection().users(), 0u);
  CHECK_EQ(rig.platform().live_allocations(), baseline); }

TEST(guest_gl_context, a_driver_refusal_of_the_transport_is_invalid_parameter_with_nothing_held)
{
  struct Step { MockKmt::Call call; std::uint32_t nth; std::uint32_t count; };
  const Step steps[] = {
      {MockKmt::Call::create_allocation, 1u, 1u}, {MockKmt::Call::create_allocation, 2u, 1u},
      {MockKmt::Call::create_context_virtual, 1u, 1u}, {MockKmt::Call::escape, 1u, 2u},
  };
  for (const Step &step : steps) {
      GlContextRig rig;
      Context context(rig);
      if (step.count == 1u) rig.kmt().fail_at(step.call, step.nth, MockKmt::invalid_parameter);
      else rig.kmt().fail_first(step.call, step.count, MockKmt::invalid_parameter);
      CHECK_STATUS(context->make(), Status::invalid_parameter);
      CHECK(context->state() == State::created);
      CHECK(context->invariants_hold());
      CHECK_EQ(rig.kmt().open_objects(), 0u);
      CHECK_EQ(rig.platform().live_allocations(), 0u);
      CHECK_EQ(rig.log().holding("the driver refused the context"), 1u);
  }
  GlContextRig rig;
  Context impossible(rig, GlContextOptions(4u, 7u, GlContextOptions::Profile::core, 0u, 0u));
  CHECK_STATUS(impossible->make(), Status::invalid_parameter);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_allocation), 0u); }

TEST(guest_gl_context, a_transport_out_of_memory_is_no_memory)
{ GlContextRig rig;
  Context context(rig);
  rig.kmt().fail_at(MockKmt::Call::create_allocation, 1u, 0xC0000017u);
  CHECK_STATUS(context->make(), Status::no_memory);
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(guest_gl_context, without_a_blob_arena_the_context_is_made_and_says_so)
{ GlContextRig rig;
  Context context(rig);
  rig.host().blob_import(1u, 0u);
  rig.host().blob_import(1u, 0u);
  rig.host().gl_string("vendor");
  rig.host().gl_string(renderer);
  rig.host().gl_string("4.6");
  rig.host().gl_string("4.60");
  rig.host().gl_integer(0);
  rig.host().gl_integer(0);
  rig.host().gl_integer(0);
  rig.host().gl_integer(4);
  rig.host().gl_integer(6);
  rig.host().gl_integer(2);
  rig.host().gl_integer(0);
  CHECK_STATUS(context->make(), Status::ok);
  CHECK_EQ(rig.log().holding("no blob arena"), 1u);
  char bytes[512];
  av::StateWriter writer(bytes, sizeof bytes);
  context->report(writer, 9u);
  CHECK(std::strstr(writer.dump(), " arena=false ") != nullptr);
  std::uint32_t error = 7u;
  CHECK(!context->take_host_error(error));
  CHECK_EQ(error, 0u);
  CHECK(context->invariants_hold()); }

TEST(guest_gl_context, a_host_refusal_is_invalid_parameter_with_nothing_held)
{ for (std::uint32_t wait = 2u; wait <= GlContextRig::link_objects + 5u; wait++) {
      GlContextRig rig;
      const std::uint32_t baseline = warmed(rig);
      Context context(rig);
      rig.host().gl_made(renderer, "4.6");
      rig.kmt().withhold_answer_at(wait);
      CHECK_STATUS(context->make(), Status::invalid_parameter);
      CHECK(context->state() == State::created);
      CHECK_EQ(rig.log().holding("the host refused the context") +
                   rig.log().holding("faulted opening its blob arena"),
               3u);
      CHECK_EQ(rig.kmt().open_objects(), 0u);
      CHECK_EQ(rig.platform().live_allocations(), baseline);
  } }

TEST(guest_gl_context, a_faulted_transport_makes_the_context_lost)
{ GlContextRig rig;
  Context context(rig);
  rig.host().gl_made(renderer, "4.6");
  CHECK_STATUS(context->make(), Status::ok);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  rig.kmt().withhold_answer_at(1u);
  av::ClientTransport *const wire = context->wire();
  CHECK(wire != nullptr);
  CHECK_EQ(av::glproto::av_glGetError(*wire), 0u);
  CHECK(context->wire() == nullptr);
  CHECK(context->state() == State::lost);
  CHECK(context->wire() == nullptr);
  CHECK_EQ(rig.log().holding("is lost"), 1u);
  CHECK(std::strcmp(context->string(GuestGlContext::gl_renderer), renderer) == 0);
  CHECK(context->share_name() != 0u);
  CHECK(context->invariants_hold()); }

TEST(guest_gl_context, a_lost_context_reports_its_loss_by_its_robustness)
{ const std::uint8_t robust_flags = GlContextOptions::robust_access | GlContextOptions::lose_context_on_reset;
  const std::uint8_t flag_sets[] = {robust_flags, GlContextOptions::robust_access, 0u};
  for (const std::uint8_t flags : flag_sets) {
    const bool robust = flags == robust_flags;
    GlContextRig rig;
    Context context(rig, GlContextOptions(4u, 6u, GlContextOptions::Profile::core, flags, 0u));
    CHECK_EQ(context->robust(), robust);
    CHECK_EQ(context->lost_reset_status(), 0u);
    rig.host().gl_made(renderer, "4.6");
    CHECK_STATUS(context->make(), Status::ok);
    CHECK_EQ(context->take_error(), 0u);
    CHECK_EQ(context->lost_reset_status(), 0u);
    rig.kmt().withhold_answer_at(1u);
    CHECK_EQ(av::glproto::av_glGetError(*context->wire()), 0u);
    CHECK(context->wire() == nullptr);
    CHECK_EQ(context->lost_reset_status(), robust ? GuestGlContext::gl_unknown_context_reset : 0u);
    CHECK_EQ(context->lost_reset_status(), robust ? GuestGlContext::gl_unknown_context_reset : 0u);
    context->record(0x0502u);
    CHECK_EQ(context->take_error(), 0x0502u);
    CHECK(context->invariants_hold());
    CHECK_EQ(context->take_error(),
             robust ? GuestGlContext::gl_context_lost : GuestGlContext::gl_out_of_memory);
    CHECK(context->invariants_hold());
    CHECK_EQ(context->take_error(), 0u);
  } }

TEST(guest_gl_context, a_fault_on_its_device_loses_it_and_it_goes_without_the_host)
{ GlContextRig rig;
  std::uint32_t commands = 0u;
  {
    Context first(rig);
    Context second(rig);
    rig.host().gl_made(renderer, "4.6");
    CHECK_STATUS(first->make(), Status::ok);
    rig.host().gl_made(renderer, "4.6");
    CHECK_STATUS(second->make(), Status::ok);
    rig.kmt().withhold_answer_at(1u);
    CHECK_EQ(av::glproto::av_glGetError(*first->wire()), 0u);
    CHECK(first->wire() == nullptr);
    CHECK(second->wire() == nullptr);
    CHECK(second->state() == State::lost);
    commands = rig.kmt().commands();
  }
  CHECK_EQ(rig.kmt().commands(), commands);
  CHECK_EQ(rig.log().holding("did not close whole"), 0u);
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(guest_gl_context, the_first_error_recorded_is_kept_until_taken)
{ GlContextRig rig;
  Context context(rig);
  CHECK_EQ(context->take_error(), 0u);
  context->record(0x0502u);
  context->record(0x0500u);
  CHECK_EQ(context->take_error(), 0x0502u);
  CHECK_EQ(context->take_error(), 0u);
  context->record(0x0500u);
  CHECK_EQ(context->take_error(), 0x0500u);
  context->set_owner(3u);
  CHECK_EQ(context->owner(), 3u);
  context->set_debug_callback(0x1000u, 0x2000u);
  CHECK_EQ(context->debug_callback(), 0x1000u);
  CHECK_EQ(context->debug_user(), 0x2000u);
  CHECK_EQ(context->arrays().attribute_pointer(0u), 0u);
  CHECK_EQ(context->uniforms().held(), 0u);
  CHECK_EQ(context->options().major(), 0u); }

TEST(guest_gl_context, a_host_newer_than_4_6_is_reported_as_4_6)
{ struct Row { const char *host; const char *reported; };
  const Row rows[] = {
      {"4.7.1 Mesa 26.0", "4.6.1 Mesa 26.0"}, {"5.0 (Core Profile)", "4.6 (Core Profile)"}, {"4.10", "4.6"},
      {"4.6.0 NVIDIA", "4.6.0 NVIDIA"},       {"3.3", "3.3"},                               {"4.", "4."},
      {"x4.7", "x4.7"},                        {"47", "47"},                                 {"4.a", "4.a"},
  };
  for (const Row &row : rows) {
      GlContextRig rig;
      Context context(rig);
      rig.host().gl_made(renderer, row.host);
      CHECK_STATUS(context->make(), Status::ok);
      CHECK(std::strcmp(context->string(GuestGlContext::gl_version), row.reported) == 0);
  } }

TEST(guest_gl_context, a_string_longer_than_its_row_is_cut)
{ GlContextRig rig;
  Context context(rig);
  char long_name[400];
  std::memset(long_name, 'n', sizeof long_name - 1u);
  long_name[sizeof long_name - 1u] = '\0';
  rig.host().gl_made(long_name, "4.6");
  CHECK_STATUS(context->make(), Status::ok);
  CHECK_EQ(std::strlen(context->string(GuestGlContext::gl_renderer)), GuestGlContext::string_bytes - 2u);
  CHECK(context->invariants_hold()); }

TEST(guest_gl_context, reports_its_state_options_and_renderer)
{ GlContextRig rig;
  Context context(rig, GlContextOptions(3u, 3u, GlContextOptions::Profile::compatibility, 0u, 0u));
  char bytes[512];
  av::StateWriter created(bytes, sizeof bytes);
  context->report(created, 0x10001u);
  CHECK(std::strcmp(created.dump(), "context/65537 state=created owner=0 pixel_format=7 major=3 minor=3 "
                                    "profile=2 flags=0 share=0 share_target=0 error=0 device=0 lost=false fault_code=0 "
                                    "arena=false renderer= extensions=0\n"
                                    "context/65537/windows/0 held=0 draws_window=false reads_window=false\n"
                                    "context/65537/mappings/0 held=0 persistent_writers=0\n"
                                    "context/65537/sliced_buffers/0 granted=false stopped=false group=1 owned=0 held=0\n"
                                    "context/65537/shadows/0 trusted=true held=0 held_bytes=0 sizes=0") == 0);
  rig.host().gl_made(renderer, "4.6");
  CHECK_STATUS(context->make(), Status::ok);
  context->set_owner(2u);
  context->record(0x0502u);
  av::StateWriter made(bytes, sizeof bytes);
  context->report(made, 1u);
  char expected[512];
  std::snprintf(expected, sizeof expected,
                "context/1 state=made owner=2 pixel_format=7 major=3 minor=3 profile=2 flags=0 "
                "share=0 share_target=0 error=1282 device=%u lost=false fault_code=0 arena=true "
                "renderer=av\\x20host\\x20renderer extensions=0\n"
                "context/1/windows/0 held=0 draws_window=false reads_window=false\n"
                "context/1/mappings/0 held=0 persistent_writers=0\n"
                "context/1/sliced_buffers/0 granted=false stopped=false group=1 owned=0 held=0\n"
                "context/1/shadows/0 trusted=true held=0 held_bytes=0 sizes=0",
                context->guest_device());
  CHECK(context->guest_device() != 0u);
  CHECK(std::strcmp(made.dump(), expected) == 0); }

TEST(guest_gl_context_invariants, fail_when_the_state_and_what_is_held_disagree)
{ GlContextRig rig;
  Context claimed(rig);
  av::glicd::GuestGlContextProbe::claim_made(*claimed);
  CHECK(!claimed->invariants_hold());
  Context staged(rig);
  std::uint8_t block = 0u;
  av::icd::HostChannelProbe staging(av::glicd::GuestGlContextProbe::channel(*staged));
  static_cast<void>(staging.swap_staging(&block));
  CHECK(!staged->invariants_hold());
  static_cast<void>(staging.swap_staging(nullptr));
  Context unterminated(rig);
  av::glicd::GuestGlContextProbe::unterminate(*unterminated);
  CHECK(!unterminated->invariants_hold());
  Context connected(rig);
  av::icd::DeviceLink *const idle = av::glicd::GuestGlContextProbe::disconnect(*connected);
  CHECK(idle == nullptr);
  char marker = 0;
  av::glicd::GuestGlContextProbe::reconnect(*connected, reinterpret_cast<av::icd::DeviceLink *>(&marker));
  CHECK(!connected->invariants_hold());
  av::glicd::GuestGlContextProbe::reconnect(*connected, idle);
  CHECK(connected->invariants_hold());
  Context reported(rig);
  av::glicd::GuestGlContextProbe::report_loss(*reported);
  CHECK(!reported->invariants_hold()); }

TEST(guest_gl_context_invariants, fail_when_a_made_context_or_its_client_state_is_broken)
{ using av::glicd::GuestGlContextProbe;
  GlContextRig rig;
  Context made(rig);
  rig.host().gl_made(renderer, "4.6");
  CHECK_STATUS(made->make(), Status::ok);
  CHECK(made->invariants_hold());
  av::icd::HostChannelProbe channel(GuestGlContextProbe::channel(*made));
  channel.drop_locked();
  CHECK(!made->invariants_hold());
  channel.restore_locked();
  CHECK(made->invariants_hold());
  av::icd::DeviceLink *const link = GuestGlContextProbe::disconnect(*made);
  CHECK(!made->invariants_hold());
  GuestGlContextProbe::reconnect(*made, link);
  av::glicd::ClientArrayStateProbe::name_default(made->arrays());
  CHECK(!made->invariants_hold());
  av::glicd::ClientArrayStateProbe::unname_default(made->arrays());
  av::glicd::UniformShapesProbe::plant_unshaped(made->uniforms());
  CHECK(!made->invariants_hold());
  av::glicd::UniformShapesProbe::pull_unshaped(made->uniforms());
  av::glicd::QueryShapesProbe::overflow(made->queries(), 0x80000000u);
  CHECK(!made->invariants_hold());
  av::glicd::QueryShapesProbe::overflow(made->queries(), 0u);
  CHECK(made->invariants_hold());
  av::glicd::BackBufferPresenter stray(rig.platform(), rig.lines());
  av::glicd::WindowFramebuffersProbe::point_at(made->framebuffers(), &stray);
  CHECK(!made->invariants_hold());
  av::glicd::WindowFramebuffersProbe::point_at(made->framebuffers(), nullptr);
  av::glicd::MappedBuffersProbe::miscount(made->mappings(), 1u);
  CHECK(!made->invariants_hold());
  av::glicd::MappedBuffersProbe::miscount(made->mappings(), 0u);
  av::glicd::BufferShadowsProbe::miscount(made->shadows(), 1u);
  CHECK(!made->invariants_hold());
  av::glicd::BufferShadowsProbe::miscount(made->shadows(), 0u);
  CHECK(made->invariants_hold()); }

TEST(guest_gl_context, sharing_either_way_ends_its_buffer_copies)
{ GlContextRig rig;
  Context      sharer(rig);
  CHECK(sharer->shadows().trusted());
  CHECK_STATUS(sharer->share(5u), Status::ok);
  CHECK(!sharer->shadows().trusted());
  Context lender(rig);
  lender->lend();
  CHECK(!lender->shadows().trusted());
  Context alone(rig);
  CHECK(alone->shadows().trusted()); }

TEST(guest_gl_context, a_make_that_fails_keeps_no_copies_of_buffers)
{ GlContextRig rig;
  const std::uint32_t baseline = warmed(rig);
  Context      context(rig);
  std::uint8_t bytes[4] = {};
  context->shadows().keep(1u, 0u, sizeof bytes, bytes);
  CHECK_EQ(context->shadows().held(), 1u);
  av::icd::DeviceLink *link = nullptr;
  CHECK_STATUS(rig.connection().acquire(link), Status::ok);
  rig.platform().fail_allocation_at(1u);
  CHECK_STATUS(context->make(), Status::no_memory);
  CHECK_EQ(context->shadows().held(), 0u);
  rig.connection().release(*link);
  CHECK_EQ(rig.platform().live_allocations(), baseline); }

TEST(guest_gl_context, marks_its_newest_batch_for_another_thread)
{ GlContextRig rig;
  Context      context(rig);
  CHECK(!context->mark().waitable());
  rig.host().gl_made(renderer, "4.6");
  CHECK_STATUS(context->make(), Status::ok);
  av::glproto::av_glFlush(*context->wire());
  context->flush();
  const av::BatchMark mark = context->mark();
  CHECK(mark.waitable());
  CHECK(mark.fence() != 0u); }

TEST(guest_gl_context, run_held_has_the_host_run_the_held_calls_before_it_returns)
{ GlContextRig rig;
  Context      context(rig);
  CHECK_STATUS(context->run_held(), Status::invalid_state);
  rig.host().gl_made(renderer, "4.6");
  CHECK_STATUS(context->make(), Status::ok);
  const std::uint32_t before = rig.kmt().commands();
  av::glproto::av_glBindTexture(*context->wire(), 0x0DE1u, 186u);
  CHECK_EQ(rig.kmt().commands(), before);
  rig.host().nop();
  CHECK_STATUS(context->run_held(), Status::ok);
  CHECK_EQ(rig.kmt().commands(), before + 2u);
  CHECK_EQ(rig.kmt().command_id(before), av::glproto::kCmd_glBindTexture);
  CHECK_EQ(rig.kmt().command_id(before + 1u), av::CommandHeader::nop_id);
  CHECK_EQ(rig.kmt().queued_answers(), 0u); }

TEST(guest_gl_context, a_held_calls_lease_stays_lent_until_the_batch_carrying_it_has_run)
{ GlContextRig rig;
  Context      context(rig);
  rig.host().gl_made(renderer, "4.6");
  CHECK_STATUS(context->make(), Status::ok);
  av::ClientBlobArena &arena = av::glicd::GuestGlContextProbe::channel(*context).transport().blobs();
  CHECK(arena.ready());
  constexpr std::uint32_t named = 2048u;
  static std::uint32_t    buffers[named];
  for (std::uint32_t at = 0u; at < named; at++) buffers[at] = at + 1u;
  const std::uint32_t before = rig.kmt().commands();
  av::glproto::av_glBindBuffersBase(*context->wire(), 0x8A11u, 0u, static_cast<std::int32_t>(named), buffers);
  CHECK_EQ(rig.kmt().commands(), before);
  CHECK_EQ(arena.used(), sizeof buffers);
  rig.host().nop();
  CHECK_STATUS(context->run_held(), Status::ok);
  CHECK_EQ(rig.kmt().command_id(before), av::glproto::kCmd_glBindBuffersBase);
  CHECK_EQ(rig.kmt().command_id(before + 1u), av::CommandHeader::nop_id);
  CHECK_EQ(arena.used(), 0u); }

namespace {
std::uint64_t end_a_frame(GuestGlContext &context)
{
    av::glproto::av_glFlush(*context.wire());
    context.end_frame();
    return av::glicd::GuestGlContextProbe::channel(context).transport().published_fence();
}

}

namespace {
constexpr std::uint64_t frame_apart = av::MockKmt::ticks_per_second / 1000u * 60u;

}

TEST(guest_gl_context, a_frame_ends_without_waiting_on_a_host_that_keeps_up)
{ GlContextRig rig;
  Context      context(rig);
  rig.host().gl_made(renderer, "4.6");
  CHECK_STATUS(context->make(), Status::ok);
  const std::uint32_t ring = av::glicd::GuestGlContextProbe::channel(*context).transport().context();
  const std::uint32_t waits = rig.kmt().calls(av::MockKmt::Call::wait_for_context);
  rig.kmt().reads_clock(nullptr, frame_apart);
  for (int frame = 0; frame < 4; frame++) {
      const std::uint64_t ended = end_a_frame(*context);
      rig.kmt().complete_on_page(ring, ended);
  }
  rig.kmt().reads_clock(nullptr, 0u);
  CHECK(context->state() == State::made);
  CHECK(context->wire() != nullptr);
  CHECK_EQ(rig.kmt().calls(av::MockKmt::Call::wait_for_context), waits); }

TEST(guest_gl_context, a_frame_the_host_never_finishes_loses_the_context)
{ GlContextRig rig;
  Context      context(rig);
  rig.host().gl_made(renderer, "4.6");
  CHECK_STATUS(context->make(), Status::ok);
  rig.kmt().reads_clock(nullptr, frame_apart);
  end_a_frame(*context);
  CHECK(context->wire() != nullptr);
  end_a_frame(*context);
  rig.kmt().reads_clock(nullptr, 0u);
  CHECK(context->wire() == nullptr);
  CHECK(context->state() == State::lost);
  CHECK_EQ(rig.log().holding("never finished an earlier frame"), 1u);
  context->end_frame(); }

TEST(guest_gl_context, externals_is_the_contexts_own_and_starts_holding_nothing)
{ GlContextRig rig;
  Context      context(rig);
  CHECK_EQ(context->externals().live(), 0u);
  CHECK(context->externals().invariants_hold());
  GLuint name = 0u;
  CHECK_EQ(context->externals().create(av::SharedObjectKind::memory, 1, &name),
           av::glicd::GlExternalObjects::gl_no_error);
  CHECK(name != 0u);
  CHECK_EQ(context->externals().live(), 1u);
  Context other(rig);
  CHECK_EQ(other->externals().live(), 0u);
  CHECK_EQ(other->externals().made(av::SharedObjectKind::memory, name), 0u); }

TEST(guest_gl_context, a_lost_context_cannot_be_made_to_share_after_another)
{ GlContextRig rig;
  Context context(rig);
  rig.host().gl_made(renderer, "4.6");
  CHECK_STATUS(context->make(), Status::ok);
  rig.kmt().withhold_answer_at(1u);
  CHECK_EQ(av::glproto::av_glGetError(*context->wire()), 0u);
  CHECK(context->wire() == nullptr);
  CHECK(context->state() == State::lost);
  CHECK_STATUS(context->share_after(3u), Status::invalid_state);
  CHECK_EQ(context->share_target(), 0u);
  CHECK(context->invariants_hold()); }

TEST(guest_gl_context, held_calls_that_cannot_run_are_logged_and_invalid_state)
{ GlContextRig rig;
  Context context(rig);
  rig.host().gl_made(renderer, "4.6");
  CHECK_STATUS(context->make(), Status::ok);
  rig.kmt().withhold_answer_at(1u);
  CHECK_EQ(av::glproto::av_glGetError(*context->wire()), 0u);
  CHECK_STATUS(context->run_held(), Status::invalid_state);
  CHECK_EQ(rig.log().holding("the held calls could not be run for another API"), 1u); }

TEST(guest_gl_context, a_context_not_made_sends_no_forgets)
{ GlContextRig rig;
  Context context(rig);
  context->send_forgets();
  CHECK_EQ(rig.kmt().commands(), 0u);
  CHECK(context->invariants_hold()); }

TEST(guest_gl_context, only_a_made_context_with_a_working_link_is_shareable)
{ using av::glicd::GuestGlContextProbe;
  GlContextRig rig;
  Context context(rig);
  CHECK(!context->shareable());
  rig.host().gl_made(renderer, "4.6");
  CHECK_STATUS(context->make(), Status::ok);
  CHECK(context->shareable());
  GuestGlContextProbe::lose(*context);
  CHECK(!context->shareable());
  Context faulted(rig);
  rig.host().gl_made(renderer, "4.6");
  CHECK_STATUS(faulted->make(), Status::ok);
  rig.kmt().withhold_answer_at(1u);
  CHECK_EQ(av::glproto::av_glGetError(*faulted->wire()), 0u);
  CHECK(!faulted->shareable()); }

namespace {

class FinishesAtRead final : public av::ClockReader {
public:
    void arm(MockKmt &kmt, std::uint32_t ring, std::uint64_t fence, std::uint32_t read)
    {
        kmt_ = &kmt;
        ring_ = ring;
        fence_ = fence;
        countdown_ = read;
    }
    void clock_read() override
    {
        if (countdown_ != 0u && --countdown_ == 0u) kmt_->complete_on_page(ring_, fence_);
    }

private:
    MockKmt      *kmt_ = nullptr;
    std::uint32_t ring_ = 0u;
    std::uint64_t fence_ = 0u;
    std::uint32_t countdown_ = 0u;
};

}

TEST(guest_gl_context, a_frame_the_host_finishes_while_waited_for_keeps_the_context)
{ GlContextRig rig;
  Context      context(rig);
  rig.host().gl_made(renderer, "4.6");
  CHECK_STATUS(context->make(), Status::ok);
  const std::uint32_t ring = av::glicd::GuestGlContextProbe::channel(*context).transport().context();
  FinishesAtRead      reader;
  rig.kmt().reads_clock(&reader, frame_apart);
  const std::uint64_t ended = end_a_frame(*context);
  reader.arm(rig.kmt(), ring, ended, 3u);
  end_a_frame(*context);
  rig.kmt().reads_clock(nullptr, 0u);
  CHECK(context->wire() != nullptr);
  CHECK(context->state() == State::made);
  CHECK_EQ(rig.log().holding("never finished an earlier frame"), 0u); }

TEST(guest_gl_context, a_slice_member_that_cannot_be_enrolled_is_no_memory_with_nothing_held)
{ GlContextRig rig;
  Context context(rig);
  rig.host().gl_made(renderer, "4.6");
  av::glicd::SliceGroupsProbe::plant_member(rig.groups(), av::glicd::SliceGroupsProbe::next_member(rig.groups()),
                                            av::glicd::SliceGroups::Member{nullptr, 99u, 0u});
  CHECK_STATUS(context->make(), Status::no_memory);
  CHECK(context->state() == State::created);
  CHECK(context->invariants_hold());
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(guest_gl_context, the_adapters_luid_is_kept_and_zero_when_the_adapter_will_not_say)
{ GlContextRig rig;
  rig.kmt().set_adapter_luid(0u, 0x0000000200001234ull);
  Context named(rig);
  rig.host().gl_made(renderer, "4.6");
  CHECK_STATUS(named->make(), Status::ok);
  CHECK_EQ(named->adapter_luid(), 0x0000000200001234ull);
  Context silent(rig);
  rig.host().gl_made(renderer, "4.6");
  rig.kmt().fail_at(MockKmt::Call::adapter_luid, 1u, MockKmt::invalid_parameter);
  CHECK_STATUS(silent->make(), Status::ok);
  CHECK_EQ(silent->adapter_luid(), 0u); }

TEST(guest_gl_context_invariants, fail_when_the_loss_state_staging_pacing_or_slices_are_broken)
{ using av::glicd::GuestGlContextProbe;
  GlContextRig rig;
  Context lost(rig);
  rig.host().gl_made(renderer, "4.6");
  CHECK_STATUS(lost->make(), Status::ok);
  GuestGlContextProbe::lose(*lost);
  CHECK(!lost->invariants_hold());
  Context mislost(rig);
  rig.host().gl_made(renderer, "4.6");
  CHECK_STATUS(mislost->make(), Status::ok);
  GuestGlContextProbe::mislose(*mislost);
  GuestGlContextProbe::set_state(*mislost, State::lost);
  CHECK(!mislost->invariants_hold());
  Context staged(rig);
  GuestGlContextProbe::name_staging(*staged);
  CHECK(!staged->invariants_hold());
  Context made(rig);
  rig.host().gl_made(renderer, "4.6");
  CHECK_STATUS(made->make(), Status::ok);
  GuestGlContextProbe::name_staging(*made);
  CHECK(!made->invariants_hold());
  Context paced(rig);
  rig.host().gl_made(renderer, "4.6");
  CHECK_STATUS(paced->make(), Status::ok);
  GuestGlContextProbe::crowd_pacing(*paced);
  CHECK(!paced->invariants_hold());
  Context sliced(rig);
  rig.host().gl_made(renderer, "4.6");
  CHECK_STATUS(sliced->make(), Status::ok);
  GuestGlContextProbe::orphan_slices(*sliced);
  CHECK(!sliced->invariants_hold()); }
