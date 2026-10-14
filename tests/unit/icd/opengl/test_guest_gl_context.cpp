/* One application context and its host context: nothing on the host until
 * made, made whole or not at all, lost when its transport faults, and the
 * guest state and host strings it answers from. */
#include "core/trace/state_writer.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/context_link_probe.hpp"
#include "harness/icd/host_channel_probe.hpp"
#include "icd/opengl/gl_external_objects.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "harness/icd/gl_context_rig.hpp"
#include "icd/opengl/granted_profile.hpp"
#include "icd/opengl/guest_gl_context.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/escape/av_context_record.hpp"

#include "gl_dispatch_client.h"

#include <cstring>

using av::GlContextOptions;
using av::GlContextRig;
using av::MockKmt;
using av::Status;
using av::glicd::GuestGlContext;
using State = GuestGlContext::State;

namespace av::glicd {

struct GuestGlContextProbe {
    static void claim_made(GuestGlContext &context) { context.state_ = GuestGlContext::State::made; }
    static av::icd::HostChannel &channel(GuestGlContext &context) { return context.channel_; }
    static void unterminate(GuestGlContext &context) { std::memset(context.strings_[2], 'x', GuestGlContext::string_bytes); }
    static void connect(GuestGlContext &context, bool connected)
    {
        av::icd::ContextLinkProbe(context.link_).link() = connected ? 0u : av::icd::IcdConnection::no_link;
    }
    static void report_loss(GuestGlContext &context) { context.loss_reported_ = true; }
};

struct ClientArrayStateProbe {
    static void forget_default(ClientArrayState &state, uint32_t known) { state.arrays_known_ = known; }
};

struct UniformShapesProbe {
    static void overfill(UniformShapes &shapes, uint32_t count) { shapes.count_ = count; }
};

struct WindowFramebuffersProbe {
    static void point_nowhere(WindowFramebuffers &windows, std::uint32_t slot) { windows.current_ = slot; }
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

}  // namespace av::glicd

namespace {

constexpr const char *renderer = "av host renderer";

class Context {
public:
    explicit Context(GlContextRig &rig, const GlContextOptions &options = GlContextOptions())
        : context_(rig.connection(), rig.platform(), rig.send_lock(), rig.log(), rig.extensions(), rig.handles(), 7,
                   options)
    {}
    GuestGlContext &operator*() { return context_; }
    GuestGlContext *operator->() { return &context_; }

private:
    GuestGlContext context_;
};

}  // namespace

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
  /* No link yet, so no kernel device to open a shared handle through. */
  CHECK_EQ(context->guest_device(), 0u);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.connection().users(), 0u); }

TEST(guest_gl_context, make_opens_an_opengl_context_with_its_options_and_learns_the_host)
{ GlContextRig rig;
  const GlContextOptions options(4u, 5u, GlContextOptions::Profile::core, GlContextOptions::debug, 0u);
  Context context(rig, options);
  rig.host().gl_make(renderer, "4.6.0 NVIDIA 580");
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
  /* The context's record carries the options it was made with. */
  av::AvContextRecord record;
  CHECK_STATUS(record.decode(rig.kmt().last_private(), rig.kmt().last_private_size()), Status::ok);
  CHECK_EQ(record.options().major(), 4u);
  CHECK_EQ(record.options().minor(), 5u);
  CHECK_EQ(record.options().flags(), GlContextOptions::debug);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.log().lines() == 0u);
  CHECK_STATUS(context->make(), Status::invalid_state);
  CHECK(context->state() == State::made); }

/* Feature 051: what the host granted is read at the make, from
 * GL_CONTEXT_PROFILE_MASK - a version string does not say, and on one of this
 * project's host GPUs the compatibility one is identical to the core one
 * (research.md §1). */
TEST(guest_gl_context, a_make_learns_the_profile_the_host_granted)
{ GlContextRig rig;
  Context context(rig);
  context->asking(av::glicd::GrantedProfile::Rung::compatibility_46);
  rig.host().gl_make_profile(renderer, "4.6.0 NVIDIA 580", 4, 6, 2);
  CHECK_STATUS(context->make(), Status::ok);
  CHECK(context->invariants_hold());
  CHECK(context->profile().rung() == av::glicd::GrantedProfile::Rung::compatibility_46);
  CHECK_EQ(context->profile().major(), 4u);
  CHECK_EQ(context->profile().minor(), 6u);
  CHECK_EQ(context->profile().mask(), 2u);
  CHECK(context->profile().legacy_calls_live()); }

/* A host that answers a core context, or nothing at all, unlocks nothing. */
TEST(guest_gl_context, a_core_or_silent_host_leaves_the_legacy_calls_shut)
{ GlContextRig core;
  Context core_context(core);
  core_context->asking(av::glicd::GrantedProfile::Rung::core_46);
  core.host().gl_make_profile(renderer, "4.6 (Core Profile) Mesa", 4, 6, 1);
  CHECK_STATUS(core_context->make(), Status::ok);
  CHECK_EQ(core_context->profile().mask(), 1u);
  CHECK(!core_context->profile().legacy_calls_live());

  GlContextRig silent;
  Context silent_context(silent);
  silent.host().gl_make_profile(renderer, "1.1", -1, -1, 0);
  CHECK_STATUS(silent_context->make(), Status::ok);
  CHECK(silent_context->invariants_hold());
  CHECK(!silent_context->profile().known());
  CHECK(!silent_context->profile().legacy_calls_live()); }

/* Feature 051, research.md §2: a context created without attributes - what
 * every OpenGL application written before 2009 makes - asks for 4.6
 * compatibility, then for the driver's own default, then for 4.6 core, and
 * reports whichever it got. A context created with attributes asks once. */
TEST(guest_gl_context, a_legacy_context_asks_for_compatibility_first)
{ GlContextRig rig;
  Context context(rig);
  rig.host().gl_make_profile(renderer, "4.6 (Compatibility Profile) Mesa", 4, 6, 2);
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
  /* The host refuses the compatibility ask; its own default answers. */
  rig.kmt().fail_at(MockKmt::Call::create_context_virtual, 1u, MockKmt::invalid_parameter);
  rig.host().gl_make_profile(renderer, "1.4 Mesa", 1, 4, 0);
  CHECK_STATUS(context->make(), Status::ok);
  CHECK(context->profile().rung() == av::glicd::GrantedProfile::Rung::host_default);
  av::AvContextRecord record;
  CHECK_STATUS(record.decode(rig.kmt().last_private(), rig.kmt().last_private_size()), Status::ok);
  CHECK_EQ(record.options().major(), 0u);
  CHECK(record.options().profile() == GlContextOptions::Profile::none);
  /* A context from before profiles existed has the fixed function pipeline. */
  CHECK(context->profile().legacy_calls_live()); }

TEST(guest_gl_context, a_host_with_neither_falls_to_core_and_says_so)
{ GlContextRig rig;
  Context context(rig);
  rig.kmt().fail_first(MockKmt::Call::create_context_virtual, 2u, MockKmt::invalid_parameter);
  rig.host().gl_make_profile(renderer, "4.6 (Core Profile) Mesa", 4, 6, 1);
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

/* A refusal that is not the options' is the answer: no rung is tried after a
 * device that has no memory. */
TEST(guest_gl_context, a_refusal_that_is_not_the_profiles_stops_the_ladder)
{ GlContextRig rig;
  Context context(rig);
  rig.platform().fail_allocation_at(1u);
  CHECK_STATUS(context->make(), Status::no_memory);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_context_virtual), 0u);
  CHECK(context->state() == State::created); }

TEST(guest_gl_context, a_context_created_with_attributes_asks_once)
{ GlContextRig rig;
  const GlContextOptions options(4u, 5u, GlContextOptions::Profile::core, 0u, 0u);
  Context context(rig, options);
  rig.kmt().fail_at(MockKmt::Call::create_context_virtual, 1u, MockKmt::invalid_parameter);
  CHECK_STATUS(context->make(), Status::invalid_parameter);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_context_virtual), 1u);
  CHECK(context->profile().rung() == av::glicd::GrantedProfile::Rung::none); }

/* Feature 051: the device context it was made current on, which a flush that
 * presents puts its frame on. */
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

/* Feature 051: a context nobody made waits for no blank. */
TEST(guest_gl_context, an_unmade_context_paces_nothing)
{ GlContextRig rig;
  Context context(rig);
  /* Pacing an unmade context waits for nothing at all. */
  const std::uint32_t blanks = rig.kmt().calls(MockKmt::Call::wait_for_vertical_blank);
  context->pace();
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_vertical_blank), blanks); }

TEST(guest_gl_context, a_destroyed_context_gives_back_everything)
{ GlContextRig rig;
  {
    Context context(rig);
    rig.host().gl_make(renderer, "4.6");
    CHECK_STATUS(context->make(), Status::ok);
    CHECK(rig.kmt().open_objects() > GlContextRig::link_objects);
  }
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.connection().users(), 0u);
  CHECK_EQ(rig.platform().live_allocations(), 0u);
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
  rig.platform().fail_allocation_at(1u);
  CHECK_STATUS(context->make(), Status::no_memory);
  CHECK(context->invariants_hold());
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.connection().users(), 0u);
  CHECK_EQ(rig.platform().live_allocations(), 0u); }

/* Each D3DKMT step of the transport's setup refused in turn: the options are
 * the driver's to refuse, and nothing is left. */
TEST(guest_gl_context, a_driver_refusal_of_the_transport_is_invalid_parameter_with_nothing_held)
{ /* `count` refusals from the `nth` on: the views escape is asked again in
   * its older version, so refusing it takes both. */
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
  /* The device will not import the arena at its size, nor at the half of it
   * that is the last size still above the least an arena is opened at. */
  rig.host().blob_import(1u, 0u);
  rig.host().blob_import(1u, 0u);
  rig.host().gl_string("vendor");
  rig.host().gl_string(renderer);
  rig.host().gl_string("4.6");
  rig.host().gl_string("4.60");
  rig.host().gl_integer(0);
  rig.host().gl_integer(0);
  rig.host().gl_integer(0);
  /* GL_MAJOR_VERSION, GL_MINOR_VERSION, GL_CONTEXT_PROFILE_MASK. */
  rig.host().gl_integer(4);
  rig.host().gl_integer(6);
  rig.host().gl_integer(2);
  CHECK_STATUS(context->make(), Status::ok);
  CHECK_EQ(rig.log().holding("no blob arena"), 1u);
  char bytes[512];
  av::StateWriter writer(bytes, sizeof bytes);
  context->report(writer, 9u);
  CHECK(std::strstr(writer.dump(), " arena=false ") != nullptr);
  /* Without an arena there is no error word for the host to have written. */
  std::uint32_t error = 7u;
  CHECK(!context->take_host_error(error));
  CHECK_EQ(error, 0u);
  CHECK(context->invariants_hold()); }

/* The host makes its context when the first command runs: a context it
 * cannot make faults, and make answers the refusal of the options. */
TEST(guest_gl_context, a_host_refusal_is_invalid_parameter_with_nothing_held)
{ for (std::uint32_t wait = 2u; wait <= GlContextRig::link_objects + 5u; wait++) {
      GlContextRig rig;
      Context context(rig);
      rig.host().gl_make(renderer, "4.6");
      rig.kmt().withhold_answer_at(wait);
      CHECK_STATUS(context->make(), Status::invalid_parameter);
      CHECK(context->state() == State::created);
      /* One line per ask the ladder made (feature 051): the host refused the
       * compatibility context, its own default and the core one - or the
       * ask's transport faulted on the arena, before the host was asked. */
      CHECK_EQ(rig.log().holding("the host refused the context") +
                   rig.log().holding("faulted opening its blob arena"),
               3u);
      CHECK_EQ(rig.kmt().open_objects(), 0u);
      CHECK_EQ(rig.platform().live_allocations(), 0u);
  } }

TEST(guest_gl_context, a_faulted_transport_makes_the_context_lost)
{ GlContextRig rig;
  Context context(rig);
  rig.host().gl_make(renderer, "4.6");
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
  /* A lost context still answers what it learned. */
  CHECK(std::strcmp(context->string(GuestGlContext::gl_renderer), renderer) == 0);
  CHECK(context->share_name() != 0u);
  CHECK(context->invariants_hold()); }

/* A lost context reports its loss once, after any error it recorded: a robust
 * one - robust access with lose-on-reset - as GL_CONTEXT_LOST, with
 * GL_UNKNOWN_CONTEXT_RESET as its reset status every time, any other as
 * GL_OUT_OF_MEMORY with no reset status. A context not lost reports neither. */
TEST(guest_gl_context, a_lost_context_reports_its_loss_by_its_robustness)
{ const std::uint8_t robust_flags = GlContextOptions::robust_access | GlContextOptions::lose_context_on_reset;
  const std::uint8_t flag_sets[] = {robust_flags, GlContextOptions::robust_access, 0u};
  for (const std::uint8_t flags : flag_sets) {
    const bool robust = flags == robust_flags;
    GlContextRig rig;
    Context context(rig, GlContextOptions(4u, 6u, GlContextOptions::Profile::core, flags, 0u));
    CHECK_EQ(context->robust(), robust);
    CHECK_EQ(context->lost_reset_status(), 0u);
    rig.host().gl_make(renderer, "4.6");
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

/* Another context's fault loses this one at its next call, and its deletion
 * then asks the host nothing: the blob arena is abandoned, not closed. */
TEST(guest_gl_context, a_fault_on_its_device_loses_it_and_it_goes_without_the_host)
{ GlContextRig rig;
  std::uint32_t commands = 0u;
  {
    Context first(rig);
    Context second(rig);
    rig.host().gl_make(renderer, "4.6");
    CHECK_STATUS(first->make(), Status::ok);
    rig.host().gl_make(renderer, "4.6");
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
      rig.host().gl_make(renderer, row.host);
      CHECK_STATUS(context->make(), Status::ok);
      CHECK(std::strcmp(context->string(GuestGlContext::gl_version), row.reported) == 0);
  } }

TEST(guest_gl_context, a_string_longer_than_its_row_is_cut)
{ GlContextRig rig;
  Context context(rig);
  char long_name[400];
  std::memset(long_name, 'n', sizeof long_name - 1u);
  long_name[sizeof long_name - 1u] = '\0';
  rig.host().gl_make(long_name, "4.6");
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
                                    "profile=2 flags=0 share=0 share_target=0 error=0 link=4 arena=false renderer= extensions=0\n"
                                    "context/65537/windows/0 held=0 draws_window=false reads_window=false\n"
                     "context/65537/mappings/0 held=0 persistent_writers=0\n"
                     "context/65537/shadows/0 trusted=true held=0 held_bytes=0 sizes=0") == 0);
  rig.host().gl_make(renderer, "4.6");
  CHECK_STATUS(context->make(), Status::ok);
  context->set_owner(2u);
  context->record(0x0502u);
  av::StateWriter made(bytes, sizeof bytes);
  context->report(made, 1u);
  CHECK(std::strcmp(made.dump(), "context/1 state=made owner=2 pixel_format=7 major=3 minor=3 profile=2 flags=0 "
                                 "share=0 share_target=0 error=1282 link=0 arena=true renderer=av host renderer extensions=0\n"
                                 "context/1/windows/0 held=0 draws_window=false reads_window=false\n"
                     "context/1/mappings/0 held=0 persistent_writers=0\n"
                     "context/1/shadows/0 trusted=true held=0 held_bytes=0 sizes=0") == 0); }

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
  av::glicd::GuestGlContextProbe::connect(*connected, true);
  CHECK(!connected->invariants_hold());
  av::glicd::GuestGlContextProbe::connect(*connected, false);
  CHECK(connected->invariants_hold());
  /* A loss reported by a context that is not lost. */
  Context reported(rig);
  av::glicd::GuestGlContextProbe::report_loss(*reported);
  CHECK(!reported->invariants_hold()); }

TEST(guest_gl_context_invariants, fail_when_a_made_context_or_its_client_state_is_broken)
{ using av::glicd::GuestGlContextProbe;
  GlContextRig rig;
  Context made(rig);
  rig.host().gl_make(renderer, "4.6");
  CHECK_STATUS(made->make(), Status::ok);
  CHECK(made->invariants_hold());
  av::icd::HostChannelProbe channel(GuestGlContextProbe::channel(*made));
  channel.drop_batched();
  CHECK(!made->invariants_hold());
  channel.restore_batched();
  CHECK(made->invariants_hold());
  GuestGlContextProbe::connect(*made, false);
  CHECK(!made->invariants_hold());
  GuestGlContextProbe::connect(*made, true);
  av::glicd::ClientArrayStateProbe::forget_default(made->arrays(), 0u);
  CHECK(!made->invariants_hold());
  av::glicd::ClientArrayStateProbe::forget_default(made->arrays(), 1u);
  av::glicd::UniformShapesProbe::overfill(made->uniforms(), av::glicd::UniformShapes::capacity + 1u);
  CHECK(!made->invariants_hold());
  av::glicd::UniformShapesProbe::overfill(made->uniforms(), 0u);
  av::glicd::QueryShapesProbe::overflow(made->queries(), 0x80000000u);
  CHECK(!made->invariants_hold());
  av::glicd::QueryShapesProbe::overflow(made->queries(), 0u);
  CHECK(made->invariants_hold());
  av::glicd::WindowFramebuffersProbe::point_nowhere(made->framebuffers(), 0u);
  CHECK(!made->invariants_hold());
  av::glicd::WindowFramebuffersProbe::point_nowhere(made->framebuffers(), av::glicd::WindowFramebuffers::max_windows);
  av::glicd::MappedBuffersProbe::miscount(made->mappings(), 1u);
  CHECK(!made->invariants_hold());
  av::glicd::MappedBuffersProbe::miscount(made->mappings(), 0u);
  av::glicd::BufferShadowsProbe::miscount(made->shadows(), 1u);
  CHECK(!made->invariants_hold());
  av::glicd::BufferShadowsProbe::miscount(made->shadows(), 0u);
  CHECK(made->invariants_hold()); }

/* Buffers another context can write are not this one's to know: a context
 * that shares, or that another shares, keeps no copies of them. */
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

/* The copies are of the host context's buffers, and go with it. */
TEST(guest_gl_context, a_make_that_fails_keeps_no_copies_of_buffers)
{ GlContextRig rig;
  Context      context(rig);
  std::uint8_t bytes[4] = {};
  context->shadows().keep(1u, 0u, sizeof bytes, bytes);
  CHECK_EQ(context->shadows().held(), 1u);
  rig.platform().fail_allocation_at(1u);
  CHECK_STATUS(context->make(), Status::no_memory);
  CHECK_EQ(context->shadows().held(), 0u);
  CHECK_EQ(rig.platform().live_allocations(), 0u); }

/* The newest batch the context sent, for the present thread to wait on: none
 * before the make, and one it can wait on after a batch. */
TEST(guest_gl_context, marks_its_newest_batch_for_another_thread)
{ GlContextRig rig;
  Context      context(rig);
  CHECK(!context->mark().waitable());
  rig.host().gl_make(renderer, "4.6");
  CHECK_STATUS(context->make(), Status::ok);
  av::glproto::av_glFlush(*context->wire());
  context->flush();
  const av::BatchMark mark = context->mark();
  CHECK(mark.waitable());
  CHECK(mark.fence() != 0u); }

namespace {

/* One frame as an application ends it: a call, then glFlush. */
std::uint64_t end_a_frame(GuestGlContext &context)
{
    av::glproto::av_glFlush(*context.wire());
    context.end_frame();
    return av::glicd::GuestGlContextProbe::channel(context).transport().published_fence();
}

}  // namespace

namespace {

/* Each read of the clock 60 ms on: every frame ends more than
 * FramesInFlight::latency_ms after the one before it. */
constexpr std::uint64_t frame_apart = av::MockKmt::ticks_per_second / 1000u * 60u;

}  // namespace

/* A host that finishes each frame before the next ends is never waited for,
 * and nothing is asked of the kernel to learn it. */
TEST(guest_gl_context, a_frame_ends_without_waiting_on_a_host_that_keeps_up)
{ GlContextRig rig;
  Context      context(rig);
  rig.host().gl_make(renderer, "4.6");
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

/* A frame ending while the one before it, longer ago than the latency, is
 * unfinished waits for it; a host that never finishes it, and has gone
 * quiet, loses the context, which says so. */
TEST(guest_gl_context, a_frame_the_host_never_finishes_loses_the_context)
{ GlContextRig rig;
  Context      context(rig);
  rig.host().gl_make(renderer, "4.6");
  CHECK_STATUS(context->make(), Status::ok);
  rig.kmt().reads_clock(nullptr, frame_apart);
  end_a_frame(*context);
  CHECK(context->wire() != nullptr);
  end_a_frame(*context);
  rig.kmt().reads_clock(nullptr, 0u);
  CHECK(context->wire() == nullptr);
  CHECK(context->state() == State::lost);
  CHECK_EQ(rig.log().holding("never finished an earlier frame"), 1u);
  /* A lost context ends no frame and waits for nothing. */
  context->end_frame(); }

/* Feature 053: the context owns the memory objects and semaphores it imported,
 * because a GL name belongs to the context that made it. */
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
  /* A second context's names are its own, not this one's. */
  Context other(rig);
  CHECK_EQ(other->externals().live(), 0u);
  CHECK_EQ(other->externals().made(av::SharedObjectKind::memory, name), 0u); }
