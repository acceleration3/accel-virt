/* One application context and its host context: nothing on the host until
 * made, made whole or not at all, lost when its transport faults, and the
 * guest state and host strings it answers from. */
#include "core/trace/state_writer.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/gl_context_rig.hpp"
#include "icd/opengl/guest_gl_context.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/context/rvk_context_record.hpp"

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
    static void drop_staging(GuestGlContext &context, void *other)
    {
        context.staging_ = static_cast<uint8_t *>(other);
    }
    static void unterminate(GuestGlContext &context) { std::memset(context.strings_[2], 'x', GuestGlContext::string_bytes); }
    static void connect(GuestGlContext &context, bool connected) { context.connected_ = connected; }
    static void drop_lock(GuestGlContext &context) { context.locked_.destroy(); }
    static void restore_lock(GuestGlContext &context)
    {
        static_cast<void>(context.locked_.construct(context.transport_.object(), context.send_lock_));
    }
};

struct ClientArrayStateProbe {
    static void forget_default(ClientArrayState &state, uint32_t known) { state.arrays_known_ = known; }
};

struct UniformShapesProbe {
    static void overfill(UniformShapes &shapes, uint32_t count) { shapes.count_ = count; }
};

struct QueryShapesProbe {
    static void overflow(QueryShapes &shapes, uint32_t length) { shapes.lengths_[1] = length; }
};

}  // namespace av::glicd

namespace {

constexpr const char *renderer = "rvk host renderer";

class Context {
public:
    explicit Context(GlContextRig &rig, const GlContextOptions &options = GlContextOptions())
        : context_(rig.connection(), rig.platform(), rig.send_lock(), rig.log(), rig.extensions(), 7, options)
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
  CHECK(std::strcmp(context->string(GuestGlContext::gl_vendor), "rvk host vendor") == 0);
  CHECK(std::strcmp(context->string(GuestGlContext::gl_renderer), renderer) == 0);
  CHECK(std::strcmp(context->string(GuestGlContext::gl_version), "4.6.0 NVIDIA 580") == 0);
  CHECK(std::strcmp(context->string(GuestGlContext::gl_shading_language_version), "4.60") == 0);
  CHECK(context->string(0x1F03u) == nullptr);
  CHECK_EQ(context->queries().values(0x86A3u), 3u);
  CHECK_EQ(context->queries().values(0x87FFu), 1u);
  CHECK_EQ(context->queries().values(0x8DF8u), 0u);
  /* The context's record carries the options it was made with. */
  av::RvkContextRecord record;
  CHECK_STATUS(record.decode(rig.kmt().last_private(), rig.kmt().last_private_size()), Status::ok);
  CHECK_EQ(record.options().major(), 4u);
  CHECK_EQ(record.options().minor(), 5u);
  CHECK_EQ(record.options().flags(), GlContextOptions::debug);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.log().lines() == 0u);
  CHECK_STATUS(context->make(), Status::invalid_state);
  CHECK(context->state() == State::made); }

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
  CHECK_EQ(rig.kmt().bindings(), 0u); }

TEST(guest_gl_context, no_rvkgpu_device_is_not_supported)
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
{ struct Step { MockKmt::Call call; std::uint32_t nth; };
  const Step steps[] = {
      {MockKmt::Call::create_allocation, 1u}, {MockKmt::Call::create_allocation, 2u},
      {MockKmt::Call::create_allocation, 3u}, {MockKmt::Call::create_allocation, 4u},
      {MockKmt::Call::lock, 1u},              {MockKmt::Call::create_context_virtual, 1u},
  };
  for (const Step &step : steps) {
      GlContextRig rig;
      Context context(rig);
      rig.kmt().fail_at(step.call, step.nth, MockKmt::invalid_parameter);
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
  /* The transport's four allocations, then the arena's chunk. */
  rig.kmt().fail_at(MockKmt::Call::create_allocation, 5u, MockKmt::invalid_parameter);
  rig.host().gl_string("vendor");
  rig.host().gl_string(renderer);
  rig.host().gl_string("4.6");
  rig.host().gl_string("4.60");
  rig.host().gl_integer(0);
  rig.host().gl_integer(0);
  rig.host().gl_integer(0);
  CHECK_STATUS(context->make(), Status::ok);
  CHECK_EQ(rig.log().holding("no blob arena"), 1u);
  char bytes[512];
  av::StateWriter writer(bytes, sizeof bytes);
  context->report(writer, 9u);
  CHECK(std::strstr(writer.dump(), " arena=false ") != nullptr);
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
      CHECK_EQ(rig.log().holding("the host refused the context"), 1u);
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
  CHECK_EQ(av::glproto::rvk_glGetError(*wire), 0u);
  CHECK(context->wire() == nullptr);
  CHECK(context->state() == State::lost);
  CHECK(context->wire() == nullptr);
  CHECK_EQ(rig.log().holding("is lost"), 1u);
  /* A lost context still answers what it learned. */
  CHECK(std::strcmp(context->string(GuestGlContext::gl_renderer), renderer) == 0);
  CHECK(context->share_name() != 0u);
  CHECK(context->invariants_hold()); }

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
                                    "profile=2 flags=0 share=0 error=0 arena=false renderer= extensions=0\n"
                                    "context/65537/windows/0 held=0 draws_window=false reads_window=false") == 0);
  rig.host().gl_make(renderer, "4.6");
  CHECK_STATUS(context->make(), Status::ok);
  context->set_owner(2u);
  context->record(0x0502u);
  av::StateWriter made(bytes, sizeof bytes);
  context->report(made, 1u);
  CHECK(std::strcmp(made.dump(), "context/1 state=made owner=2 pixel_format=7 major=3 minor=3 profile=2 flags=0 "
                                 "share=0 error=1282 arena=true renderer=rvk host renderer extensions=0\n"
                                 "context/1/windows/0 held=0 draws_window=false reads_window=false") == 0); }

TEST(guest_gl_context_invariants, fail_when_the_state_and_what_is_held_disagree)
{ GlContextRig rig;
  Context claimed(rig);
  av::glicd::GuestGlContextProbe::claim_made(*claimed);
  CHECK(!claimed->invariants_hold());
  Context staged(rig);
  int block = 0;
  av::glicd::GuestGlContextProbe::drop_staging(*staged, &block);
  CHECK(!staged->invariants_hold());
  av::glicd::GuestGlContextProbe::drop_staging(*staged, nullptr);
  Context unterminated(rig);
  av::glicd::GuestGlContextProbe::unterminate(*unterminated);
  CHECK(!unterminated->invariants_hold());
  Context connected(rig);
  av::glicd::GuestGlContextProbe::connect(*connected, true);
  CHECK(!connected->invariants_hold());
  av::glicd::GuestGlContextProbe::connect(*connected, false);
  CHECK(connected->invariants_hold()); }

TEST(guest_gl_context_invariants, fail_when_a_made_context_or_its_client_state_is_broken)
{ using av::glicd::GuestGlContextProbe;
  GlContextRig rig;
  Context made(rig);
  rig.host().gl_make(renderer, "4.6");
  CHECK_STATUS(made->make(), Status::ok);
  CHECK(made->invariants_hold());
  GuestGlContextProbe::drop_lock(*made);
  CHECK(!made->invariants_hold());
  GuestGlContextProbe::restore_lock(*made);
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
  CHECK(made->invariants_hold()); }
