#include "harness/framework/test_registry.hpp"
#include "harness/icd/gl_icd_rig.hpp"
#include "icd/opengl/buffer_shadows.hpp"
#include "icd/opengl/gl_context_table.hpp"
#include "icd/opengl/gl_icd_root.hpp"
#include "icd/opengl/guest_gl_context.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/escape/av_context_record.hpp"
#include "proto/submission/command_header.hpp"

#include <cstdint>

namespace av::glicd {

struct GlIcdRootProbe {
    static GuestGlContext *context(GlIcdRoot &root, std::uint64_t handle) { return root.contexts_.resolve(handle); }
};

}

using av::GlIcdRig;
using av::MockKmt;
using av::Status;
using av::glicd::GuestGlContext;
using Root = av::glicd::GlIcdRoot;

namespace {

using GetErrorFn = std::uint32_t (APIENTRY *)();
constexpr std::uint32_t slot_get_error = 261u;

void check_error(GlIcdRig &rig, std::uint32_t expected)
{
    CHECK_EQ(rig.threads().last_error(), expected);
    rig.threads().clear_last_error();
}

GuestGlContext *current_on(GlIcdRig &rig, void *context)
{
    rig.host().gl_window(1u, 1u);
    CHECK(rig.opengl32().make_current(rig.dc(), context));
    return rig.root().current();
}

std::uint32_t last_record_share(GlIcdRig &rig)
{
    av::AvContextRecord record;
    CHECK_STATUS(record.decode(rig.kmt().last_private(), rig.kmt().last_private_size()), Status::ok);
    return record.options().share_context();
}

}

TEST(gl_share_lists, a_chain_is_made_from_its_far_end_each_naming_the_one_before)
{ GlIcdRig rig;
  rig.load_with_format();
  void *const a = rig.opengl32().create_context(rig.dc());
  void *const b = rig.opengl32().create_context(rig.dc());
  void *const c = rig.opengl32().create_context(rig.dc());
  CHECK(rig.opengl32().share_lists(a, b));
  CHECK(rig.opengl32().share_lists(b, c));
  CHECK(rig.root().invariants_hold());
  for (int made = 0; made < 3; made++) {
      rig.host().gl_make("host gpu", "4.6");
      rig.host().gl_policy(0);
  }
  const std::uint32_t opened = rig.kmt().calls(MockKmt::Call::create_context_virtual);
  GuestGlContext *const third = current_on(rig, c);
  CHECK(third != nullptr);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_context_virtual) - opened, 3u);
  CHECK_EQ(last_record_share(rig), third->options().share_context());
  GuestGlContext *const second = current_on(rig, b);
  GuestGlContext *const first = current_on(rig, a);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_context_virtual) - opened, 3u);
  CHECK(second->share_name() != 0u);
  CHECK_EQ(third->options().share_context(), second->share_name());
  CHECK_EQ(second->options().share_context(), first->share_name());
  CHECK_EQ(first->options().share_context(), 0u);
  CHECK_EQ(second->options().major(), 1u);
  CHECK_EQ(first->options().major(), 4u);
  CHECK(first->options().profile() == av::GlContextOptions::Profile::compatibility);
  CHECK_EQ(third->share_target(), rig.opengl32().driver_context(b));
  CHECK(!first->shadows().trusted());
  CHECK(!second->shadows().trusted());
  CHECK(!third->shadows().trusted());
  CHECK(rig.root().invariants_hold()); }

TEST(gl_share_lists, a_share_into_a_context_once_current_or_unknown_or_same_or_into_a_loop_is_refused)
{ GlIcdRig rig;
  rig.load_with_format();
  void *const a = rig.opengl32().create_context(rig.dc());
  void *const b = rig.opengl32().create_context(rig.dc());
  void *const c = rig.opengl32().create_context(rig.dc());
  CHECK(!rig.opengl32().share_lists(a, reinterpret_cast<void *>(0x5u)));
  check_error(rig, Root::error_invalid_handle);
  CHECK(!rig.opengl32().share_lists(reinterpret_cast<void *>(0x5u), a));
  check_error(rig, Root::error_invalid_handle);
  CHECK(!rig.opengl32().share_lists(a, a));
  check_error(rig, Root::error_invalid_operation);
  CHECK(rig.opengl32().share_lists(a, b));
  CHECK(rig.opengl32().share_lists(b, c));
  CHECK(!rig.opengl32().share_lists(c, a));
  check_error(rig, Root::error_invalid_operation);
  CHECK(!rig.opengl32().share_lists(c, b));
  check_error(rig, Root::error_invalid_operation);
  void *const d = rig.opengl32().create_context(rig.dc());
  CHECK(!rig.opengl32().share_lists(d, b));
  check_error(rig, Root::error_invalid_operation);
  CHECK(rig.opengl32().share_lists(d, a));
  void *const made = rig.opengl32().create_context(rig.dc());
  rig.host().gl_make("host gpu", "4.6");
  rig.host().gl_policy(0);
  CHECK(current_on(rig, made) != nullptr);
  CHECK(rig.opengl32().make_current(rig.dc(), nullptr));
  void *const fresh = rig.opengl32().create_context(rig.dc());
  CHECK(!rig.opengl32().share_lists(fresh, made));
  check_error(rig, Root::error_invalid_operation);
  CHECK(rig.root().invariants_hold()); }

TEST(gl_share_lists, a_context_in_use_lends_its_objects_to_one_never_current)
{ GlIcdRig rig;
  rig.load_with_format();
  void *const in_use = rig.opengl32().create_context(rig.dc());
  rig.host().gl_make("host gpu", "4.6");
  rig.host().gl_policy(0);
  GuestGlContext *const lender = current_on(rig, in_use);
  CHECK(lender != nullptr);
  if (lender == nullptr) return;
  void *const fresh = rig.opengl32().create_context(rig.dc());
  CHECK(rig.opengl32().share_lists(in_use, fresh));
  const std::uint32_t opened = rig.kmt().calls(MockKmt::Call::create_context_virtual);
  rig.host().gl_make("host gpu", "4.6");
  rig.host().gl_policy(0);
  GuestGlContext *const borrower = current_on(rig, fresh);
  CHECK(borrower != nullptr);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_context_virtual) - opened, 1u);
  CHECK(lender->share_name() != 0u);
  CHECK_EQ(last_record_share(rig), lender->share_name());
  CHECK(!lender->shadows().trusted());
  CHECK(rig.root().invariants_hold()); }

TEST(gl_share_lists, a_context_made_with_attributes_and_never_current_is_made_again_sharing)
{ GlIcdRig rig;
  rig.load_with_format();
  using AttribsFn = void *(APIENTRY *)(void *, void *, const std::int32_t *);
  const auto create = reinterpret_cast<AttribsFn>(rig.opengl32().driver_proc_address("wglCreateContextAttribsARB"));
  void *const in_use = rig.opengl32().create_context(rig.dc());
  rig.host().gl_make("host gpu", "4.6");
  rig.host().gl_policy(0);
  GuestGlContext *const lender = current_on(rig, in_use);
  CHECK(lender != nullptr);
  if (lender == nullptr) return;
  const std::int32_t three_oh[] = {0x2091, 3, 0x2092, 0, 0};
  rig.host().gl_make("host gpu", "4.6");
  rig.host().gl_policy(0);
  void *const compute = create(rig.dc(), nullptr, three_oh);
  CHECK(compute != nullptr);
  const std::uint32_t destroyed = rig.kmt().calls(MockKmt::Call::destroy_context);
  CHECK(rig.opengl32().share_lists(in_use, compute));
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::destroy_context) - destroyed, 1u);
  const std::uint32_t opened = rig.kmt().calls(MockKmt::Call::create_context_virtual);
  rig.host().gl_make("host gpu", "4.6");
  rig.host().gl_policy(0);
  GuestGlContext *const borrower = current_on(rig, compute);
  CHECK(borrower != nullptr);
  if (borrower == nullptr) return;
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_context_virtual) - opened, 1u);
  av::AvContextRecord record;
  CHECK_STATUS(record.decode(rig.kmt().last_private(), rig.kmt().last_private_size()), Status::ok);
  CHECK_EQ(record.options().share_context(), lender->share_name());
  CHECK_EQ(record.options().major(), 3u);
  CHECK_EQ(record.options().minor(), 0u);
  CHECK(rig.opengl32().make_current(rig.dc(), nullptr));
  void *const other = rig.opengl32().create_context(rig.dc());
  CHECK(!rig.opengl32().share_lists(other, compute));
  check_error(rig, Root::error_invalid_operation);
  CHECK(rig.root().invariants_hold()); }

TEST(gl_share_lists, a_deleted_target_shares_nothing_and_a_deleted_context_is_no_target)
{ GlIcdRig rig;
  rig.load_with_format();
  void *const a = rig.opengl32().create_context(rig.dc());
  void *const b = rig.opengl32().create_context(rig.dc());
  CHECK(rig.opengl32().share_lists(a, b));
  const std::uintptr_t deleted = rig.opengl32().driver_context(a);
  CHECK(rig.opengl32().delete_context(a));
  CHECK(!rig.root().share_lists(deleted, rig.opengl32().driver_context(b)));
  check_error(rig, Root::error_invalid_handle);
  rig.host().gl_make("host gpu", "4.6");
  rig.host().gl_policy(0);
  const std::uint32_t opened = rig.kmt().calls(MockKmt::Call::create_context_virtual);
  GuestGlContext *const alone = current_on(rig, b);
  CHECK(alone != nullptr);
  CHECK(alone->shadows().trusted());
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_context_virtual) - opened, 1u);
  CHECK_EQ(alone->options().share_context(), 0u);
  CHECK_EQ(last_record_share(rig), 0u); }

TEST(gl_share_lists, a_target_that_cannot_be_made_or_was_lost_fails_the_make)
{ GlIcdRig rig;
  rig.load_with_format();
  void *const a = rig.opengl32().create_context(rig.dc());
  void *const b = rig.opengl32().create_context(rig.dc());
  CHECK(rig.opengl32().share_lists(a, b));
  rig.kmt().fail_at(MockKmt::Call::create_context_virtual, 1u, MockKmt::invalid_parameter);
  CHECK(!rig.opengl32().make_current(rig.dc(), b));
  check_error(rig, Root::error_invalid_operation);
  CHECK(rig.root().current() == nullptr);
  rig.host().gl_make("host gpu", "4.6");
  rig.host().gl_policy(0);
  CHECK(current_on(rig, a) != nullptr);
  rig.kmt().withhold_answer_at(1u);
  CHECK_EQ(rig.opengl32().gl<GetErrorFn>(slot_get_error)(), 0u);
  CHECK(rig.root().current()->wire() == nullptr);
  const std::uint32_t opened = rig.kmt().calls(MockKmt::Call::create_context_virtual);
  CHECK(!rig.opengl32().make_current(rig.dc(), b));
  check_error(rig, Root::error_invalid_operation);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_context_virtual), opened);
  CHECK(rig.root().invariants_hold()); }

TEST(gl_share_lists, a_context_records_one_target_and_only_while_unmade)
{ GlIcdRig rig;
  rig.load_with_format();
  void *const a = rig.opengl32().create_context(rig.dc());
  rig.host().gl_make("host gpu", "4.6");
  rig.host().gl_policy(0);
  GuestGlContext *const context = current_on(rig, a);
  CHECK_STATUS(context->share_after(9u), Status::invalid_state);
  CHECK_STATUS(context->share(9u), Status::invalid_state);
  CHECK_EQ(context->share_target(), 0u); }

TEST(gl_share_lists, a_loop_of_targets_is_walked_no_further_than_the_table)
{ GlIcdRig rig;
  rig.load_with_format();
  void *const x = rig.opengl32().create_context(rig.dc());
  void *const y = rig.opengl32().create_context(rig.dc());
  void *const z = rig.opengl32().create_context(rig.dc());
  const std::uintptr_t hx = rig.opengl32().driver_context(x);
  const std::uintptr_t hy = rig.opengl32().driver_context(y);
  using Probe = av::glicd::GlIcdRootProbe;
  CHECK_STATUS(Probe::context(rig.root(), hx)->share_after(hy), Status::ok);
  CHECK_STATUS(Probe::context(rig.root(), hy)->share_after(hx), Status::ok);
  CHECK(rig.opengl32().share_lists(x, z));
  const std::uint32_t opened = rig.kmt().calls(MockKmt::Call::create_context_virtual);
  CHECK(!rig.opengl32().make_current(rig.dc(), z));
  check_error(rig, Root::error_invalid_operation);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_context_virtual), opened); }

TEST(gl_share_lists, a_walk_through_a_deleted_target_stops_at_it)
{ GlIcdRig rig;
  rig.load_with_format();
  void *const a = rig.opengl32().create_context(rig.dc());
  void *const b = rig.opengl32().create_context(rig.dc());
  void *const c = rig.opengl32().create_context(rig.dc());
  CHECK(rig.opengl32().share_lists(a, b));
  CHECK(rig.opengl32().delete_context(a));
  CHECK(rig.opengl32().share_lists(b, c));
  CHECK_EQ(av::glicd::GlIcdRootProbe::context(rig.root(), rig.opengl32().driver_context(c))->share_target(),
           rig.opengl32().driver_context(b)); }

TEST(gl_share_lists, the_current_share_name_is_the_current_context_s_and_none_without_one)
{ GlIcdRig rig;
  rig.load_with_format();
  Root &root = rig.root();
  CHECK_EQ(root.current_share_name(), 0u);
  void *const a = rig.opengl32().create_context(rig.dc());
  void *const b = rig.opengl32().create_context(rig.dc());
  rig.host().gl_make("host gpu", "4.6");
  rig.host().gl_policy(0);
  GuestGlContext *const first = current_on(rig, a);
  CHECK(first != nullptr);
  CHECK(first->share_name() != 0u);
  CHECK_EQ(root.current_share_name(), first->share_name());
  rig.host().gl_make("host gpu", "4.6");
  rig.host().gl_policy(0);
  GuestGlContext *const second = current_on(rig, b);
  CHECK(second != nullptr);
  CHECK(second->share_name() != first->share_name());
  CHECK_EQ(root.current_share_name(), second->share_name());
  CHECK(rig.opengl32().make_current(rig.dc(), nullptr));
  CHECK_EQ(root.current_share_name(), 0u);
  CHECK(root.invariants_hold()); }

TEST(gl_share_lists, the_current_adapter_luid_is_the_adapter_of_the_current_context_and_none_without_one)
{ GlIcdRig rig;
  rig.load_with_format();
  Root &root = rig.root();
  CHECK_EQ(root.current_adapter_luid(), 0u);
  void *const a = rig.opengl32().create_context(rig.dc());
  rig.host().gl_make("host gpu", "4.6");
  rig.host().gl_policy(0);
  CHECK(current_on(rig, a) != nullptr);
  CHECK(root.current_adapter_luid() != 0u);
  CHECK(rig.opengl32().make_current(rig.dc(), nullptr));
  CHECK_EQ(root.current_adapter_luid(), 0u);
  CHECK(root.invariants_hold()); }

TEST(gl_share_lists, run_held_calls_runs_the_current_context_s_and_nothing_without_one)
{ GlIcdRig rig;
  rig.load_with_format();
  Root &root = rig.root();
  const std::uint32_t idle = rig.kmt().commands();
  CHECK_EQ(root.run_held_calls(), 0);
  CHECK_EQ(rig.kmt().commands(), idle);
  void *const a = rig.opengl32().create_context(rig.dc());
  rig.host().gl_make("host gpu", "4.6");
  rig.host().gl_policy(0);
  CHECK(current_on(rig, a) != nullptr);
  const std::uint32_t before = rig.kmt().commands();
  rig.host().nop();
  CHECK_EQ(root.run_held_calls(), 1);
  CHECK(rig.kmt().commands() > before);
  CHECK_EQ(rig.kmt().command_id(rig.kmt().commands() - 1u), av::CommandHeader::nop_id);
  CHECK(rig.opengl32().make_current(rig.dc(), nullptr));
  const std::uint32_t released = rig.kmt().commands();
  CHECK_EQ(root.run_held_calls(), 0);
  CHECK_EQ(rig.kmt().commands(), released);
  CHECK(root.invariants_hold()); }

TEST(gl_share_lists, a_lost_context_cannot_be_shared_with)
{ GlIcdRig rig;
  rig.load_with_format();
  void *const lost = rig.opengl32().create_context(rig.dc());
  void *const other = rig.opengl32().create_context(rig.dc());
  rig.host().gl_make("host gpu", "4.6");
  rig.host().gl_policy(0);
  GuestGlContext *const context = current_on(rig, lost);
  CHECK(context != nullptr);
  rig.kmt().withhold_answer_at(1u);
  CHECK_EQ(rig.opengl32().gl<GetErrorFn>(slot_get_error)(), 0u);
  CHECK(context->wire() == nullptr);
  CHECK(context->state() == GuestGlContext::State::lost);
  CHECK(!rig.opengl32().share_lists(lost, other));
  check_error(rig, Root::error_invalid_operation);
  CHECK(rig.root().invariants_hold()); }
