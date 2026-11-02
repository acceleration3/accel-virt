#include "harness/framework/test_registry.hpp"
#include "harness/icd/gl_context_rig.hpp"
#include "harness/icd/scripted_thread_slot.hpp"
#include "icd/opengl/current_contexts.hpp"
#include "icd/opengl/guest_gl_context.hpp"

using av::GlContextOptions;
using av::GlContextRig;
using av::ScriptedThreadSlot;
using av::glicd::CurrentContexts;
using av::glicd::GuestGlContext;

namespace {

struct Threads {
    GlContextRig       rig;
    ScriptedThreadSlot slot;
    CurrentContexts    currents{slot};
    GuestGlContext     first{rig.connection(), rig.platform(), rig.send_lock(), rig.log(), rig.extensions(), rig.handles(), 1,
                          GlContextOptions()};
    GuestGlContext     second{rig.connection(), rig.platform(), rig.send_lock(), rig.log(), rig.extensions(), rig.handles(), 1,
                          GlContextOptions()};
};

}

TEST(current_contexts, a_context_is_current_on_one_thread_and_another_is_refused_it)
{ Threads threads;
  CurrentContexts &currents = threads.currents;
  CHECK(currents.invariants_hold());
  CHECK(currents.current() == nullptr);
  CHECK_EQ(currents.thread(), 1u);
  CHECK(currents.make_current(threads.first));
  CHECK(currents.invariants_hold());
  CHECK(currents.current() == &threads.first);
  CHECK_EQ(threads.first.owner(), 1u);
  CHECK_EQ(currents.anywhere(), 1u);
  threads.slot.switch_to(2u);
  CHECK(currents.current() == nullptr);
  CHECK(currents.current_elsewhere(threads.first));
  CHECK(!currents.make_current(threads.first));
  CHECK(currents.current() == nullptr);
  CHECK_EQ(threads.first.owner(), 1u);
  CHECK(currents.make_current(threads.second));
  CHECK_EQ(currents.anywhere(), 2u);
  CHECK(currents.invariants_hold());
  threads.slot.switch_to(1u);
  CHECK(currents.current() == &threads.first);
  CHECK(!currents.current_elsewhere(threads.first)); }

TEST(current_contexts, making_another_current_releases_the_first_and_release_clears_only_this_thread)
{ Threads threads;
  CurrentContexts &currents = threads.currents;
  CHECK(currents.make_current(threads.first));
  CHECK(currents.make_current(threads.first));
  CHECK_EQ(currents.anywhere(), 1u);
  CHECK(currents.make_current(threads.second));
  CHECK_EQ(threads.first.owner(), 0u);
  CHECK_EQ(currents.anywhere(), 1u);
  threads.slot.switch_to(2u);
  CHECK(currents.make_current(threads.first));
  currents.release();
  CHECK_EQ(threads.first.owner(), 0u);
  CHECK_EQ(currents.anywhere(), 1u);
  currents.release();
  CHECK_EQ(currents.anywhere(), 1u);
  threads.slot.switch_to(1u);
  CHECK(currents.current() == &threads.second);
  currents.release();
  CHECK_EQ(currents.anywhere(), 0u);
  CHECK(currents.invariants_hold()); }

TEST(current_contexts_invariants, fail_when_the_threads_context_names_another_thread)
{ Threads threads;
  CHECK(threads.currents.make_current(threads.first));
  threads.first.set_owner(5u);
  CHECK(!threads.currents.invariants_hold());
  threads.first.set_owner(1u);
  threads.currents.release();
  threads.slot.set(&threads.first);
  CHECK(!threads.currents.invariants_hold());
  threads.slot.set(nullptr); }
